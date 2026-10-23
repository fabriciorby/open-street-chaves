/* F1 configuration menu (state 2), F2 key configuration (state 3) and the
 * settings file (01-flow-and-input.md "Config file", 07 "F1 configuration
 * menu"). Drawing, input and value handling follow the x86 code of the timer
 * handler (0x4a0178 state 2, 0x4a1b55 state 3) and FormKeyDown (0x49e2e8);
 * 07b-layout.md "Menus reviewed against the binary" has the call counts.
 *
 * The original writes Chaves.dat next to the executable; we never touch the
 * user's game folder, so the file goes to SDL's preferences directory (the
 * original's own Chaves.dat is still read for the initial values). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "canvas.h"
#include "exetext.h"
#include "game.h"
#include "wav.h"

static const int k_round_time[5] = {0, 30, 60, 90, 99};

int osc_round_time(const osc_game *g)
{
    int m = g->cfg_time_mode;
    if (m < 1 || m > 4) m = 3;
    return k_round_time[m];
}

/* ---- settings file --------------------------------------------------------- */
static void config_path(char *out, size_t cap)
{
    char *p = SDL_GetPrefPath("open-street-chaves", "open-street-chaves");
    snprintf(out, cap, "%sChaves.dat", p ? p : "");
    SDL_free(p);
}

/* FormCreate defaults (0x49d05d..0x49d0c1) */
static void apply_defaults(osc_game *g)
{
    g->level = 4; g->cfg_time_mode = 3; g->cfg_power = 1; g->damage_level = 0.9;
    g->cfg_blood = true; g->rounds_to_win = 2; g->lang = 0; g->cfg_flash = true;
    g->show_level = true; g->show_credits = true;
    g->snd_test = 0; g->mus_test = 0;
}

/* StrToInt stored into a Byte (the original does not range-check before the clamps) */
static bool parse_byte(const char *s, int *out)
{
    char *end;
    while (*s == ' ') s++;
    long v = strtol(s, &end, 10);
    if (end == s) return false;
    while (*end == ' ') end++;
    if (*end) return false;
    *out = (int)(uint8_t)v;
    return true;
}
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* one line of Chaves.dat; the original's loader (0x49d0c9) reads lines 0..9 by index, a missing or
 * malformed line keeps the default, bools are exactly "0" or "1" and the clamps run afterwards */
static void config_line(osc_game *g, int line, const char *tok)
{
    int v;
    switch (line) {
    case 0: if (parse_byte(tok, &v)) g->level = clampi(v, 1, 8); break;
    case 1: if (tok[0] >= '1' && tok[0] <= '4' && !tok[1]) g->cfg_time_mode = tok[0] - '0'; break;
    case 2: if (parse_byte(tok, &v)) g->cfg_power = clampi(v, 1, 4); break;
    case 3: {   /* StrToFloat: the decimal separator is the writer's locale's */
        char tmp[64], *end;
        snprintf(tmp, sizeof tmp, "%s", tok);
        for (char *c = tmp; *c; c++) if (*c == ',') *c = '.';
        double d = strtod(tmp, &end);
        if (end == tmp || *end) break;
        if (0.3L > (long double)d) d = 0.3;
        if (d > 2.0) d = 2.0;
        g->damage_level = d;
        break;
    }
    case 4: if (!strcmp(tok, "0")) g->cfg_blood = false; if (!strcmp(tok, "1")) g->cfg_blood = true; break;
    case 5: if (parse_byte(tok, &v)) g->rounds_to_win = clampi(v, 1, 3); break;
    case 6: if (parse_byte(tok, &v)) g->lang = clampi(v, 1, 2) == 2 ? 1 : 0; break;
    case 7: if (!strcmp(tok, "0")) g->cfg_flash = false; if (!strcmp(tok, "1")) g->cfg_flash = true; break;
    case 8: if (!strcmp(tok, "0")) g->show_level = false; if (!strcmp(tok, "1")) g->show_level = true; break;
    case 9: if (!strcmp(tok, "0")) g->show_credits = false; if (!strcmp(tok, "1")) g->show_credits = true; break;
    default:   /* lines 10..25: our own key map (the original's F2 menu is not persisted), P1 then P2 scancodes */
        if (line >= 10 && line < 26 && parse_byte(tok, &v) && v > 0 && v < SDL_SCANCODE_COUNT)
            g->keymap[(line - 10) / 8][(line - 10) % 8] = (SDL_Scancode)v;
        break;
    }
}

