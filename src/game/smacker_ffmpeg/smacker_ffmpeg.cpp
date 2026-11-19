#include "smacker.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace {
    constexpr int k_track_count = 7;
    constexpr size_t k_header_size = 104;
    constexpr int k_io_buffer_size = 32768;

    struct s_audio_track {
        bool exists = false;
        bool enabled = false;
        unsigned char channels = 0;
        unsigned char bitdepth = 0;
        unsigned long rate = 0;
        int stream = -1;
        AVCodecContext* codec = nullptr;
        std::vector<unsigned char> samples;
    };
} // namespace

struct smk_t {
    std::vector<unsigned char> file;
    size_t file_position = 0;

    // From the file header, read the way libsmacker reads it.
    unsigned long width = 0;
    unsigned long height = 0;
    unsigned long frame_count = 0;
    double microseconds_per_frame = 0.0;
    bool ring_frame = false;
    unsigned char y_scale_mode = SMK_FLAG_Y_NONE;
    s_audio_track audio[k_track_count];

    AVIOContext* io = nullptr;
    AVFormatContext* format = nullptr;
    AVCodecContext* video_codec = nullptr;
    int video_stream = -1;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;

    bool video_enabled = false;
    bool decoding_started = false;
    unsigned long current_frame = 0;
    std::vector<unsigned char> video;
    unsigned char palette[256 * 3] = {};
};

namespace {
    uint32_t Read_Le32_(const unsigned char* data) {
        return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    }

    void Parse_Header_(smk_t* s) {
        const unsigned char* header = s->file.data();
        s->width = Read_Le32_(header + 4);
        s->height = Read_Le32_(header + 8);
        s->frame_count = Read_Le32_(header + 12);

        const int32_t frame_rate = (int32_t)Read_Le32_(header + 16);
        if (frame_rate > 0) {
            s->microseconds_per_frame = frame_rate * 1000.0;
        } else if (frame_rate < 0) {
            s->microseconds_per_frame = frame_rate * -10.0;
        } else {
            s->microseconds_per_frame = 100000.0;
        }

        const uint32_t flags = Read_Le32_(header + 20);
        s->ring_frame = (flags & 0x01) != 0;
        if (flags & 0x02) {
            s->y_scale_mode = SMK_FLAG_Y_DOUBLE;
        }
        if (flags & 0x04) {
            s->y_scale_mode = SMK_FLAG_Y_INTERLACE;
        }

        for (int track = 0; track < k_track_count; ++track) {
            const uint32_t audio = Read_Le32_(header + 72 + track * 4);
            s_audio_track& info = s->audio[track];
            // FFmpeg's demuxer makes a stream of every track with a rate,
            // whether or not the track is flagged as present.
            info.rate = audio & 0x00FFFFFF;
            if (audio & 0x40000000) {
                info.exists = true;
                info.bitdepth = (audio & 0x20000000) ? 16 : 8;
                info.channels = (audio & 0x10000000) ? 2 : 1;
            }
        }
    }

    int Read_File_(void* opaque, uint8_t* buffer, int size) {
        smk_t* s = (smk_t*)opaque;
        const size_t remaining = s->file.size() - s->file_position;
        if (remaining == 0) {
            return AVERROR_EOF;
        }
        const size_t count = remaining < (size_t)size ? remaining : (size_t)size;
        memcpy(buffer, s->file.data() + s->file_position, count);
        s->file_position += count;
        return (int)count;
    }

    int64_t Seek_File_(void* opaque, int64_t offset, int whence) {
        smk_t* s = (smk_t*)opaque;
        const int64_t size = (int64_t)s->file.size();
        if (whence & AVSEEK_SIZE) {
            return size;
        }
        whence &= ~AVSEEK_FORCE;
        if (whence == SEEK_CUR) {
            offset += (int64_t)s->file_position;
        } else if (whence == SEEK_END) {
            offset += size;
        } else if (whence != SEEK_SET) {
            return AVERROR(EINVAL);
        }
        if (offset < 0 || offset > size) {
            return AVERROR(EINVAL);
        }
        s->file_position = (size_t)offset;
        return offset;
    }

