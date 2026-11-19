#pragma once

// The part of the libsmacker API that the game uses, implemented with FFmpeg
// in smacker_ffmpeg.cpp. It stands in for libsmacker's smacker.h when
// ORION2RE_USE_FFMPEG_SMACKER is enabled, so the declarations match it.

typedef struct smk_t* smk;

#define SMK_DONE 0x00
#define SMK_MORE 0x01
#define SMK_LAST 0x02
#define SMK_ERROR -1

#define SMK_FLAG_Y_NONE 0x00
#define SMK_FLAG_Y_INTERLACE 0x01
#define SMK_FLAG_Y_DOUBLE 0x02

#define SMK_AUDIO_TRACK_0 0x01
#define SMK_AUDIO_TRACK_1 0x02
#define SMK_AUDIO_TRACK_2 0x04
#define SMK_AUDIO_TRACK_3 0x08
#define SMK_AUDIO_TRACK_4 0x10
#define SMK_AUDIO_TRACK_5 0x20
#define SMK_AUDIO_TRACK_6 0x40
#define SMK_VIDEO_TRACK 0x80

#ifdef __cplusplus
extern "C" {
#endif

/* Opens a Smacker file held in memory. The buffer is copied. */
smk smk_open_memory(const unsigned char* buffer, unsigned long size);
void smk_close(smk object);

char smk_info_all(const smk object, unsigned long* frame, unsigned long* frame_count, double* usf);
char smk_info_video(const smk object, unsigned long* w, unsigned long* h, unsigned char* y_scale_mode);
char smk_info_audio(const smk object, unsigned char* track_mask, unsigned char channels[7], unsigned char bitdepth[7], unsigned long audio_rate[7]);

char smk_enable_video(smk object, unsigned char enable);
char smk_enable_audio(smk object, unsigned char track, unsigned char enable);

/* The current frame: 256 RGB triples, width * height palette indices, and the
 * audio of an enabled track as unsigned 8-bit or little-endian 16-bit PCM. */
const unsigned char* smk_get_palette(const smk object);
const unsigned char* smk_get_video(const smk object);
const unsigned char* smk_get_audio(const smk object, unsigned char track);
unsigned long smk_get_audio_size(const smk object, unsigned char track);

char smk_first(smk object);
char smk_next(smk object);

#ifdef __cplusplus
}
#endif