static bool read_config(osc_game *g, const char *path)
{
    size_t len = 0;
    char *txt = SDL_LoadFile(path, &len);
    if (!txt) return false;
    /* TStrings.LoadFromFile: lines end at CR, LF or CRLF and empty lines count */
    int line = 0;
    size_t i = 0;
    while (i < len) {
        size_t s = i;
        while (i < len && txt[i] != '\r' && txt[i] != '\n') i++;
        char tok[64];
        size_t n = i - s < sizeof tok - 1 ? i - s : sizeof tok - 1;
        memcpy(tok, txt + s, n); tok[n] = 0;
        config_line(g, line++, tok);
        if (i < len && txt[i] == '\r') i++;
        if (i < len && txt[i] == '\n') i++;
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
    config_path(path, sizeof path);
    read_config(g, path);              /* ours win */
}

/* Leaving the F1 menu (0x49e47b): Memo.Lines.Clear, one Add per value, SaveToFile('Chaves.dat'): ten lines,
 * CRLF, the damage level as FloatToStr (15 digits, "1" not "1.0"). Lines 10..25 are ours and the original's
 * loader ignores them. */
void osc_config_save(osc_game *g)
{
    char path[1200];
    config_path(path, sizeof path);
    SDL_IOStream *f = SDL_IOFromFile(path, "wb");
    if (!f) { SDL_Log("config: cannot write %s: %s", path, SDL_GetError()); return; }
    char buf[512];
    int n = snprintf(buf, sizeof buf, "%d\r\n%d\r\n%d\r\n%.15g\r\n%d\r\n%d\r\n%d\r\n%d\r\n%d\r\n%d\r\n", g->level, g->cfg_time_mode, g->cfg_power,
                     g->damage_level, g->cfg_blood ? 1 : 0, g->rounds_to_win, g->lang == 1 ? 2 : 1,
                     g->cfg_flash ? 1 : 0, g->show_level ? 1 : 0, g->show_credits ? 1 : 0);
    for (int p = 0; p < 2; p++)
        for (int i = 0; i < 8; i++) n += snprintf(buf + n, sizeof buf - (size_t)n, "%d\r\n", (int)g->keymap[p][i]);
    SDL_WriteIO(f, buf, (size_t)n);
    SDL_CloseIO(f);
}

/* ---- helpers --------------------------------------------------------------- */
/* the original's texto (0x48e720) with align '-' and size 11; every row is first drawn white and the
 * current choices are drawn again on top in RGB(130,130,255) */
static const osc_rgb k_blue = {130, 130, 255};
static void ltxt(osc_game *g, int x, int y, const char *s, osc_rgb c) { texto(g, x, y, s, '-', 11, c); }
static void cfgtxt(osc_game *g, int x, int y, int line, osc_rgb c) { ltxt(g, x, y, osc_text(OSC_MEMO_CONFIG, line, g->lang), c); }

/* where the F1 menu came back to: leaving it only sets g_state := 1, the attract cycle carries on */
static int s_resume_tick, s_resume_cam, s_resume_ciclo, s_n_music;

/* ---- state 2: F1 menu ------------------------------------------------------ */
void osc_menu_enter(osc_game *g, int which)
{
    if (g->screen == OSC_SCREEN_ATTRACT) { s_resume_tick = g->screen_tick; s_resume_cam = g->menu_cam; s_resume_ciclo = g->ciclo; }
    g->screen = which == 2 ? OSC_SCREEN_MENU_F1 : OSC_SCREEN_MENU_F2;
    g->menu_cursor = 1;
    g->p[1].in_prev = g->p[2].in_prev = 0xff;
    if (which == 2) {
        g->snd_test = g->mus_test = 1;   /* g_sndIndexA / g_sndFlagB := 1 */
        /* 0x4b58a3: FormCreate counts Musicas/Musica-NN.wav */
        s_n_music = 0;
        for (int n = 1; n < 100; n++) {
            char p[1200];
            SDL_PathInfo info;
            snprintf(p, sizeof p, "%s/Musicas/Musica-%02d.wav", g->data_dir, n);
            if (!SDL_GetPathInfo(p, &info)) break;
            s_n_music = n;
        }
    }
}

static void menu_leave(osc_game *g)
{
    if (g->screen == OSC_SCREEN_MENU_F1) {
        osc_audio_stop_music();          /* the test sound and music are stopped, then both indexes := 0 */
        g->snd_test = g->mus_test = 0;
    }
    osc_config_save(g);
    g->screen = OSC_SCREEN_ATTRACT;
    g->screen_tick = s_resume_tick; g->menu_cam = s_resume_cam; g->ciclo = s_resume_ciclo;
}

/* FPU extended arithmetic of the original: x +/- 0.1 (an 80-bit constant) stored back as a double */
static void damage_step(osc_game *g, int d)
{
    g->damage_level = (double)((long double)g->damage_level + (d < 0 ? -0.1L : 0.1L));
}

static void f1_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    /* edge of each device against last tick's state (Players[].0xac.. bytes); P1 = keyboard, P2 = joystick */
    uint8_t e1 = (uint8_t)(in1 & ~g->p[1].in_prev), e2 = (uint8_t)(in2 & ~g->p[2].in_prev);
    g->p[1].in_prev = in1; g->p[2].in_prev = in2;
    uint8_t c = (uint8_t)g->menu_cursor;   /* a Byte: P1 and P2 both pressing up at 1 gives 255 */
    if (e1 & OSC_IN_UP) c--;
    if (e1 & OSC_IN_DOWN) c++;
    if (e2 & OSC_IN_UP) c--;
    if (e2 & OSC_IN_DOWN) c++;
    if ((e1 | e2) & OSC_IN_LEFT) {
        switch (c) {
        case 1: g->level--; break;
        case 2: g->cfg_time_mode--; break;
        case 3: g->cfg_power--; break;
        case 4: damage_step(g, -1); break;
        case 5: g->cfg_blood = true; break;
        case 6: g->rounds_to_win--; break;
        case 7: g->lang = 0; break;
        case 8: if (g->snd_test > 1) g->snd_test--; break;
        case 9: if (g->mus_test > 1) g->mus_test--; break;
        case 10: g->cfg_flash = true; break;
        case 11: g->show_level = true; break;
        case 12: g->show_credits = true; break;
        }
    }
    if ((e1 | e2) & OSC_IN_RIGHT) {
        switch (c) {
        case 1: g->level++; break;
        case 2: g->cfg_time_mode++; break;
        case 3: g->cfg_power++; break;
        case 4: damage_step(g, 1); break;
        case 5: g->cfg_blood = false; break;
        case 6: g->rounds_to_win++; break;
        case 7: g->lang = 1; break;
        case 8: if (g->snd_test < g->n_sounds) g->snd_test++; break;
        case 9: if (g->mus_test < s_n_music) g->mus_test++; break;
        case 10: g->cfg_flash = false; break;
        case 11: g->show_level = false; break;
        case 12: g->show_credits = false; break;
        }
    }
    if ((e1 | e2) & OSC_IN_B1) {   /* only the first button plays */
        if (c == 8 && g->n_sounds > g->snd_test - 1) osc_play_snd(g, g->snd_test - 1);
        if (c == 9) osc_play_music(g, g->mus_test);
    }
    if ((e1 | e2) & OSC_IN_B2) osc_audio_stop_music();   /* the second button stops the music, on any row */
    /* the clamps run after the input; the cursor wraps */
    if (c < 1) c = 12;
    if (c > 12) c = 1;
    g->menu_cursor = c;
    g->cfg_time_mode = clampi(g->cfg_time_mode, 1, 4);
    g->level = clampi(g->level, 1, 8);
    g->cfg_power = clampi(g->cfg_power, 1, 4);
    if (0.3L > (long double)g->damage_level) g->damage_level = 0.3;
    if (g->damage_level > 2.0) g->damage_level = 2.0;
    g->rounds_to_win = clampi(g->rounds_to_win, 1, 3);
    if (g->snd_test < 1) g->snd_test = 1;
    if (g->snd_test > g->n_sounds) g->snd_test = g->n_sounds;
}