    AVCodecContext* Open_Codec_(const AVCodecParameters* parameters) {
        const AVCodec* decoder = avcodec_find_decoder(parameters->codec_id);
        if (decoder == nullptr) {
            return nullptr;
        }
        AVCodecContext* codec = avcodec_alloc_context3(decoder);
        if (codec == nullptr) {
            return nullptr;
        }
        // One frame out for each packet in, with no frame-threading delay.
        codec->thread_count = 1;
        if (avcodec_parameters_to_context(codec, parameters) < 0 || avcodec_open2(codec, decoder, nullptr) < 0) {
            avcodec_free_context(&codec);
        }
        return codec;
    }

    void Close_Decoder_(smk_t* s) {
        av_frame_free(&s->frame);
        av_packet_free(&s->packet);
        avcodec_free_context(&s->video_codec);
        for (s_audio_track& track : s->audio) {
            avcodec_free_context(&track.codec);
            track.stream = -1;
        }
        avformat_close_input(&s->format);
        if (s->io != nullptr) {
            av_freep(&s->io->buffer);
            avio_context_free(&s->io);
        }
        s->video_stream = -1;
    }

    bool Open_Decoder_(smk_t* s) {
        s->file_position = 0;
        s->decoding_started = false;

        unsigned char* io_buffer = (unsigned char*)av_malloc(k_io_buffer_size);
        if (io_buffer == nullptr) {
            return false;
        }
        s->io = avio_alloc_context(io_buffer, k_io_buffer_size, 0, s, Read_File_, nullptr, Seek_File_);
        if (s->io == nullptr) {
            av_free(io_buffer);
            return false;
        }

        s->format = avformat_alloc_context();
        if (s->format == nullptr) {
            return false;
        }
        s->format->pb = s->io;
        // avformat_open_input frees the context when it fails.
        if (avformat_open_input(&s->format, nullptr, av_find_input_format("smk"), nullptr) < 0) {
            return false;
        }

        int track = 0;
        for (unsigned int index = 0; index < s->format->nb_streams; ++index) {
            const AVCodecParameters* parameters = s->format->streams[index]->codecpar;
            if (parameters->codec_type == AVMEDIA_TYPE_VIDEO) {
                if (s->video_stream >= 0) {
                    return false;
                }
                s->video_stream = (int)index;
                continue;
            }
            if (parameters->codec_type != AVMEDIA_TYPE_AUDIO) {
                continue;
            }
            // The audio streams are in track order.
            while (track < k_track_count && s->audio[track].rate == 0) {
                ++track;
            }
            if (track == k_track_count) {
                return false;
            }
            s->audio[track].stream = (int)index;
            if (s->audio[track].exists) {
                // A track without a decoder plays as silence.
                s->audio[track].codec = Open_Codec_(parameters);
            }
            ++track;
        }

        if (s->video_stream < 0) {
            return false;
        }
        const AVCodecParameters* video = s->format->streams[s->video_stream]->codecpar;
        if (video->width <= 0 || video->height <= 0 ||
            (unsigned long)video->width != s->width || (unsigned long)video->height != s->height) {
            return false;
        }
        s->video_codec = Open_Codec_(video);
        s->packet = av_packet_alloc();
        s->frame = av_frame_alloc();
        return s->video_codec != nullptr && s->packet != nullptr && s->frame != nullptr;
    }

