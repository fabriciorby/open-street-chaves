#include "strip.h"

#include <string.h>

static bool dark(const uint8_t *px) { return px[0] < 101 && px[1] < 101 && px[2] < 101; }

SDL_Texture *osc_texture_from_image(SDL_Renderer *r, const osc_image *img)
{
    SDL_Texture *t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, img->w, img->h);
    if (!t) return NULL;
    SDL_UpdateTexture(t, NULL, img->rgba, img->w * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    return t;
}

static SDL_Texture *sub_texture(SDL_Renderer *r, const osc_image *img, int x0, int x1)
{
    int w = x1 - x0 + 1;
    if (w <= 0) return NULL;
    SDL_Texture *t = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, img->h);
    if (!t) return NULL;
    SDL_UpdateTexture(t, NULL, osc_image_px(img, x0, 0), img->w * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
    return t;
}

bool osc_strip_build(SDL_Renderer *r, const osc_image *img, osc_strip *out)
{
    memset(out, 0, sizeof *out);
    out->h = img->h;
    int prev = 0;
    int n = 1;
    for (int x = 0; x < img->w && n < OSC_MAX_FRAMES; x++) {
        if (dark(osc_image_px(img, x, 0)) && dark(osc_image_px(img, x, img->h - 1))) {
            /* copiabmp(slot n, x1 = prev, x2 = x - 1): frame is columns prev..x-1 */
            out->tex[n] = sub_texture(r, img, prev, x - 1);
            out->w[n] = x - prev;
            if (out->tex[n]) n++;
            prev = x + 1;
        }
    }
    out->count = n - 1;
    return out->count > 0;
}

void osc_strip_free(osc_strip *s)
{
    for (int i = 1; i <= s->count; i++)
        if (s->tex[i]) SDL_DestroyTexture(s->tex[i]);
    memset(s, 0, sizeof *s);
}