static void f1_render(osc_game *g)
{
    char buf[48];
    int c = g->menu_cursor;
    const osc_rgb w = k_white;
    barra(g, 0, 0, 400, 300, k_black);
    texto(g, 200, 5, osc_text(OSC_MEMO_CONFIG, 1, g->lang), 'c', 15, w);
    /* 1 Nível do Jogo */
    cfgtxt(g, 10, 60, 2, w);
    for (int i = 1; i <= 8; i++) { snprintf(buf, sizeof buf, "%d", i); ltxt(g, 140 + 10 * (i - 1), 60, buf, w); }
    /* 2 Tempo do Round */
    cfgtxt(g, 10, 75, 3, w);
    ltxt(g, 140, 75, "30", w); ltxt(g, 190, 75, "60", w); ltxt(g, 240, 75, "90", w); ltxt(g, 290, 75, "--", w);
    /* 3 Poder */
    cfgtxt(g, 10, 90, 6, w);
    cfgtxt(g, 140, 90, 7, w); cfgtxt(g, 215, 90, 8, w); cfgtxt(g, 280, 90, 9, w); cfgtxt(g, 345, 90, 10, w);
    /* 4 Nível de Dano: 78 px per 1.0, a 2 px red band on top, and the percentage (Round: half to even) */
    cfgtxt(g, 10, 105, 11, w);
    barraredonda(g, 140, 110, 300, 121, 5, w, k_black);
    int bw = (int)lrintl(78.0L * g->damage_level);
    barra(g, 142, 112, 142 + bw, 119, rgb(255, 0, 0));
    barra(g, 142, 113, 142 + bw, 114, rgb(255, 100, 100));
    snprintf(buf, sizeof buf, "%lld%%", (long long)llrintl(g->damage_level * 100.0L));
    ltxt(g, 310, 105, buf, w);
    /* 5 Sangue, 6 Rounds, 7 Linguagem */
    cfgtxt(g, 10, 120, 12, w); cfgtxt(g, 140, 120, 4, w); cfgtxt(g, 230, 120, 5, w);
    cfgtxt(g, 10, 135, 13, w);
    for (int i = 1; i <= 3; i++) { snprintf(buf, sizeof buf, "%d", i); ltxt(g, 140 + 30 * (i - 1), 135, buf, w); }
    cfgtxt(g, 10, 150, 14, w); cfgtxt(g, 140, 150, 15, w); cfgtxt(g, 230, 150, 16, w);
    /* 8, 9 tests: "n / max" */
    cfgtxt(g, 10, 165, 17, w);
    snprintf(buf, sizeof buf, "%d / %d", g->snd_test, g->n_sounds); ltxt(g, 140, 165, buf, w);
    cfgtxt(g, 10, 180, 18, w);
    snprintf(buf, sizeof buf, "%d / %d", g->mus_test, s_n_music); ltxt(g, 140, 180, buf, w);
    /* 10..12 Flash, Mostrar Nível, Mostrar Creditos */
    for (int i = 0; i < 3; i++) {
        cfgtxt(g, 10, 195 + 15 * i, 19 + i, w);
        cfgtxt(g, 140, 195 + 15 * i, 4, w); cfgtxt(g, 230, 195 + 15 * i, 5, w);
    }
    texto(g, 200, 280, osc_text(OSC_MEMO_CONFIG, 22, g->lang), 'c', 11, w);   /* F1-SAIR */
    /* the cursor row's label and every current choice again in blue */
    static const int label_line[13] = {0, 2, 3, 6, 11, 12, 13, 14, 17, 18, 19, 20, 21};
    if (c >= 1 && c <= 12) cfgtxt(g, 10, 45 + 15 * c, label_line[c], k_blue);
    snprintf(buf, sizeof buf, "%d", g->level); ltxt(g, 140 + 10 * (g->level - 1), 60, buf, k_blue);
    static const char *times[5] = {"", "30", "60", "90", "--"};
    if (g->cfg_time_mode >= 1 && g->cfg_time_mode <= 4) ltxt(g, 140 + 50 * (g->cfg_time_mode - 1), 75, times[g->cfg_time_mode], k_blue);
    static const int power_x[5] = {0, 140, 215, 280, 345};
    if (g->cfg_power >= 1 && g->cfg_power <= 4) cfgtxt(g, power_x[g->cfg_power], 90, 6 + g->cfg_power, k_blue);
    cfgtxt(g, g->cfg_blood ? 140 : 230, 120, g->cfg_blood ? 4 : 5, k_blue);
    snprintf(buf, sizeof buf, "%d", g->rounds_to_win); ltxt(g, 140 + 30 * (g->rounds_to_win - 1), 135, buf, k_blue);
    cfgtxt(g, g->lang == 0 ? 140 : 230, 150, g->lang == 0 ? 15 : 16, k_blue);
    const bool flag[3] = {g->cfg_flash, g->show_level, g->show_credits};
    for (int i = 0; i < 3; i++) cfgtxt(g, flag[i] ? 140 : 230, 195 + 15 * i, flag[i] ? 4 : 5, k_blue);
}

