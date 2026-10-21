/* open-street-chaves — entry point.
 *
 * Fixed 28 ms tick (the original DXTimer interval), 400x300 logical screen
 * letterboxed into the window. SDL3 callback main so the same code runs in
 * the browser (Emscripten) and on desktop.
 *
 *   open-street-chaves --data <folder with Chaves.exe, Lutadores/, Cenarios/...>
 */
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <string.h>

#include "game.h"

typedef struct {
    SDL_Window *win;
    SDL_Renderer *ren;
    osc_game game;
    Uint64 last_ns;
    Uint64 acc_ns;
    bool keys[SDL_SCANCODE_COUNT];
    bool loaded;
    long max_frames;   /* --frames N: quit after N presented frames (smoke tests) */
    long frames;
    const char *screenshot; /* --screenshot file.bmp: save the last frame before quitting */
} app;

static const Uint64 TICK_NS = (Uint64)OSC_TICK_MS * 1000000ull;

SDL_AppResult SDL_AppInit(void **state, int argc, char **argv)
{
    const char *data = "StreetChaves";
    long max_frames = 0;
    const char *screenshot = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) data = argv[++i];
        else if (strncmp(argv[i], "--data=", 7) == 0) data = argv[i] + 7;
        else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) max_frames = SDL_strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) screenshot = argv[++i];
        else if (strcmp(argv[i], "--help") == 0) {
            SDL_Log("usage: open-street-chaves --data <path to Street Chaves 1.5A folder> [--frames N]");
            return SDL_APP_SUCCESS;
        }
    }

    SDL_SetAppMetadata("open-street-chaves", "0.0.1", "org.openstreetchaves.game");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    app *a = SDL_calloc(1, sizeof *a);
    *state = a;
    a->max_frames = max_frames;
    a->screenshot = screenshot;
    if (!SDL_CreateWindowAndRenderer("open-street-chaves", OSC_SCREEN_W * 2, OSC_SCREEN_H * 2,
                                     SDL_WINDOW_RESIZABLE, &a->win, &a->ren)) {
        SDL_Log("window/renderer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(a->ren, OSC_SCREEN_W, OSC_SCREEN_H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(a->ren, 1);

    a->loaded = osc_game_init(&a->game, a->ren, data);
    if (!a->loaded)
        SDL_Log("Could not load the game data from '%s'. Pass --data <folder containing Chaves.exe>.", data);
    a->last_ns = SDL_GetTicksNS();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *state, SDL_Event *e)
{
    app *a = state;
    switch (e->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
        if (e->key.key == SDLK_ESCAPE) return SDL_APP_SUCCESS;
        if (e->key.key == SDLK_P && !e->key.repeat) a->game.paused = !a->game.paused;
        if (e->key.scancode < SDL_SCANCODE_COUNT) a->keys[e->key.scancode] = true;
        break;
    case SDL_EVENT_KEY_UP:
        if (e->key.scancode < SDL_SCANCODE_COUNT) a->keys[e->key.scancode] = false;
        break;
    default:
        break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *state)
{
    app *a = state;
    Uint64 now = SDL_GetTicksNS();
    a->acc_ns += now - a->last_ns;
    a->last_ns = now;
    /* Never spiral: at most 5 ticks per frame. */
    if (a->acc_ns > TICK_NS * 5) a->acc_ns = TICK_NS * 5;
    while (a->acc_ns >= TICK_NS) {
        if (a->loaded) osc_game_tick(&a->game, a->keys);
        a->acc_ns -= TICK_NS;
    }
    if (a->loaded) {
        osc_game_render(&a->game);
    } else {
        SDL_SetRenderDrawColor(a->ren, 40, 0, 0, 255);
        SDL_RenderClear(a->ren);
        SDL_SetRenderDrawColor(a->ren, 255, 255, 255, 255);
        SDL_RenderDebugText(a->ren, 20, 140, "Game data not found. Run with --data <Street Chaves folder>");
    }
    a->frames++;
    if (a->max_frames > 0 && a->frames >= a->max_frames && a->screenshot) {
        /* Grab the whole output (window-sized, letterboxed) before presenting it. */
        SDL_Surface *shot = SDL_RenderReadPixels(a->ren, NULL);
        if (shot) {
            SDL_SaveBMP(shot, a->screenshot);
            SDL_DestroySurface(shot);
        }
    }
    SDL_RenderPresent(a->ren);
    if (a->max_frames > 0 && a->frames >= a->max_frames) {
        SDL_Log("frames=%ld ticks=%u loaded=%d p1.x=%.1f frame=%d", a->frames, a->game.tick, a->loaded,
                a->game.p1.x, a->game.p1.frame);
        return a->loaded ? SDL_APP_SUCCESS : SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *state, SDL_AppResult result)
{
    (void)result;
    app *a = state;
    if (!a) return;
    if (a->loaded) osc_game_shutdown(&a->game);
    if (a->ren) SDL_DestroyRenderer(a->ren);
    if (a->win) SDL_DestroyWindow(a->win);
    SDL_free(a);
}
