#include "data/wav.h"

#define OSC_MAX_VOICES 8

typedef struct {
    SDL_AudioStream *stream;
    const osc_wav *wav;
    bool loop;
} voice;

static SDL_AudioDeviceID g_dev;
static voice g_voices[OSC_MAX_VOICES];   /* one-shot sounds */
static voice g_music;                     /* the looping track, kept apart so it never takes a sound's slot */

bool osc_wav_load(const char *path, osc_wav *out)
{
    SDL_zerop(out);
    if (!SDL_LoadWAV(path, &out->spec, &out->buf, &out->len)) {
        SDL_Log("wav: cannot load %s: %s", path, SDL_GetError());
        return false;
    }
    return true;
}

void osc_wav_free(osc_wav *w)
{
    SDL_free(w->buf);
    SDL_zerop(w);
}

bool osc_audio_open(void)
{
    if (g_dev) return true;
#ifdef __EMSCRIPTEN__
    /* the browser pulls audio on the main thread, which the game loop also uses:
     * a bigger device buffer survives slow frames without stuttering */
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "4096");
#endif
    g_dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (!g_dev) {
        SDL_Log("audio: no output device: %s", SDL_GetError());
        return false;
    }
    return true;
}

void osc_audio_close(void)
{
    for (int i = 0; i < OSC_MAX_VOICES; i++) {
        if (g_voices[i].stream) SDL_DestroyAudioStream(g_voices[i].stream);
        g_voices[i].stream = NULL;
    }
    osc_audio_stop_music();
    if (g_dev) SDL_CloseAudioDevice(g_dev);
    g_dev = 0;
}

static bool voice_start(voice *v, const osc_wav *w, bool loop)
{
    if (v->stream) SDL_DestroyAudioStream(v->stream);
    v->stream = SDL_CreateAudioStream(&w->spec, NULL);
    if (!v->stream) return false;
    if (!SDL_BindAudioStream(g_dev, v->stream)) {
        SDL_DestroyAudioStream(v->stream);
        v->stream = NULL;
        return false;
    }
    v->wav = w;
    v->loop = loop;
    SDL_PutAudioStreamData(v->stream, w->buf, (int)w->len);
    return true;
}

bool osc_audio_play(const osc_wav *w)
{
    if (!g_dev || !w->buf) return false;
    /* a free voice, or else steal the one with the least left to play,
     * so hits and voices are never dropped (the original's DirectSound did the same) */
    voice *v = NULL;
    int least = 0;
    for (int i = 0; i < OSC_MAX_VOICES; i++) {
        int q = g_voices[i].stream ? SDL_GetAudioStreamQueued(g_voices[i].stream) : 0;
        if (!v || q < least) { v = &g_voices[i]; least = q; }
        if (q == 0) break;
    }
    return voice_start(v, w, false);
}

bool osc_audio_music(const osc_wav *w)
{
    osc_audio_stop_music();
    if (!g_dev || !w->buf) return false;
    return voice_start(&g_music, w, true);
}

void osc_audio_stop_music(void)
{
    if (g_music.stream) SDL_DestroyAudioStream(g_music.stream);
    SDL_zero(g_music);
}

void osc_audio_update(void)
{
    if (g_music.stream && SDL_GetAudioStreamQueued(g_music.stream) < (int)g_music.wav->len / 2)
        SDL_PutAudioStreamData(g_music.stream, g_music.wav->buf, (int)g_music.wav->len);
}
