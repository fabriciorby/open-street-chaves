/* F1 configuration menu (state 2), F2 key configuration (state 3) and the
 * settings file (01-flow-and-input.md "Config file", 07 "F1 configuration
 * menu"). Layout transcribed from the x86 code of the timer handler.
 *
 * The original writes Chaves.dat next to the executable; we never touch the
 * user's game folder, so the file goes to SDL's preferences directory (the
 * original's own Chaves.dat is still read for the initial values). */
#include <stdio.h>
#include <string.h>

#include "exetext.h"
#include "game.h"

static const int k_round_time[5] = {0, 30, 60, 90, 99};

int osc_round_time(const osc_game *g)
{
    int m = g->cfg_time_mode;
    if (m < 1 || m > 4) m = 3;
    return k_round_time[m];
}

/* ---- settings file --------------------------------------------------------- */
static void config_path(char *out, size_t cap, bool pref)
{
    if (pref) {
        char *p = SDL_GetPrefPath("open-street-chaves", "open-street-chaves");
        snprintf(out, cap, "%sChaves.dat", p ? p : "");
        SDL_free(p);
    }
}

static void apply_defaults(osc_game *g)
{
    g->level = 4; g->cfg_time_mode = 3; g->cfg_power = 1; g->damage_level = 0.9;
    g->cfg_blood = true; g->rounds_to_win = 2; g->lang = 0; g->cfg_flash = true;
    g->show_level = true; g->show_credits = true;
    g->snd_test = 1; g->mus_test = 1;
}

static bool read_config(osc_game *g, const char *path)
{
    size_t len = 0;
    char *txt = SDL_LoadFile(path, &len);
    if (!txt) return false;
    int line = 0;
    char *save = NULL;
    for (char *tok = SDL_strtok_r(txt, "\r\n", &save); tok; tok = SDL_strtok_r(NULL, "\r\n", &save), line++) {
        double v = SDL_atof(tok);
        int iv = (int)SDL_strtol(tok, NULL, 10);
        switch (line) {
        case 0: if (iv >= 1 && iv <= 8) g->level = iv; break;
        case 1: if (iv >= 1 && iv <= 4) g->cfg_time_mode = iv; break;
        case 2: if (iv >= 1 && iv <= 4) g->cfg_power = iv; break;
        case 3: if (v >= 0.29 && v <= 2.01) g->damage_level = v; break;
        case 4: g->cfg_blood = iv != 0; break;
        case 5: if (iv >= 1 && iv <= 3) g->rounds_to_win = iv; break;
        case 6: g->lang = iv == 2 ? 1 : 0; break;
        case 7: g->cfg_flash = iv != 0; break;
        case 8: g->show_level = iv != 0; break;
        case 9: g->show_credits = iv != 0; break;
        default:   /* lines 10..25: our own key map, P1 then P2 scancodes */
            if (line >= 10 && line < 26 && iv > 0 && iv < SDL_SCANCODE_COUNT) g->keymap[(line - 10) / 8][(line - 10) % 8] = (SDL_Scancode)iv;
            break;
        }
    }
    SDL_free(txt);
    return true;
}

void osc_config_load(osc_game *g)
{
    static const SDL_Scancode k1[8] = {SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
                                       SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V};
    static const SDL_Scancode k2[8] = {SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D,
                                       SDL_SCANCODE_F, SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J};
    memcpy(g->keymap[0], k1, sizeof k1);
    memcpy(g->keymap[1], k2, sizeof k2);
    apply_defaults(g);
    char path[1200];
    snprintf(path, sizeof path, "%s/Chaves.dat", g->data_dir);
    read_config(g, path);              /* the user's own settings, if any */
    config_path(path, sizeof path, true);
    read_config(g, path);              /* ours win */
}

void osc_config_save(osc_game *g)
{
    char path[1200];
    config_path(path, sizeof path, true);
    SDL_IOStream *f = SDL_IOFromFile(path, "w");
    if (!f) { SDL_Log("config: cannot write %s: %s", path, SDL_GetError()); return; }
    char buf[512];
    int n = snprintf(buf, sizeof buf, "%d\n%d\n%d\n%.1f\n%d\n%d\n%d\n%d\n%d\n%d\n", g->level, g->cfg_time_mode, g->cfg_power,
                     g->damage_level, g->cfg_blood ? 1 : 0, g->rounds_to_win, g->lang == 1 ? 2 : 1,
                     g->cfg_flash ? 1 : 0, g->show_level ? 1 : 0, g->show_credits ? 1 : 0);
    for (int p = 0; p < 2; p++)
        for (int i = 0; i < 8; i++) n += snprintf(buf + n, sizeof buf - (size_t)n, "%d\n", (int)g->keymap[p][i]);
    SDL_WriteIO(f, buf, (size_t)n);
    SDL_CloseIO(f);
}

/* ---- helpers --------------------------------------------------------------- */
static void txt(osc_game *g, float x, float y, const char *s, bool hl)
{
    osc_text_ex(g, x, y, s, 'l', 11, 255, 255, hl ? 0 : 255);
}
static void cfgtxt(osc_game *g, float x, float y, int line, bool hl) { txt(g, x, y, osc_text(OSC_MEMO_CONFIG, line, g->lang), hl); }

