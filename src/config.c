/* Settings: the Chaves.dat loader and writer (01-flow-and-input.md "Config
 * file"), FormCreate defaults (0x49d05d) and the round-time table.
 *
 * The original writes Chaves.dat next to the executable; we never touch the
 * user's game folder, so the file goes to SDL's preferences directory (the
 * original's own Chaves.dat is still read for the initial values). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "game.h"

static const int k_round_time[5] = {0, 30, 60, 90, 99};

int osc_round_time(const osc_game *g)
{
    int m = g->settings.cfg_time_mode;
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
    g->settings.level = 4; g->settings.cfg_time_mode = 3; g->settings.cfg_power = 1; g->settings.damage_level = 0.9;
    g->settings.cfg_blood = true; g->settings.rounds_to_win = 2; g->settings.lang = 0; g->settings.cfg_flash = true;
    g->settings.show_level = true; g->settings.show_credits = true;
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

/* one line of Chaves.dat; the original's loader (0x49d0c9) reads lines 0..9 by index, a missing or
 * malformed line keeps the default, bools are exactly "0" or "1" and the clamps run afterwards */
static void config_line(osc_game *g, int line, const char *tok)
{
    int v;
    switch (line) {
    case 0: if (parse_byte(tok, &v)) g->settings.level = clampi(v, 1, 8); break;
    case 1: if (tok[0] >= '1' && tok[0] <= '4' && !tok[1]) g->settings.cfg_time_mode = tok[0] - '0'; break;
    case 2: if (parse_byte(tok, &v)) g->settings.cfg_power = clampi(v, 1, 4); break;
    case 3: {   /* StrToFloat: the decimal separator is the writer's locale's */
        char tmp[64], *end;
        snprintf(tmp, sizeof tmp, "%s", tok);
        for (char *c = tmp; *c; c++) if (*c == ',') *c = '.';
        double d = strtod(tmp, &end);
        if (end == tmp || *end) break;
        if (0.3L > (long double)d) d = 0.3;
        if (d > 2.0) d = 2.0;
        g->settings.damage_level = d;
        break;
    }
    case 4: if (!strcmp(tok, "0")) g->settings.cfg_blood = false; if (!strcmp(tok, "1")) g->settings.cfg_blood = true; break;
    case 5: if (parse_byte(tok, &v)) g->settings.rounds_to_win = clampi(v, 1, 3); break;
    case 6: if (parse_byte(tok, &v)) g->settings.lang = clampi(v, 1, 2) == 2 ? 1 : 0; break;
    case 7: if (!strcmp(tok, "0")) g->settings.cfg_flash = false; if (!strcmp(tok, "1")) g->settings.cfg_flash = true; break;
    case 8: if (!strcmp(tok, "0")) g->settings.show_level = false; if (!strcmp(tok, "1")) g->settings.show_level = true; break;
    case 9: if (!strcmp(tok, "0")) g->settings.show_credits = false; if (!strcmp(tok, "1")) g->settings.show_credits = true; break;
    default:   /* lines 10..25: our own key map (the original's F2 menu is not persisted), P1 then P2 scancodes */
        if (line >= 10 && line < 26 && parse_byte(tok, &v) && v > 0 && v < SDL_SCANCODE_COUNT)
            g->settings.keymap[(line - 10) / 8][(line - 10) % 8] = (SDL_Scancode)v;
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
    memcpy(g->settings.keymap[0], k1, sizeof k1);
    memcpy(g->settings.keymap[1], k2, sizeof k2);
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
    int n = snprintf(buf, sizeof buf, "%d\r\n%d\r\n%d\r\n%.15g\r\n%d\r\n%d\r\n%d\r\n%d\r\n%d\r\n%d\r\n", g->settings.level, g->settings.cfg_time_mode, g->settings.cfg_power,
                     g->settings.damage_level, g->settings.cfg_blood ? 1 : 0, g->settings.rounds_to_win, g->settings.lang == 1 ? 2 : 1,
                     g->settings.cfg_flash ? 1 : 0, g->settings.show_level ? 1 : 0, g->settings.show_credits ? 1 : 0);
    for (int p = 0; p < 2; p++)
        for (int i = 0; i < 8; i++) n += snprintf(buf + n, sizeof buf - (size_t)n, "%d\r\n", (int)g->settings.keymap[p][i]);
    SDL_WriteIO(f, buf, (size_t)n);
    SDL_CloseIO(f);
}
