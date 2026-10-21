#include "bmp.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) { return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24)); }

bool osc_bmp_load(const char *path, bool colorkey, osc_image *out)
{
    memset(out, 0, sizeof *out);
    size_t len = 0;
    uint8_t *data = SDL_LoadFile(path, &len);
    if (!data) {
        SDL_Log("bmp: cannot read %s: %s", path, SDL_GetError());
        return false;
    }
    if (len < 54 || data[0] != 'B' || data[1] != 'M') {
        SDL_Log("bmp: %s is not a BMP", path);
        SDL_free(data);
        return false;
    }
    uint32_t off = rd32(data + 10);
    uint32_t hsz = rd32(data + 14);
    int32_t w = (int32_t)rd32(data + 18);
    int32_t h = (int32_t)rd32(data + 22);
    uint16_t bpp = rd16(data + 28);
    uint32_t comp = rd32(data + 30);
    uint32_t ncol = rd32(data + 46);
    bool bottom_up = h > 0;
    if (h < 0) h = -h;
    if (comp != 0 || (bpp != 24 && bpp != 8 && bpp != 32) || w <= 0 || h <= 0) {
        SDL_Log("bmp: unsupported format in %s (bpp=%u comp=%u)", path, bpp, comp);
        SDL_free(data);
        return false;
    }
    const uint8_t *pal = data + 14 + hsz;
    if (bpp == 8 && ncol == 0) ncol = 256;
    size_t stride = ((size_t)w * bpp / 8 + 3) & ~(size_t)3;
    if ((size_t)off + stride * (size_t)h > len) {
        SDL_Log("bmp: truncated file %s", path);
        SDL_free(data);
        return false;
    }
    out->w = w;
    out->h = h;
    out->rgba = malloc((size_t)w * (size_t)h * 4);
    if (!out->rgba) {
        SDL_free(data);
        return false;
    }
    for (int y = 0; y < h; y++) {
        const uint8_t *row = data + off + stride * (size_t)(bottom_up ? h - 1 - y : y);
        uint8_t *dst = out->rgba + (size_t)y * (size_t)w * 4;
        for (int x = 0; x < w; x++, dst += 4) {
            uint8_t r, g, b;
            if (bpp == 24) {
                b = row[x * 3]; g = row[x * 3 + 1]; r = row[x * 3 + 2];
            } else if (bpp == 32) {
                b = row[x * 4]; g = row[x * 4 + 1]; r = row[x * 4 + 2];
            } else {
                const uint8_t *c = pal + 4 * (row[x] < ncol ? row[x] : 0);
                b = c[0]; g = c[1]; r = c[2];
            }
            dst[0] = r; dst[1] = g; dst[2] = b;
            dst[3] = (colorkey && r == 255 && g == 255 && b == 255) ? 0 : 255;
        }
    }
    SDL_free(data);
    return true;
}

void osc_image_free(osc_image *img)
{
    free(img->rgba);
    img->rgba = NULL;
    img->w = img->h = 0;
}
