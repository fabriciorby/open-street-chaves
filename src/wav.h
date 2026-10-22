/* Minimal WAV playback through SDL3 audio streams. */
#ifndef OSC_WAV_H
#define OSC_WAV_H

#include <SDL3/SDL.h>

typedef struct {
    SDL_AudioSpec spec;
    Uint8 *buf;
    Uint32 len;
} osc_wav;

bool osc_wav_load(const char *path, osc_wav *out);
void osc_wav_free(osc_wav *w);

/* One output device shared by all sounds. */
bool osc_audio_open(void);
void osc_audio_close(void);
/* Play a one-shot sound on a free voice. */
bool osc_audio_play(const osc_wav *w);
/* Loop `w` as the music track, replacing the current one. Stop it before freeing its wav. */
bool osc_audio_music(const osc_wav *w);
void osc_audio_stop_music(void);
void osc_audio_update(void);

#endif