/* ---- state 3: F2 key configuration (P1 keyboard) --------------------------- */
/* FormKeyDown (0x49e789): while in state 3 a letter, digit, arrow or numpad key (VK 41..5A, 30..39, 25..28,
 * 60..69) is stored in DXInput's assign slot of the current row and the cursor moves on, with no wrap (the
 * timer clamps it to 9, after which keys do nothing). Auto-repeat counts: WM_KEYDOWN repeats. */
static bool f2_key_ok(SDL_Scancode k)
{
    return (k >= SDL_SCANCODE_A && k <= SDL_SCANCODE_0) || (k >= SDL_SCANCODE_RIGHT && k <= SDL_SCANCODE_UP) ||
           (k >= SDL_SCANCODE_KP_1 && k <= SDL_SCANCODE_KP_0);
}

void osc_menu_key(osc_game *g, SDL_Scancode k)
{
    if (g->screen != OSC_SCREEN_MENU_F2 || !f2_key_ok(k)) return;
    uint8_t c = (uint8_t)g->menu_cursor;
    if (c >= 1 && c <= 8) g->keymap[0][c - 1] = k;
    g->menu_cursor = (uint8_t)(c + 1);
}

/* the character Delphi's Chr(VK) gives for the key; the numpad's VK 60..69 are ` and a..i */
static char vk_char(SDL_Scancode k)
{
    if (k >= SDL_SCANCODE_A && k <= SDL_SCANCODE_Z) return (char)('A' + (k - SDL_SCANCODE_A));
    if (k >= SDL_SCANCODE_1 && k <= SDL_SCANCODE_9) return (char)('1' + (k - SDL_SCANCODE_1));
    if (k == SDL_SCANCODE_0) return '0';
    if (k >= SDL_SCANCODE_KP_1 && k <= SDL_SCANCODE_KP_9) return (char)('a' + (k - SDL_SCANCODE_KP_1));
    if (k == SDL_SCANCODE_KP_0) return '`';
    return 0;
}

