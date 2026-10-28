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
 * strong punch, strong kick), P pause, G move list, - = speed, , . shift picture, B toggle hit boxes, Esc quit.
 */
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <string.h>

#include "fight.h"
#include "game.h"
#include "arcade.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
static int g_web_ready;
EMSCRIPTEN_KEEPALIVE void osc_web_data_ready(void) { g_web_ready = 1; }
#endif

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
    SDL_Gamepad *pads[2];
    int n_press; long press_tick[16]; SDL_Scancode press_key[16];   /* --press T:key test input */
    long ticks_run;
    bool demo;     /* scripted P2 for headless tests */
    bool autofire; /* scripted P1 fireballs for headless tests */
} app;


static uint8_t read_input(const bool *k, const SDL_Scancode *map)
{
    uint8_t in = 0;
    for (int i = 0; i < 8; i++)
        if (k[map[i]]) in |= (uint8_t)(1 << i);
    return in;
}

/* Gamepad -> DelphiX bits: d-pad/left stick, A B X Y = weak punch, weak kick, strong punch, strong kick. */
static uint8_t pad_input(SDL_Gamepad *gp)
{
    if (!gp) return 0;
    uint8_t in = 0;
    Sint16 ax = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTX), ay = SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTY);
    if (SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_UP) || ay < -16000) in |= OSC_IN_UP;
    if (SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_DOWN) || ay > 16000) in |= OSC_IN_DOWN;
    if (SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_LEFT) || ax < -16000) in |= OSC_IN_LEFT;
    if (SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || ax > 16000) in |= OSC_IN_RIGHT;
    if (SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_WEST)) in |= OSC_IN_B1;
    if (SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_SOUTH)) in |= OSC_IN_B2;
    if (SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_NORTH)) in |= OSC_IN_B3;
    if (SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_EAST)) in |= OSC_IN_B4;
    return in;
}


