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
/* Queue a sound; `loop` keeps re-queueing it (used for music). */
bool osc_audio_play(const osc_wav *w, bool loop);
void osc_audio_update(void);

#endif
