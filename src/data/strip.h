/* Fighter strip slicing (Lutadores/Cxx.bmp).
 *
 * The original `carregarlutador` walks the strip column by column: a column
 * whose top pixel AND bottom pixel both have R, G and B < 101 is a separator;
 * everything since the previous separator becomes the next frame (full strip
 * height). Frames are numbered from 1, matching the animation tables.
 */
#ifndef OSC_STRIP_H
#define OSC_STRIP_H

#include <SDL3/SDL.h>

#include "data/bmp.h"

#define OSC_MAX_FRAMES 64

typedef struct {
    int count;                       /* number of frames, frames[1..count] valid */
    SDL_Texture *tex[OSC_MAX_FRAMES];
    int w[OSC_MAX_FRAMES];
    int h;                           /* strip height */
} osc_strip;

/* Slice `img` and upload every frame as a texture. */
bool osc_strip_build(SDL_Renderer *r, const osc_image *img, osc_strip *out);
void osc_strip_free(osc_strip *s);

/* Upload a whole image as one texture (stages, single sprites). */
SDL_Texture *osc_texture_from_image(SDL_Renderer *r, const osc_image *img);

#endif
