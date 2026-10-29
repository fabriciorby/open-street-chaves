#include "data/exetext.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

#include "gen/tables_gen.h"

#define MAX_CONST 4096

static char **g_strs;
static int g_count;
/* Portuguese block sizes per memo (07): Spanish lines follow at line + size */
static const int k_block[7] = {16, 32, 12, 650, 180, 60, 33};

static uint32_t rd32(const uint8_t *p) { return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24)); }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

/* Windows-1252 -> UTF-8 (only the accented range matters here). */
static char *to_utf8(const uint8_t *s, size_t n)
{
    char *out = malloc(n * 3 + 1), *o = out;
    for (size_t i = 0; i < n; i++) {
        unsigned c = s[i];
        if (c < 0x80) *o++ = (char)c;
        else if (c < 0xa0) *o++ = '?';   /* the C1 range is not used by the game */
        else { *o++ = (char)(0xc0 | (c >> 6)); *o++ = (char)(0x80 | (c & 0x3f)); }
    }
    *o = 0;
    return out;
}

bool osc_text_load(const char *data_dir)
{
    char path[1200];
    SDL_snprintf(path, sizeof path, "%s/Chaves.exe", data_dir);
    size_t len = 0;
    uint8_t *d = SDL_LoadFile(path, &len);
    if (!d) {
        SDL_Log("text: %s not found, in-game text disabled", path);
        return false;
    }
    bool ok = false;
    if (len > 0x400 && d[0] == 'M' && d[1] == 'Z') {
        uint32_t pe = rd32(d + 0x3c);
        if (pe + 24 < len && memcmp(d + pe, "PE\0\0", 4) == 0) {
            uint16_t nsec = rd16(d + pe + 6), optsz = rd16(d + pe + 20);
            const uint8_t *sec = d + pe + 24 + optsz;
            /* first section = CODE in Delphi executables */
            if (nsec > 0 && (size_t)(sec + 40 - d) <= len) {
                uint32_t rsize = rd32(sec + 16), raw = rd32(sec + 20);
                if ((size_t)raw + rsize <= len) {
                    g_strs = calloc(MAX_CONST, sizeof *g_strs);
                    size_t p = raw + 8, end = raw + rsize;
                    while (p + 1 < end && g_count < MAX_CONST) {
                        if (rd32(d + p - 8) == 0xffffffffu) {
                            uint32_t ln = rd32(d + p - 4);
                            if (ln > 0 && ln <= 512 && p + ln < end && d[p + ln] == 0) {
                                bool text = true;
                                for (uint32_t i = 0; i < ln && text; i++)
                                    if (d[p + i] < 0x20 && d[p + i] != 9 && d[p + i] != 10 && d[p + i] != 13) text = false;
                                if (text) {
                                    g_strs[g_count++] = to_utf8(d + p, ln);
                                    p = p + ln + 1;
                                    p += (4 - (p & 3)) & 3;
                                    continue;
                                }
                            }
                        }
                        p += 4;
                    }
                    ok = g_count > 0;
                }
            }
        }
    }
    SDL_free(d);
    SDL_Log("text: %d string constants read from Chaves.exe", g_count);
    return ok;
}

void osc_text_free(void)
{
    for (int i = 0; i < g_count; i++) free(g_strs[i]);
    free(g_strs);
    g_strs = NULL;
    g_count = 0;
}

int osc_text_count(void) { return g_count; }

const char *osc_text_const(int index)
{
    return index >= 0 && index < g_count ? g_strs[index] : "";
}

const char *osc_text(int memo, int line, int lang)
{
    if (memo < 0 || memo > 6) return "";
    if (lang) line += k_block[memo];
    for (int i = 0; i < OSC_N_TEXT_INDEX; i++) {
        const osc_text_index_row *r = &osc_text_index[i];
        if (r->memo == memo && r->line == line) {
            if (r->index < 0 || r->index >= g_count) return "";
            return g_strs[r->index];
        }
    }
    return "";
}