SDL_AppResult SDL_AppInit(void **state, int argc, char **argv)
{
    const char *data = "StreetChaves";
    long max_frames = 0;
    const char *screenshot = NULL;
    int c1 = 1, c2 = 2;
    int battle = 1;   /* --battle N: fight N of the ladder (8 has the dialogue) */
    bool demo = false, boxes = false, two_players = false, autofire = false, direct = false;
    int lang = 0, stage = 0, start_tick = 0, start_screen = 0;
    int n_press = 0; long press_tick[16]; SDL_Scancode press_key[16];
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--data") == 0 && i + 1 < argc) data = argv[++i];
        else if (strncmp(argv[i], "--data=", 7) == 0) data = argv[i] + 7;
        else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) max_frames = SDL_strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) screenshot = argv[++i];
        else if (strcmp(argv[i], "--p1") == 0 && i + 1 < argc) c1 = (int)SDL_strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--p2") == 0 && i + 1 < argc) c2 = (int)SDL_strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--demo") == 0) demo = true;
        else if (strcmp(argv[i], "--boxes") == 0) boxes = true;
        else if (strcmp(argv[i], "--2p") == 0) two_players = true;
        else if (strcmp(argv[i], "--autofire") == 0) autofire = true;
        else if (strcmp(argv[i], "--fight") == 0) direct = true;
        else if (strcmp(argv[i], "--stage") == 0 && i + 1 < argc) stage = (int)SDL_strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--es") == 0) lang = 1;
        else if (strcmp(argv[i], "--tick") == 0 && i + 1 < argc) start_tick = (int)SDL_strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--battle") == 0 && i + 1 < argc) battle = (int)SDL_strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--ending") == 0 && i + 1 < argc) { start_screen = OSC_SCREEN_ENDING; c1 = (int)SDL_strtol(argv[++i], NULL, 10); }   /* --ending N [--tick T]: character N's ending from tick T */
        else if (strcmp(argv[i], "--screen") == 0 && i + 1 < argc) start_screen = (int)SDL_strtol(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--press") == 0 && i + 1 < argc && n_press < 16) {   /* --press 60:5 (tick:key name) */
            const char *spec = argv[++i], *colon = strchr(spec, ':');
            if (colon) { press_tick[n_press] = SDL_strtol(spec, NULL, 10); press_key[n_press] = SDL_GetScancodeFromName(colon + 1); n_press++; }
        }
        else if (strcmp(argv[i], "--help") == 0) {
            SDL_Log("usage: open-street-chaves --data <Street Chaves folder> [--2p] [--es] [--fight --p1 N --p2 N --stage N --battle N] [--tick N] [--screen N] [--ending N] [--frames N --screenshot f.bmp] [--demo] [--autofire] [--boxes]");
            return SDL_APP_SUCCESS;
        }
    }
    if (c1 < 1 || c1 > 15) c1 = 1;
    if (c2 < 1 || c2 > 15) c2 = 2;

    SDL_SetAppMetadata("open-street-chaves", "0.0.2", "org.openstreetchaves.game");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    app *a = SDL_calloc(1, sizeof *a);
    *state = a;
    a->max_frames = max_frames;
    a->n_press = n_press; memcpy(a->press_tick, press_tick, sizeof press_tick); memcpy(a->press_key, press_key, sizeof press_key);
    a->screenshot = screenshot;
    a->demo = demo;
    a->autofire = autofire;
    if (!SDL_CreateWindowAndRenderer("open-street-chaves", OSC_SCREEN_W * 2, OSC_SCREEN_H * 2,
                                     SDL_WINDOW_RESIZABLE, &a->win, &a->ren)) {
        SDL_Log("window/renderer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(a->ren, OSC_SCREEN_W, OSC_SCREEN_H, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(a->ren, 1);

    a->game.ai_p2 = !two_players && !demo;   /* P2 is the CPU unless --2p (or --demo) */
#ifdef __EMSCRIPTEN__
    data = "/data";   /* filled by the page's folder picker (web/shell.html) */
    a->loaded = false;
    (void)g_web_ready;
#else
    a->loaded = osc_game_init(&a->game, a->ren, data);
#endif
    if (lang) a->game.lang = lang;
    if (max_frames > 0) a->game.rng = 0x5EED1234u;   /* --frames runs are reproducible (tests/golden.sh): fixed RNG seed ... */
    if (a->loaded && (direct || demo)) {
        a->game.p1_in = true; a->game.p2_in = !a->game.ai_p2;
        a->game.fight_no = battle;
        osc_make_ladder(&a->game, c1);
        a->game.sel[1] = c1; a->game.sel[2] = c2;
        a->game.p[1].human = true; a->game.p[2].human = !a->game.ai_p2;
        a->game.p[1].char_id = c1; a->game.p[2].char_id = c2;
        a->loaded = osc_fight_start(&a->game, c1, c2, stage);
    }
    if (a->loaded && !(direct || demo) && start_tick > 0) osc_go_attract(&a->game, start_tick);
    if (a->loaded && !(direct || demo) && (start_screen == OSC_SCREEN_MENU_F1 || start_screen == OSC_SCREEN_MENU_F2)) osc_menu_enter(&a->game, start_screen);
    if (a->loaded && !(direct || demo) && start_screen == OSC_SCREEN_ENDING) { osc_ending_start(&a->game, c1); osc_ending_seek(&a->game, start_tick); }
    if (a->loaded && !(direct || demo) && start_screen == OSC_SCREEN_CREDITS) { a->game.screen = OSC_SCREEN_CREDITS; a->game.screen_tick = start_tick; }
    if (a->loaded && !(direct || demo) && start_screen == OSC_SCREEN_RECORDS) { a->game.screen = OSC_SCREEN_RECORDS; a->game.screen_tick = start_tick; }
    if (a->loaded && !(direct || demo) && start_screen == OSC_SCREEN_NAME) {   /* debug: P1 enters a name */
        osc_game *g = &a->game;
        g->p[1].human = true; g->p[1].char_id = c1; g->p[1].score = 5000;
        memcpy(g->name[1], "AAA", 4); g->name_pos[1] = 1; g->name_active[1] = true;
        g->name_after = OSC_SCREEN_CONTINUE; g->screen = OSC_SCREEN_NAME;
    }
    if (a->loaded && !(direct || demo) && start_screen == OSC_SCREEN_CONTINUE) {   /* debug: the continue countdown */
        osc_game *g = &a->game;
        g->screen = OSC_SCREEN_CONTINUE; g->screen_tick = 0; g->ciclo = 0; g->timer_sec = 9; g->timer_sub = 25;
    }
    a->game.debug_boxes = boxes;
#ifndef __EMSCRIPTEN__
    if (!a->loaded)
        SDL_Log("Could not load the game data from '%s'. Pass --data <folder containing Chaves.exe>.", data);
#endif
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
        if (e->key.scancode == SDL_SCANCODE_F1) a->game.key_f1 = true;
        if (e->key.scancode == SDL_SCANCODE_F2) a->game.key_f2 = true;
        if (a->loaded) osc_menu_key(&a->game, e->key.scancode);   /* F2 menu: repeats count, as WM_KEYDOWN */
        if (e->key.scancode == SDL_SCANCODE_5 || e->key.scancode == SDL_SCANCODE_KP_5) a->game.key_coin = true;
        if (e->key.scancode == SDL_SCANCODE_1 || e->key.scancode == SDL_SCANCODE_KP_1) a->game.key_start1 = true;
        if (e->key.scancode == SDL_SCANCODE_2 || e->key.scancode == SDL_SCANCODE_KP_2) a->game.key_start2 = true;
        if (e->key.key == SDLK_ESCAPE) return SDL_APP_SUCCESS;
        if (a->loaded && !(e->key.repeat && (e->key.key == SDLK_P || e->key.key == SDLK_G))) osc_game_key(&a->game, e->key.key);   /* - = , . repeat like WM_KEYDOWN */
        if (e->key.key == SDLK_B && !e->key.repeat) a->game.debug_boxes = !a->game.debug_boxes;
        if (e->key.scancode < SDL_SCANCODE_COUNT) a->keys[e->key.scancode] = true;
        break;
    case SDL_EVENT_KEY_UP:
        if (e->key.scancode < SDL_SCANCODE_COUNT) a->keys[e->key.scancode] = false;
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
        /* first pad -> P2 (the original's joystick 0), second pad -> P1 */
        for (int i = 1; i >= 0; i--)
            if (!a->pads[i]) { a->pads[i] = SDL_OpenGamepad(e->gdevice.which); break; }
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        for (int i = 0; i < 2; i++)
            if (a->pads[i] && SDL_GetGamepadID(a->pads[i]) == e->gdevice.which) { SDL_CloseGamepad(a->pads[i]); a->pads[i] = NULL; }
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

/* --autofire: P1 performs down, down-forward, forward + punch every 60 ticks (QCF+P). */
static uint8_t autofire_input(const osc_game *g)
{
    if (g->screen != OSC_SCREEN_FIGHT) return (g->screen_tick % 30) == 20 ? OSC_IN_B1 : 0; /* confirm menus */
    int t = (int)(g->tick % 60);
    const osc_fighter *p1 = &g->p[1];
    uint8_t fwd = p1->facing == 1 ? OSC_IN_RIGHT : OSC_IN_LEFT;
    if (t < 3) return OSC_IN_DOWN;
    if (t < 6) return OSC_IN_DOWN | fwd;
    if (t < 8) return fwd;
    if (t == 8) return fwd | OSC_IN_B1;
    return 0;
}

SDL_AppResult SDL_AppIterate(void *state)
{
    app *a = state;
#ifdef __EMSCRIPTEN__
    if (!a->loaded && g_web_ready) {
        g_web_ready = 0;
        a->loaded = osc_game_init(&a->game, a->ren, "/data");
        a->game.debug_boxes = false;
        a->last_ns = SDL_GetTicksNS();
    }
#endif
    /* DXTimer1.Interval = g_speedDelay ms; 0 runs one tick per frame */
    const Uint64 TICK_NS = (Uint64)(a->game.speed_delay > 0 ? a->game.speed_delay : 1) * 1000000ull;
    Uint64 now = SDL_GetTicksNS();
    a->acc_ns += now - a->last_ns;
    a->last_ns = now;
    if (a->max_frames > 0) a->acc_ns = TICK_NS;   /* ... and exactly one tick per frame, independent of the wall clock */
#ifdef __EMSCRIPTEN__
    if (a->acc_ns > TICK_NS * 2) a->acc_ns = TICK_NS * 2;   /* never let a slow frame snowball on the main thread */
#else
    if (a->acc_ns > TICK_NS * 5) a->acc_ns = TICK_NS * 5;
#endif
    while (a->acc_ns >= TICK_NS) {
        if (a->loaded) {
            uint8_t in1 = a->autofire ? autofire_input(&a->game) : (uint8_t)(read_input(a->keys, a->game.keymap[0]) | pad_input(a->pads[0]));
            uint8_t in2 = a->demo ? demo_input(&a->game) : (uint8_t)(read_input(a->keys, a->game.keymap[1]) | pad_input(a->pads[1]));
            for (int i = 0; i < a->n_press; i++) {   /* scripted key taps for headless tests */
                if (a->press_tick[i] == a->ticks_run) { SDL_Event ev = {0}; ev.type = SDL_EVENT_KEY_DOWN; ev.key.scancode = a->press_key[i]; ev.key.key = SDL_GetKeyFromScancode(a->press_key[i], SDL_KMOD_NONE, false); SDL_AppEvent(a, &ev); }
                if (a->press_tick[i] + 3 == a->ticks_run) { SDL_Event ev = {0}; ev.type = SDL_EVENT_KEY_UP; ev.key.scancode = a->press_key[i]; ev.key.key = SDL_GetKeyFromScancode(a->press_key[i], SDL_KMOD_NONE, false); SDL_AppEvent(a, &ev); }
            }
            a->ticks_run++;
            osc_game_tick(&a->game, in1, in2);
        }
        a->acc_ns -= TICK_NS;
        if (a->game.speed_delay == 0) a->acc_ns = 0;
    }
    if (a->loaded) {
        /* DXDraw1.Left shift: the 400x300 picture slides, the form behind it is black */
        int dx = a->game.view_dx;
        SDL_SetRenderViewport(a->ren, NULL); SDL_SetRenderClipRect(a->ren, NULL);
        if (dx) {
            SDL_SetRenderDrawColor(a->ren, 0, 0, 0, 255); SDL_RenderClear(a->ren);
            SDL_Rect vp = {dx, 0, OSC_SCREEN_W, OSC_SCREEN_H}, clip = {-dx, 0, OSC_SCREEN_W, OSC_SCREEN_H};
            SDL_SetRenderViewport(a->ren, &vp); SDL_SetRenderClipRect(a->ren, &clip);
        }
        osc_game_render(&a->game);
        SDL_SetRenderViewport(a->ren, NULL); SDL_SetRenderClipRect(a->ren, NULL);
    } else {
        SDL_SetRenderDrawColor(a->ren, 40, 0, 0, 255);
        SDL_RenderClear(a->ren);
        SDL_SetRenderDrawColor(a->ren, 255, 255, 255, 255);
        #ifdef __EMSCRIPTEN__
        SDL_RenderDebugText(a->ren, 60, 140, "Choose your Street Chaves folder below");
#else
        SDL_RenderDebugText(a->ren, 20, 140, "Game data not found. Run with --data <Street Chaves folder>");
#endif
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
    for (int i = 0; i < 2; i++) if (a->pads[i]) SDL_CloseGamepad(a->pads[i]);
    if (a->loaded) osc_game_shutdown(&a->game);
    if (a->ren) SDL_DestroyRenderer(a->ren);
    if (a->win) SDL_DestroyWindow(a->win);
    SDL_free(a);
}