static void fillr(osc_game *g, float x1, float y1, float x2, float y2, Uint8 r, Uint8 gg, Uint8 b)
{
    SDL_FRect d = {x1, y1, x2 - x1, y2 - y1};
    SDL_SetRenderDrawColor(g->ren, r, gg, b, 255);
    SDL_RenderFillRect(g->ren, &d);
}

/* ---- state 2: F1 menu ------------------------------------------------------ */
void osc_menu_enter(osc_game *g, int which)
{
    g->screen = which == 2 ? OSC_SCREEN_MENU_F1 : OSC_SCREEN_MENU_F2;
    g->menu_cursor = 1;
    g->p[1].in_prev = g->p[2].in_prev = 0xff;
}

static void menu_leave(osc_game *g)
{
    osc_config_save(g);
    osc_go_attract(g, 0);
}

static void f1_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    uint8_t p1 = (uint8_t)(in1 & ~g->p[1].in_prev), p2 = (uint8_t)(in2 & ~g->p[2].in_prev);
    g->p[1].in_prev = in1; g->p[2].in_prev = in2;
    uint8_t pr = (uint8_t)(p1 | p2);
    int *c = &g->menu_cursor;
    if (pr & OSC_IN_UP) (*c)--;
    if (pr & OSC_IN_DOWN) (*c)++;
    if (*c < 1) *c = 1;
    if (*c > 12) *c = 12;
    int d = (pr & OSC_IN_LEFT) ? -1 : (pr & OSC_IN_RIGHT) ? 1 : 0;
    if (d) {
        switch (*c) {
        case 1: g->level += d; if (g->level < 1) g->level = 1; if (g->level > 8) g->level = 8; break;
        case 2: g->cfg_time_mode += d; if (g->cfg_time_mode < 1) g->cfg_time_mode = 1; if (g->cfg_time_mode > 4) g->cfg_time_mode = 4; break;
        case 3: g->cfg_power += d; if (g->cfg_power < 1) g->cfg_power = 1; if (g->cfg_power > 4) g->cfg_power = 4; break;
        case 4: g->damage_level += 0.1 * d; if (g->damage_level < 0.3) g->damage_level = 0.3; if (g->damage_level > 2.0) g->damage_level = 2.0; break;
        case 5: g->cfg_blood = d < 0; break;
        case 6: g->rounds_to_win += d; if (g->rounds_to_win < 1) g->rounds_to_win = 1; if (g->rounds_to_win > 3) g->rounds_to_win = 3; break;
        case 7: g->lang = d > 0 ? 1 : 0; break;
        case 8: g->snd_test += d; if (g->snd_test < 1) g->snd_test = 1; if (g->snd_test > g->n_sounds) g->snd_test = g->n_sounds; break;
        case 9: g->mus_test += d; if (g->mus_test < 1) g->mus_test = 1; if (g->mus_test > 6) g->mus_test = 6; break;
        case 10: g->cfg_flash = d < 0; break;
        case 11: g->show_level = d < 0; break;
        case 12: g->show_credits = d < 0; break;
        }
    }
    if (pr & (OSC_IN_B1 | OSC_IN_B2 | OSC_IN_B3 | OSC_IN_B4)) {
        if (*c == 8 && g->snd_test >= 1 && g->snd_test <= g->n_sounds) osc_play_snd(g, g->snd_test - 1);
        if (*c == 9) osc_play_music(g, g->mus_test);
    }
}

