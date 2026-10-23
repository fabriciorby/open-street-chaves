#include "font.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

#define MAX_SIZES 32

typedef struct {
    int size;                /* points */
    float scale, ascent;
    SDL_Texture *tex;
    stbtt_packedchar ascii[95];     /* 32..126 */
    stbtt_packedchar latin1[96];    /* 160..255 */
} font_size;

static unsigned char *g_ttf;
static stbtt_fontinfo g_info;
static font_size g_sizes[MAX_SIZES];
static int g_n_sizes;
static SDL_Renderer *g_ren;
static char g_name[128];

static bool try_load(const char *path, const char *label)
{
    if (!path || !*path) return false;
    size_t len = 0;
    unsigned char *d = SDL_LoadFile(path, &len);
    if (!d) return false;
    if (!stbtt_InitFont(&g_info, d, stbtt_GetFontOffsetForIndex(d, 0))) { SDL_free(d); return false; }
    g_ttf = d;
    SDL_strlcpy(g_name, label, sizeof g_name);
    SDL_Log("font: %s (%s)", label, path);
    return true;
}

bool osc_font_init(SDL_Renderer *ren)
{
    g_ren = ren;
    if (g_ttf) return true;
    char buf[1200];
    /* the real thing, when the system has it */
#if defined(_WIN32)
    const char *windir = SDL_getenv("WINDIR");
    snprintf(buf, sizeof buf, "%s\\Fonts\\comic.ttf", windir ? windir : "C:\\Windows");
    if (try_load(buf, "Comic Sans MS")) return true;
#elif defined(__APPLE__)
    if (try_load("/System/Library/Fonts/Supplemental/Comic Sans MS.ttf", "Comic Sans MS")) return true;
    if (try_load("/Library/Fonts/Comic Sans MS.ttf", "Comic Sans MS")) return true;
    const char *home = SDL_getenv("HOME");
    if (home) { snprintf(buf, sizeof buf, "%s/Library/Fonts/Comic Sans MS.ttf", home); if (try_load(buf, "Comic Sans MS")) return true; }
#else
    if (try_load("/usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS.ttf", "Comic Sans MS")) return true;
    if (try_load("/usr/share/fonts/truetype/msttcorefonts/comic.ttf", "Comic Sans MS")) return true;
#endif
    /* bundled OFL, metric-compatible with Comic Sans MS (Comic Relief) */
    const char *base = SDL_GetBasePath();
    snprintf(buf, sizeof buf, "%sassets/fonts/ComicRelief-Regular.ttf", base ? base : "");
    if (try_load(buf, "Comic Relief")) return true;
    if (try_load("assets/fonts/ComicRelief-Regular.ttf", "Comic Relief")) return true;
    if (try_load("/fonts/ComicRelief-Regular.ttf", "Comic Relief")) return true;   /* web build (embedded) */
    SDL_Log("font: no TrueType font found, using the debug font");
    return false;
}

bool osc_font_ready(void) { return g_ttf != NULL; }
const char *osc_font_name(void) { return g_name; }

void osc_font_quit(void)
{
    for (int i = 0; i < g_n_sizes; i++) if (g_sizes[i].tex) SDL_DestroyTexture(g_sizes[i].tex);
    g_n_sizes = 0;
    SDL_free(g_ttf); g_ttf = NULL;
}

