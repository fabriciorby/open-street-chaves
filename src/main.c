/* open-street-chaves — entry point.
 *
 * Fixed 28 ms tick (the original DXTimer interval), 400x300 logical screen
 * letterboxed into the window. SDL3 callback main so the same code runs in
 * the browser (Emscripten) and on desktop.
 *
 *   open-street-chaves --data <folder with Chaves.exe, Lutadores/, Cenarios/...>
 *                      [--p1 N] [--p2 N] [--frames N] [--screenshot f.bmp]
 *                      [--demo] [--boxes]
 *
 * Keys: P1 arrows + Z X C V, P2 W A S D + F G H J (weak punch, weak kick,
 * strong punch, strong kick), P pause, B toggle hit boxes, Esc quit.
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
    long max_frames;
    long frames;
    const char *screenshot;
    bool demo; /* scripted P2 for headless tests */
} app;

static const Uint64 TICK_NS = (Uint64)OSC_TICK_MS * 1000000ull;

static uint8_t read_input(const bool *k, const SDL_Scancode *map)
{
    uint8_t in = 0;
    for (int i = 0; i < 8; i++)
        if (k[map[i]]) in |= (uint8_t)(1 << i);
    return in;
}

static const SDL_Scancode k_p1[8] = {SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
                                     SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V};
static const SDL_Scancode k_p2[8] = {SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D,
                                     SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J};

SDL_AppResult SDL_AppInit(void **state, int argc, char **argv)
{
    const char *data = "StreetChaves";
    long max_frames = 0;
    const char *screenshot = NULL;
    int c1 = 1, c2 = 2;
    bool demo = false, boxes = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) data = argv[++i];
        else if (strncmp(argv[i], "--data=", 7) == 0) data = argv[i] + 7;
        else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) max_frames = SDL_strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) screenshot = argv[++i];
        else if (strcmp(argv[i], "--p1") == 0 && i + 1 < argc) c1 = (int)SDL_strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--p2") == 0 && i + 1 < argc) c2 = (int)SDL_strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--demo") == 0) demo = true;
        else if (strcmp(argv[i], "--boxes") == 0) boxes = true;
        else if (strcmp(argv[i], "--help") == 0) {
            SDL_Log("usage: open-street-chaves --data <Street Chaves folder> [--p1 N] [--p2 N] [--frames N] [--screenshot f.bmp] [--demo] [--boxes]");
            return SDL_APP_SUCCESS;
        }
    }
    if (c1 < 1 || c1 > 15) c1 = 1;
    if (c2 < 1 || c2 > 15) c2 = 2;

    SDL_SetAppMetadata("open-street-chaves", "0.0.2", "org.openstreetchaves.game");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    app *a = SDL_calloc(1, sizeof *a);
    *state = a;
    a->max_frames = max_frames;
    a->screenshot = screenshot;
    a->demo = demo;
    if (!SDL_CreateWindowAndRenderer("open-street-chaves", OSC_SCREEN_W * 2, OSC_SCREEN_H * 2,
                                     SDL_WINDOW_RESIZABLE, &a->win, &a->ren)) {
        SDL_Log("window/renderer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(a->ren, OSC_SCREEN_W, OSC_SCREEN_H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(a->ren, 1);

    a->loaded = osc_game_init(&a->game, a->ren, data, c1, c2);
    a->game.debug_boxes = boxes;
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
        if (e->key.key == SDLK_B && !e->key.repeat) a->game.debug_boxes = !a->game.debug_boxes;
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

/* Scripted opponent for --demo: walk towards P1, punch every 25 ticks, block sometimes. */
static uint8_t demo_input(const osc_game *g)
{
    const osc_fighter *p2 = &g->p[2], *p1 = &g->p[1];
    uint8_t in = 0;
    double dist = p2->x - p1->x;
    if (dist > 70) in |= OSC_IN_LEFT;
    if (dist < -70) in |= OSC_IN_RIGHT;
    if (g->tick % 25 == 0 && dist >= -70 && dist <= 70) in |= (g->tick % 50 == 0) ? OSC_IN_B3 : OSC_IN_B1;
    if (p1->attacking && g->tick % 3 == 0) in |= (dist > 0) ? OSC_IN_RIGHT : OSC_IN_LEFT; /* step back = block */
    return in;
}

SDL_AppResult SDL_AppIterate(void *state)
{
    app *a = state;
    Uint64 now = SDL_GetTicksNS();
    a->acc_ns += now - a->last_ns;
    a->last_ns = now;
    if (a->acc_ns > TICK_NS * 5) a->acc_ns = TICK_NS * 5;
    while (a->acc_ns >= TICK_NS) {
        if (a->loaded) {
            uint8_t in1 = read_input(a->keys, k_p1);
            uint8_t in2 = a->demo ? demo_input(&a->game) : read_input(a->keys, k_p2);
            osc_game_tick(&a->game, in1, in2);
        }
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
        SDL_Surface *shot = SDL_RenderReadPixels(a->ren, NULL);
        if (shot) {
            SDL_SaveBMP(shot, a->screenshot);
            SDL_DestroySurface(shot);
        }
    }
    SDL_RenderPresent(a->ren);
    if (a->max_frames > 0 && a->frames >= a->max_frames) {
        const osc_game *g = &a->game;
        SDL_Log("frames=%ld ticks=%u loaded=%d p1(x=%.0f st=%d fr=%d hp=%.1f) p2(x=%.0f st=%d fr=%d hp=%.1f) cam=%d,%d",
                a->frames, g->tick, a->loaded, g->p[1].x, g->p[1].state, g->p[1].frame, g->p[1].health,
                g->p[2].x, g->p[2].state, g->p[2].frame, g->p[2].health, g->cam_x, g->cam_y);
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
