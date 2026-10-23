/* TForm1 canvas primitives (barra, retangulo, barraredonda, circulopintado, texto)
 * shared by the fight HUD and the menu screens. */
#ifndef OSC_CANVAS_H
#define OSC_CANVAS_H

#include <math.h>
#include <stdlib.h>
#include "game.h"

/* TForm1 canvas helpers (0x48e444..0x48e720), GDI semantics: rectangles exclude
 * their right and bottom edge; RoundRect corners are ellipses of diameter `d`. */
typedef struct { Uint8 r, g, b; } osc_rgb;
static inline osc_rgb rgb(int r, int g, int b) { return (osc_rgb){(Uint8)r, (Uint8)g, (Uint8)b}; }   /* Delphi bytes wrap */
static const osc_rgb k_white = {255, 255, 255}, k_black = {0, 0, 0}, k_grey = {170, 170, 170};

static inline void shape(osc_game *g, int x1, int y1, int x2, int y2, double rx, double ry, osc_rgb c)
{
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    int w = x2 - x1, h = y2 - y1;
    if (w <= 0 || h <= 0) return;
    if (rx > w / 2.0) rx = w / 2.0;
    if (ry > h / 2.0) ry = h / 2.0;
    SDL_SetRenderDrawColor(g->ren, c.r, c.g, c.b, 255);
    for (int j = 0; j < h; j++) {
        double yc = j + 0.5, dy = 0.0, in = 0.0;
        if (yc < ry) dy = ry - yc;
        else if (yc > h - ry) dy = yc - (h - ry);
        if (dy > 0.0 && ry > 0.0) { double t = 1.0 - (dy / ry) * (dy / ry); in = rx - rx * sqrt(t > 0.0 ? t : 0.0); }
        int l = x1 + (int)floor(in + 0.5), r = x2 - (int)floor(in + 0.5);
        if (r > l) { SDL_FRect row = {(float)l, (float)(y1 + j), (float)(r - l), 1.f}; SDL_RenderFillRect(g->ren, &row); }
    }
}
static inline void barra(osc_game *g, int x1, int y1, int x2, int y2, osc_rgb c) { shape(g, x1, y1, x2, y2, 0, 0, c); }
static inline void retangulo(osc_game *g, int x1, int y1, int x2, int y2, osc_rgb c)
{
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    SDL_SetRenderDrawColor(g->ren, c.r, c.g, c.b, 255);
    if (x2 - x1 < 1 || y2 - y1 < 1) {
        /* Windows FrameRect still draws a degenerate rect: a column at x1 and one at x2 - 1 */
        SDL_FRect l = {(float)x1, (float)y1, 1.f, (float)(y2 - y1)}, r = {(float)(x2 - 1), (float)y1, 1.f, (float)(y2 - y1)};
        if (y2 > y1) { SDL_RenderFillRect(g->ren, &l); SDL_RenderFillRect(g->ren, &r); }
        if (x2 > x1) {
            SDL_FRect t = {(float)x1, (float)y1, (float)(x2 - x1), 1.f}, b = {(float)x1, (float)(y2 - 1), (float)(x2 - x1), 1.f};
            SDL_RenderFillRect(g->ren, &t); SDL_RenderFillRect(g->ren, &b);
        }
        return;
    }
    SDL_FRect rc = {(float)x1, (float)y1, (float)(x2 - x1), (float)(y2 - y1)};
    SDL_RenderRect(g->ren, &rc);
}
static inline void barraredonda(osc_game *g, int x1, int y1, int x2, int y2, int d, osc_rgb pen, osc_rgb brush)
{
    shape(g, x1, y1, x2, y2, d / 2.0, d / 2.0, pen);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    shape(g, x1 + sx, y1 + sy, x2 - sx, y2 - sy, d / 2.0 - 1.0, d / 2.0 - 1.0, brush);
}
static inline void circulopintado(osc_game *g, int x1, int y1, int x2, int y2, osc_rgb pen, osc_rgb brush)
{
    double rx = abs(x2 - x1) / 2.0, ry = abs(y2 - y1) / 2.0;
    shape(g, x1, y1, x2, y2, rx, ry, pen);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    shape(g, x1 + sx, y1 + sy, x2 - sx, y2 - sy, rx - 1.0, ry - 1.0, brush);
}
static inline void texto(osc_game *g, int x, int y, const char *s, char align, int size, osc_rgb c)
{
    osc_text_ex(g, (float)x, (float)y, s, align, size, c.r, c.g, c.b);
}

/* TPictureCollectionItem.Draw / DrawAlpha (0x46161c / 0x4616c0) */
static inline void draw_tex(osc_game *g, SDL_Texture *t, float x, float y, int alpha, bool flip)
{
    float w, h;
    if (!t || !SDL_GetTextureSize(t, &w, &h)) return;
    SDL_FRect d = {x, y, w, h};
    SDL_SetTextureAlphaMod(t, (Uint8)alpha);
    SDL_RenderTextureRotated(g->ren, t, NULL, &d, 0.0, NULL, flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureAlphaMod(t, 255);
}
static inline void draw_tex_rect(osc_game *g, SDL_Texture *t, int l, int tp, int r, int b, int alpha)
{
    if (!t) return;
    SDL_FRect d = {(float)l, (float)tp, (float)(r - l), (float)(b - tp)};
    SDL_SetTextureAlphaMod(t, (Uint8)alpha);
    SDL_RenderTexture(g->ren, t, NULL, &d);
    SDL_SetTextureAlphaMod(t, 255);
}

#endif