static void f1_render(osc_game *g)
{
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderClear(g->ren);
    int c = g->menu_cursor;
    char buf[48];
    osc_text_ex(g, 200, 5, osc_text(OSC_MEMO_CONFIG, 1, g->lang), 'c', 15, 255, 255, 255);
    static const int label_line[13] = {0, 2, 3, 6, 11, 12, 13, 14, 17, 18, 19, 20, 21};
    static const float row_y[13] = {0, 60, 75, 90, 105, 120, 135, 150, 165, 180, 195, 210, 225};
    for (int i = 1; i <= 12; i++) cfgtxt(g, 10, row_y[i], label_line[i], c == i);
    /* 1 Nível do Jogo: 1..8 */
    for (int i = 1; i <= 8; i++) { snprintf(buf, sizeof buf, "%d", i); txt(g, (float)(140 + 20 * (i - 1)), 60, buf, g->level == i); }
    /* 2 Tempo do Round */
    static const char *times[4] = {"30", "60", "90", "--"};
    for (int i = 0; i < 4; i++) txt(g, (float)(140 + 50 * i), 75, times[i], g->cfg_time_mode == i + 1);
    /* 3 Poder */
    static const int power_x[4] = {140, 215, 280, 345};
    for (int i = 0; i < 4; i++) cfgtxt(g, (float)power_x[i], 90, 7 + i, g->cfg_power == i + 1);
    /* 4 Nível de Dano: bar 78 px per 1.0 and percentage */
    int bw = (int)SDL_lround(78.0 * g->damage_level);
    fillr(g, 140, 110, 300, 122, 255, 255, 255);
    fillr(g, 141, 111, 299, 121, 0, 0, 0);
    fillr(g, 142, 112, (float)(142 + bw), 119, 255, 255, 255);
    fillr(g, 142, 113, (float)(142 + bw), 114, 100, 100, 255);
    snprintf(buf, sizeof buf, "%d%%", (int)SDL_lround(100.0 * g->damage_level));
    txt(g, 310, 105, buf, c == 4);
    /* 5 Sangue */
    cfgtxt(g, 140, 120, 4, g->cfg_blood); cfgtxt(g, 230, 120, 5, !g->cfg_blood);
    /* 6 Rounds */
    for (int i = 1; i <= 3; i++) { snprintf(buf, sizeof buf, "%d", i); txt(g, (float)(140 + 30 * (i - 1)), 135, buf, g->rounds_to_win == i); }
    /* 7 Linguagem */
    cfgtxt(g, 140, 150, 15, g->lang == 0); cfgtxt(g, 230, 150, 16, g->lang == 1);
    /* 8, 9 tests */
    snprintf(buf, sizeof buf, "%d / %d", g->snd_test, g->n_sounds); txt(g, 140, 165, buf, c == 8);
    snprintf(buf, sizeof buf, "%d / %d", g->mus_test, 6); txt(g, 140, 180, buf, c == 9);
    /* 10..12 booleans */
    cfgtxt(g, 140, 195, 4, g->cfg_flash); cfgtxt(g, 230, 195, 5, !g->cfg_flash);
    cfgtxt(g, 140, 210, 4, g->show_level); cfgtxt(g, 230, 210, 5, !g->show_level);
    cfgtxt(g, 140, 225, 4, g->show_credits); cfgtxt(g, 230, 225, 5, !g->show_credits);
    osc_text_ex(g, 200, 280, osc_text(OSC_MEMO_CONFIG, 22, g->lang), 'c', 11, 255, 255, 255);   /* F1-SAIR */
}

/* ---- state 3: F2 key configuration (P1 keyboard) --------------------------- */
static void f2_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    (void)in1; (void)in2;
    if (g->last_key == SDL_SCANCODE_UNKNOWN) return;
    SDL_Scancode k = g->last_key;
    g->last_key = SDL_SCANCODE_UNKNOWN;
    int *c = &g->menu_cursor;
    if (k == SDL_SCANCODE_F1 || k == SDL_SCANCODE_F2 || k == SDL_SCANCODE_ESCAPE) return;
    /* the original assigns any letter/digit/arrow/numpad key to the current action and moves on */
    bool ok = (k >= SDL_SCANCODE_A && k <= SDL_SCANCODE_0) || (k >= SDL_SCANCODE_RIGHT && k <= SDL_SCANCODE_UP) ||
              (k >= SDL_SCANCODE_KP_1 && k <= SDL_SCANCODE_KP_0) || k == SDL_SCANCODE_SPACE || k == SDL_SCANCODE_RETURN ||
              (k >= SDL_SCANCODE_LCTRL && k <= SDL_SCANCODE_RGUI) || k == SDL_SCANCODE_LSHIFT;
    if (!ok) return;
    g->keymap[0][*c - 1] = k;
    (*c)++;
    if (*c > 8) *c = 1;
}

static void f2_render(osc_game *g)
{
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderClear(g->ren);
    osc_text_ex(g, 200, 5, osc_text(OSC_MEMO_CONFIG, 23, g->lang), 'c', 15, 255, 255, 255);
    for (int i = 1; i <= 8; i++) {
        float y = (float)(80 + 20 * (i - 1));
        cfgtxt(g, 50, y, 23 + i, g->menu_cursor == i);
        SDL_Scancode k = g->keymap[0][i - 1];
        int icon = k == SDL_SCANCODE_UP ? 0 : k == SDL_SCANCODE_DOWN ? 1 : k == SDL_SCANCODE_RIGHT ? 2 : k == SDL_SCANCODE_LEFT ? 3 : -1;
        if (icon >= 0 && icon < g->n_icone && g->icone[icon]) {
            SDL_FRect d = {200, y + 2, (float)g->icone_w[icon], (float)g->icone_h[icon]};
            SDL_RenderTexture(g->ren, g->icone[icon], NULL, &d);
        } else {
            txt(g, 200, y, SDL_GetScancodeName(k), g->menu_cursor == i);
        }
    }
    osc_text_ex(g, 200, 280, osc_text(OSC_MEMO_CONFIG, 32, g->lang), 'c', 11, 255, 255, 255);   /* F2-SAIR */
}

void osc_menu_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    if (g->key_f1 || g->key_f2) { g->key_f1 = g->key_f2 = false; menu_leave(g); return; }
    if (g->screen == OSC_SCREEN_MENU_F1) f1_tick(g, in1, in2); else f2_tick(g, in1, in2);
}

void osc_menu_render(osc_game *g)
{
    if (g->screen == OSC_SCREEN_MENU_F1) f1_render(g); else f2_render(g);
}