    // A frame that does not decode leaves the previous picture in place, so
    // that playback and its audio carry on. The original game data has such
    // frames, which libsmacker draws as far as their data goes.
    void Decode_Video_(smk_t* s) {
        if (avcodec_send_packet(s->video_codec, s->packet) < 0 || avcodec_receive_frame(s->video_codec, s->frame) < 0) {
            return;
        }
        const AVFrame* frame = s->frame;
        if (frame->format == AV_PIX_FMT_PAL8 &&
            (unsigned long)frame->width == s->width && (unsigned long)frame->height == s->height) {
            for (unsigned long y = 0; y < s->height; ++y) {
                memcpy(s->video.data() + y * s->width, frame->data[0] + y * frame->linesize[0], s->width);
            }
            // PAL8 palette entries are native-endian 0xAARRGGBB.
            for (int index = 0; index < 256; ++index) {
                uint32_t color;
                memcpy(&color, frame->data[1] + index * 4, sizeof(color));
                s->palette[index * 3] = (unsigned char)(color >> 16);
                s->palette[index * 3 + 1] = (unsigned char)(color >> 8);
                s->palette[index * 3 + 2] = (unsigned char)color;
            }
        }
        av_frame_unref(s->frame);
    }

    void Decode_Audio_(smk_t* s, s_audio_track& track) {
        if (avcodec_send_packet(track.codec, s->packet) < 0) {
            return;
        }
        while (avcodec_receive_frame(track.codec, s->frame) == 0) {
            const AVFrame* frame = s->frame;
            const AVSampleFormat format = track.bitdepth == 16 ? AV_SAMPLE_FMT_S16 : AV_SAMPLE_FMT_U8;
            const size_t size = (size_t)frame->nb_samples * track.channels * (track.bitdepth / 8);
            if (frame->format == format && frame->nb_samples > 0 && size <= (size_t)frame->linesize[0]) {
                const size_t offset = track.samples.size();
                track.samples.resize(offset + size);
                memcpy(track.samples.data() + offset, frame->data[0], size);
                // The decoder's 16-bit samples are native-endian.
                if (track.bitdepth == 16 && std::endian::native == std::endian::big) {
                    for (size_t index = offset; index + 1 < offset + size; index += 2) {
                        const unsigned char high = track.samples[index];
                        track.samples[index] = track.samples[index + 1];
                        track.samples[index + 1] = high;
                    }
                }
            }
            av_frame_unref(s->frame);
        }
    }

    // The demuxer returns the audio packets of a frame, then its video packet.
    bool Decode_Frame_(smk_t* s) {
        s->decoding_started = true;
        for (s_audio_track& track : s->audio) {
            track.samples.clear();
        }
        if (s->format == nullptr || s->video_codec == nullptr || s->packet == nullptr || s->frame == nullptr) {
            return false;
        }
        while (av_read_frame(s->format, s->packet) >= 0) {
            const int stream = s->packet->stream_index;
            if (stream == s->video_stream) {
                Decode_Video_(s);
                av_packet_unref(s->packet);
                return true;
            }
            for (s_audio_track& track : s->audio) {
                if (track.stream == stream && track.enabled && track.codec != nullptr) {
                    Decode_Audio_(s, track);
                }
            }
            av_packet_unref(s->packet);
        }
        return false;
    }

    bool Rewind_(smk_t* s) {
        if (!s->decoding_started) {
            return true;
        }
        Close_Decoder_(s);
        if (!Open_Decoder_(s)) {
            Close_Decoder_(s);
            return false;
        }
        return true;
    }
} // namespace

