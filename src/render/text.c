/* Text helpers: osc_text_at (menu/flow debug-style labels) and osc_text_ex
 * (TForm1.texto). Both use the TrueType font when it loaded, else SDL's debug font. */
#include <stddef.h>

#include "render/font.h"
#include "game.h"
#include "render/text.h"

void osc_text_at(osc_game *g, float x, float y, const char *s, bool centered)
{
    if (!s || !*s) return;
    if (osc_font_ready()) { osc_text_ex(g, x, y, s, centered ? 'c' : 'l', 8, 255, 255, 255); return; }
    /* the debug font is ASCII only: strip UTF-8 accents to the base letter */
    char buf[128];
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p && n < sizeof buf - 1; p++) {
        if (*p < 0x80) buf[n++] = (char)*p;
        else if ((*p & 0xe0) == 0xc0 && p[1]) { unsigned cp = ((*p & 0x1f) << 6) | (p[1] & 0x3f); p++;
            const char *map = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
            buf[n++] = (cp >= 0xc0 && cp <= 0xff) ? map[cp - 0xc0] : '?'; }
    }
    buf[n] = 0;
    if (centered) x -= (float)(n * 8) / 2.f;
    SDL_SetRenderDrawColor(g->ren, 255, 255, 255, 255);
    SDL_RenderDebugText(g->ren, x, y, buf);
}

/* TForm1.texto (0x48e720): Comic Sans MS at `size` points with a black shadow at
 * (+1,+1). Until a bitmap font is produced we scale SDL's 8 px debug font by
 * size/6 (roughly points -> pixels at 96 dpi). */
void osc_text_ex(osc_game *g, float x, float y, const char *s, char align, int size, Uint8 r, Uint8 gg, Uint8 b)
{
    if (!s || !*s) return;
    if (osc_font_ready()) {
        float w = osc_font_width(s, size);
        if (align == 'c') x -= w / 2.f;
        else if (align == 'e') x -= w;
        osc_font_draw(g->ren, x, y, s, size, r, gg, b);
        return;
    }
    char buf[160];
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p && n < sizeof buf - 1; p++) {
        if (*p < 0x80) buf[n++] = (char)*p;
        else if ((*p & 0xe0) == 0xc0 && p[1]) { unsigned cp = ((*p & 0x1f) << 6) | (p[1] & 0x3f); p++;
            const char *map = "AAAAAAACEEEEIIIIDNOOOOOxOUUUUYTsaaaaaaaceeeeiiiidnooooo/ouuuuyty";
            buf[n++] = (cp >= 0xc0 && cp <= 0xff) ? map[cp - 0xc0] : '?'; }
    }
    buf[n] = 0;
    /* Comic Sans at `size` pt is about size*1.3 px tall and size*0.8 px per letter */
    float sx = (float)size / 10.f, sy = (float)size / 8.f;
    if (sx < 0.7f) sx = 0.7f;
    if (sy < 0.9f) sy = 0.9f;
    float w = (float)n * 8.f * sx;
    if (align == 'c') x -= w / 2.f;
    else if (align == 'e') x -= w;
    SDL_SetRenderScale(g->ren, sx, sy);
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderDebugText(g->ren, (x + 1.f) / sx, (y + 1.f) / sy, buf);
    SDL_SetRenderDrawColor(g->ren, r, gg, b, 255);
    SDL_RenderDebugText(g->ren, x / sx, y / sy, buf);
    SDL_SetRenderScale(g->ren, 1.f, 1.f);
}
