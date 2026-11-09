#include "pch.h"

namespace {
    unsigned long current_frame;
    unsigned char pixels[1];
    unsigned char colors[256 * 3];
}

namespace test::fakes {
    int playback_stream_open_count;
    int32_t last_playback_stream_volume;

    void Reset_Smacker_Audio_Fake_() {
        playback_stream_open_count = 0;
        last_playback_stream_volume = -1;
        current_frame = 0;
        memset(colors, 0, sizeof(colors));
        colors[10 * 3] = 120;
        colors[10 * 3 + 1] = 80;
        colors[10 * 3 + 2] = 40;
        memset(&MOX::settings, 0, sizeof(MOX::settings));
    }
}

namespace MOX {
    s_settings settings;
}

namespace SMACK {
    int16_t smack_x;
    int16_t smack_y;
}

namespace vfs {
    s_vfs_file* fopen(const char*, const char*) {
        return reinterpret_cast<s_vfs_file*>(uintptr_t{1});
    }

    size_t fread(void* ptr, size_t size, size_t count, s_vfs_file*) {
        memset(ptr, 0, size * count);
        return count;
    }

    int32_t fsize(s_vfs_file*) {
        return 16;
    }

    int32_t fclose(s_vfs_file*) {
        return 0;
    }
}

namespace sound {
    SDL_AudioStream* __cdecl Open_Playback_Stream(uint16_t, uint16_t, uint32_t, int32_t volume_percent) {
        ++test::fakes::playback_stream_open_count;
        test::fakes::last_playback_stream_volume = volume_percent;
        return nullptr;
    }

    void __cdecl Close_Playback_Stream(SDL_AudioStream*& stream) {
        stream = nullptr;
    }
}

namespace palette {
    void __cdecl Save_Palette() {
    }

    void __cdecl Restore_Palette() {
    }

    void __cdecl Refresh_Palette() {
    }
}

extern "C" {
    smk smk_open_memory(const unsigned char*, unsigned long) {
        return reinterpret_cast<smk>(uintptr_t{1});
    }

    void smk_close(smk) {
    }

    char smk_info_all(const smk, unsigned long* frame, unsigned long* frame_count, double* usf) {
        *frame = current_frame;
        *frame_count = 2;
        *usf = 1000.0;
        return 0;
    }

    char smk_info_video(const smk, unsigned long* width, unsigned long* height, unsigned char*) {
        *width = 1;
        *height = 1;
        return 0;
    }

    char smk_info_audio(const smk, unsigned char* track_mask, unsigned char channels[7],
                        unsigned char bitdepth[7], unsigned long audio_rate[7]) {
        memset(channels, 0, 7 * sizeof(channels[0]));
        memset(bitdepth, 0, 7 * sizeof(bitdepth[0]));
        memset(audio_rate, 0, 7 * sizeof(audio_rate[0]));
        *track_mask = SMK_AUDIO_TRACK_0;
        channels[0] = 1;
        bitdepth[0] = 16;
        audio_rate[0] = 22050;
        return 0;
    }

    char smk_enable_video(smk, unsigned char) {
        return 0;
    }

    char smk_enable_audio(smk, unsigned char, unsigned char) {
        return 0;
    }

    const unsigned char* smk_get_audio(const smk, unsigned char) {
        return nullptr;
    }

    unsigned long smk_get_audio_size(const smk, unsigned char) {
        return 0;
    }

    char smk_first(smk) {
        current_frame = 0;
        return SMK_MORE;
    }

    char smk_next(smk) {
        current_frame = 1;
        return SMK_LAST;
    }

    const unsigned char* smk_get_video(const smk) {
        pixels[0] = static_cast<unsigned char>(40 + current_frame);
        return pixels;
    }

    const unsigned char* smk_get_palette(const smk) {
        return colors;
    }
}