static font_size *get_size(int size)
{
    for (int i = 0; i < g_n_sizes; i++) if (g_sizes[i].size == size) return &g_sizes[i];
    if (g_n_sizes == MAX_SIZES) return &g_sizes[0];
    font_size *f = &g_sizes[g_n_sizes];
    memset(f, 0, sizeof *f);
    f->size = size;
    /* TFont.Size in points at 96 dpi: em height = size * 96 / 72 px, a whole number of pixels in GDI */
    float px = floorf((float)size * 96.f / 72.f + 0.5f);
    f->scale = stbtt_ScaleForMappingEmToPixels(&g_info, px);
    /* GDI's tmAscent comes from OS/2 usWinAscent (hhea only as a fallback), rounded to a pixel */
    int asc, desc, gap;
    stbtt_GetFontVMetrics(&g_info, &asc, &desc, &gap);
    {
        stbtt_uint32 os2 = stbtt__find_table(g_info.data, (stbtt_uint32)g_info.fontstart, "OS/2");
        if (os2) { int wa = ttUSHORT(g_info.data + os2 + 74); if (wa > 0) asc = wa; }   /* usWinAscent */
    }
    f->ascent = floorf((float)asc * f->scale + 0.5f);
    /* room for 191 glyphs, 1x (no oversampling: glyph pixels land on the game's pixel grid) */
    int ATLAS = 256;
    const int over = 1;
    while ((double)ATLAS * ATLAS < 191.0 * (1.3 * over * px) * (1.3 * over * px) * 0.6 && ATLAS < 2048) ATLAS *= 2;
    unsigned char *bmp = calloc((size_t)ATLAS * ATLAS, 1);
    if (!bmp) return NULL;
    stbtt_pack_context pc;
    if (!stbtt_PackBegin(&pc, bmp, ATLAS, ATLAS, 0, 1, NULL)) { free(bmp); return NULL; }
    /* PackFontRanges scales by (ascent - descent); feed it the height that yields our em scale */
    int hasc, hdesc, hgap;
    stbtt_GetFontVMetrics(&g_info, &hasc, &hdesc, &hgap);
    float h = f->scale * (float)(hasc - hdesc);
    stbtt_pack_range ranges[2] = {
        { h, 32, NULL, 95, f->ascii, 0, 0 },
        { h, 160, NULL, 96, f->latin1, 0, 0 },
    };
    if (!stbtt_PackFontRanges(&pc, g_ttf, 0, ranges, 2)) SDL_Log("font: size %d did not fit a %d px atlas", size, ATLAS);
    stbtt_PackEnd(&pc);
    /* GDI advances are whole pixels at these sizes */
    for (int i = 0; i < 95; i++) f->ascii[i].xadvance = floorf(f->ascii[i].xadvance + 0.5f);
    for (int i = 0; i < 96; i++) f->latin1[i].xadvance = floorf(f->latin1[i].xadvance + 0.5f);
    /* GDI draws these sizes without antialiasing: threshold the coverage */
    for (int i = 0; i < ATLAS * ATLAS; i++) bmp[i] = bmp[i] >= 128 ? 255 : 0;
    /* alpha -> RGBA (white) */
    Uint32 *rgba = malloc((size_t)ATLAS * ATLAS * 4);
    if (!rgba) { free(bmp); return NULL; }
    for (int i = 0; i < ATLAS * ATLAS; i++) rgba[i] = 0x00ffffffu | ((Uint32)bmp[i] << 24);
    free(bmp);
    SDL_Surface *s = SDL_CreateSurfaceFrom(ATLAS, ATLAS, SDL_PIXELFORMAT_ABGR8888, rgba, ATLAS * 4);
    f->tex = s ? SDL_CreateTextureFromSurface(g_ren, s) : NULL;
    if (s) SDL_DestroySurface(s);
    free(rgba);
    if (!f->tex) return NULL;
    SDL_SetTextureBlendMode(f->tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(f->tex, SDL_SCALEMODE_NEAREST);
    g_n_sizes++;
    return f;
}

static const stbtt_packedchar *glyph(const font_size *f, unsigned cp)
{
    if (cp >= 32 && cp <= 126) return &f->ascii[cp - 32];
    if (cp >= 160 && cp <= 255) return &f->latin1[cp - 160];
    return &f->ascii['?' - 32];
}

static unsigned next_cp(const unsigned char **p)
{
    unsigned c = **p;
    if (!c) return 0;
    (*p)++;
    if (c < 0x80) return c;
    if ((c & 0xe0) == 0xc0 && **p) { unsigned cp = ((c & 0x1f) << 6) | (**p & 0x3f); (*p)++; return cp; }
    if ((c & 0xf0) == 0xe0 && (*p)[0] && (*p)[1]) { unsigned cp = ((c & 0x0f) << 12) | (((*p)[0] & 0x3f) << 6) | ((*p)[1] & 0x3f); *p += 2; return cp; }
    return '?';
}

float osc_font_width(const char *utf8, int size)
{
    font_size *f = get_size(size);
    if (!f) return 0;
    float w = 0;
    const unsigned char *p = (const unsigned char *)utf8;
    for (unsigned cp; (cp = next_cp(&p)) != 0;) w += glyph(f, cp)->xadvance;
    return w;
}

static void draw_run(font_size *f, float x, float y, const char *utf8)
{
    const unsigned char *p = (const unsigned char *)utf8;
    float pen = x, base = floorf(y + 0.5f) + f->ascent;
    for (unsigned cp; (cp = next_cp(&p)) != 0;) {
        const stbtt_packedchar *g = glyph(f, cp);
        float gx = floorf(pen + 0.5f);
        SDL_FRect src = {(float)g->x0, (float)g->y0, (float)(g->x1 - g->x0), (float)(g->y1 - g->y0)};
        SDL_FRect dst = {gx + g->xoff, base + g->yoff, g->xoff2 - g->xoff, g->yoff2 - g->yoff};
        SDL_RenderTexture(g_ren, f->tex, &src, &dst);
        pen += g->xadvance;
    }
}

void osc_font_draw(SDL_Renderer *ren, float x, float y, const char *utf8, int size, Uint8 r, Uint8 g, Uint8 b)
{
    g_ren = ren;
    font_size *f = get_size(size);
    if (!f) return;
    SDL_SetTextureColorMod(f->tex, 0, 0, 0);
    draw_run(f, x + 1, y + 1, utf8);
    SDL_SetTextureColorMod(f->tex, r, g, b);
    draw_run(f, x, y, utf8);
}
