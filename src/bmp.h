/* BMP loading for Street Chaves assets.
 *
 * The original game uses uncompressed 24-bit (and a few 8-bit) Windows BMPs.
 * DelphiX draws every sprite with TransparentColor = white, so pure white
 * (255,255,255) becomes transparent when `colorkey` is set (see 03-assets.md).
 */
#ifndef OSC_BMP_H
#define OSC_BMP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int w, h;
    uint8_t *rgba; /* w*h*4, top-down */
} osc_image;

/* Load a BMP file into RGBA. Returns false and leaves *out zeroed on error. */
bool osc_bmp_load(const char *path, bool colorkey, osc_image *out);
void osc_image_free(osc_image *img);

/* Pixel helpers (no bounds checks). */
static inline const uint8_t *osc_image_px(const osc_image *img, int x, int y)
{
    return img->rgba + ((size_t)y * (size_t)img->w + (size_t)x) * 4;
}

#endif