extern "C" {

smk smk_open_memory(const unsigned char* buffer, unsigned long size) {
    if (buffer == nullptr || size < k_header_size) {
        return nullptr;
    }
    smk_t* s = new (std::nothrow) smk_t;
    if (s == nullptr) {
        return nullptr;
    }
    s->file.assign(buffer, buffer + size);
    Parse_Header_(s);
    // Bound the frame buffer before trusting the header's dimensions.
    if (s->width == 0 || s->height == 0 || s->width > 0xFFFF || s->height > 0xFFFF || !Open_Decoder_(s)) {
        smk_close(s);
        return nullptr;
    }
    s->video.assign(s->width * s->height, 0);
    return s;
}

void smk_close(smk object) {
    if (object != nullptr) {
        Close_Decoder_(object);
        delete object;
    }
}

char smk_info_all(const smk object, unsigned long* frame, unsigned long* frame_count, double* usf) {
    if (object == nullptr || (frame == nullptr && frame_count == nullptr && usf == nullptr)) {
        return -1;
    }
    if (frame != nullptr) {
        *frame = object->current_frame;
    }
    if (frame_count != nullptr) {
        *frame_count = object->frame_count;
    }
    if (usf != nullptr) {
        *usf = object->microseconds_per_frame;
    }
    return 0;
}

char smk_info_video(const smk object, unsigned long* w, unsigned long* h, unsigned char* y_scale_mode) {
    if (object == nullptr || (w == nullptr && h == nullptr && y_scale_mode == nullptr)) {
        return -1;
    }
    if (w != nullptr) {
        *w = object->width;
    }
    if (h != nullptr) {
        *h = object->height;
    }
    if (y_scale_mode != nullptr) {
        *y_scale_mode = object->y_scale_mode;
    }
    return 0;
}

char smk_info_audio(const smk object, unsigned char* track_mask, unsigned char channels[7], unsigned char bitdepth[7], unsigned long audio_rate[7]) {
    if (object == nullptr || (track_mask == nullptr && channels == nullptr && bitdepth == nullptr && audio_rate == nullptr)) {
        return -1;
    }
    if (track_mask != nullptr) {
        *track_mask = 0;
    }
    for (int track = 0; track < k_track_count; ++track) {
        const s_audio_track& info = object->audio[track];
        if (track_mask != nullptr && info.exists) {
            *track_mask |= (unsigned char)(1u << track);
        }
        if (channels != nullptr) {
            channels[track] = info.channels;
        }
        if (bitdepth != nullptr) {
            bitdepth[track] = info.bitdepth;
        }
        if (audio_rate != nullptr) {
            audio_rate[track] = info.exists ? info.rate : 0;
        }
    }
    return 0;
}

char smk_enable_video(smk object, unsigned char enable) {
    if (object == nullptr) {
        return -1;
    }
    // Frames are always decoded, as each one builds on the frame before it.
    object->video_enabled = enable != 0;
    return 0;
}

char smk_enable_audio(smk object, unsigned char track, unsigned char enable) {
    if (object == nullptr || track >= k_track_count) {
        return -1;
    }
    object->audio[track].enabled = enable != 0;
    return 0;
}

const unsigned char* smk_get_palette(const smk object) {
    return object != nullptr ? object->palette : nullptr;
}

const unsigned char* smk_get_video(const smk object) {
    return object != nullptr ? object->video.data() : nullptr;
}

const unsigned char* smk_get_audio(const smk object, unsigned char track) {
    if (object == nullptr || track >= k_track_count) {
        return nullptr;
    }
    return object->audio[track].samples.data();
}

unsigned long smk_get_audio_size(const smk object, unsigned char track) {
    if (object == nullptr || track >= k_track_count) {
        return 0;
    }
    return (unsigned long)object->audio[track].samples.size();
}

char smk_first(smk object) {
    if (object == nullptr || !Rewind_(object)) {
        return SMK_ERROR;
    }
    object->current_frame = 0;
    if (!Decode_Frame_(object)) {
        return SMK_ERROR;
    }
    return object->frame_count == 1 ? SMK_LAST : SMK_MORE;
}

char smk_next(smk object) {
    if (object == nullptr) {
        return SMK_ERROR;
    }
    const unsigned long total_frames = object->frame_count + (object->ring_frame ? 1 : 0);
    if (object->current_frame + 1 < total_frames) {
        ++object->current_frame;
        if (!Decode_Frame_(object)) {
            return SMK_ERROR;
        }
    } else if (object->ring_frame) {
        // The ring frame leads back to frame 1.
        if (!Rewind_(object) || !Decode_Frame_(object) || !Decode_Frame_(object)) {
            return SMK_ERROR;
        }
        object->current_frame = 1;
    } else {
        return SMK_DONE;
    }
    return object->current_frame + 1 == total_frames ? SMK_LAST : SMK_MORE;
}

} // extern "C"