static void f2_tick(osc_game *g)
{
    if (g->menu_cursor < 1) g->menu_cursor = 1;
    if (g->menu_cursor > 9) g->menu_cursor = 9;
}

static void f2_render(osc_game *g)
{
    const osc_rgb w = k_white;
    barra(g, 0, 0, 400, 300, k_black);
    texto(g, 200, 5, osc_text(OSC_MEMO_CONFIG, 23, g->lang), 'c', 15, w);
    for (int i = 1; i <= 8; i++) cfgtxt(g, 50, 60 + 20 * i, 23 + i, w);
    for (int i = 1; i <= 8; i++) {
        SDL_Scancode k = g->keymap[0][i - 1];
        int icon = k == SDL_SCANCODE_UP ? 0 : k == SDL_SCANCODE_DOWN ? 1 : k == SDL_SCANCODE_RIGHT ? 2 : k == SDL_SCANCODE_LEFT ? 3 : -1;
        if (icon >= 0) {
            if (icon < g->n_icone && g->icone[icon]) draw_tex(g, g->icone[icon], 200, (float)(85 + 20 * (i - 1)), 255, false);
        } else {
            char s[2] = {vk_char(k), 0};
            ltxt(g, 200, 60 + 20 * i, s[0] ? s : SDL_GetScancodeName(k), w);
        }
    }
    if (g->menu_cursor >= 1 && g->menu_cursor <= 8) cfgtxt(g, 50, 60 + 20 * g->menu_cursor, 23 + g->menu_cursor, k_blue);
    texto(g, 200, 280, osc_text(OSC_MEMO_CONFIG, 32, g->lang), 'c', 11, w);   /* F2-SAIR */
}

void osc_menu_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    /* F1 leaves state 2 only and F2 leaves state 3 only; the other key does nothing there */
    bool leave = g->screen == OSC_SCREEN_MENU_F1 ? g->key_f1 : g->key_f2;
    g->key_f1 = g->key_f2 = false;
    if (leave) { menu_leave(g); return; }
    if (g->screen == OSC_SCREEN_MENU_F1) f1_tick(g, in1, in2); else f2_tick(g);
}

void osc_menu_render(osc_game *g)
{
    if (g->screen == OSC_SCREEN_MENU_F1) f1_render(g); else f2_render(g);
}
