/* Loading of the game's files (FormCreate and the per-fight loads): shared path
 * join, BMP -> texture, sounds, music, fighter strips and stages. */
#include <stdio.h>
#include <string.h>

#include "assets.h"
#include "exeimg.h"
#include "exetext.h"
#include "font.h"

bool load_path(char *dst, size_t n, const char *dir, const char *rel)
{
    int r = snprintf(dst, n, "%s/%s", dir, rel);
    return r > 0 && (size_t)r < n;
}

bool load_image_tex(osc_game *g, const char *rel, bool key, SDL_Texture **tex, int *w, int *h)
{
    char path[1200];
    osc_image img;
    load_path(path, sizeof path, g->data_dir, rel);
    if (!osc_bmp_load(path, key, &img)) return false;
    *tex = osc_texture_from_image(g->ren, &img);
    if (w) *w = img.w;
    if (h) *h = img.h;
    osc_image_free(&img);
    return *tex != NULL;
}

void osc_play_music(osc_game *g, int n)
{
    char rel[64], path[1200];
    osc_audio_stop_music();
    if (g->has_music) { osc_wav_free(&g->music); g->has_music = false; }
    snprintf(rel, sizeof rel, "Musicas/Musica-%02d.wav", n);
    load_path(path, sizeof path, g->data_dir, rel);
    if (osc_audio_open() && osc_wav_load(path, &g->music)) { g->has_music = true; osc_audio_music(&g->music); }
}

/* ---- helpers for the ending scripts (endings.c) ---------------------------- */
bool osc_load_fighter(osc_game *g, int slot, int ch)
{
    char rel[64], path[1200];
    osc_image img;
    osc_strip_free(&g->strips[slot - 1]);
    snprintf(rel, sizeof rel, "Lutadores/C%02d.bmp", ch);
    load_path(path, sizeof path, g->data_dir, rel);
    if (!osc_bmp_load(path, true, &img)) return false;
    bool ok = osc_strip_build(g->ren, &img, &g->strips[slot - 1]);
    osc_image_free(&img);
    if (!ok) return false;
    osc_fighter *f = &g->p[slot];
    f->char_id = ch; f->strip = &g->strips[slot - 1];
    f->facing = slot == 1 ? 1 : 2; f->rot = 0; f->state = 1; f->anim_timer = 0;
    return true;
}

bool osc_load_stage(osc_game *g, int stage)
{
    char rel[64];
    if (g->stage) { SDL_DestroyTexture(g->stage); g->stage = NULL; }
    snprintf(rel, sizeof rel, "Cenarios/Cenario-%02d.bmp", stage);
    g->stage_id = stage;
    /* carregarcenario: stage 2 has a breakable thing (Icone[6]) */
    g->crate.icon = 0;
    if (stage == 2) { g->crate.x = 395; g->crate.y = 195; g->crate.r0 = 25; g->crate.b0 = 25; g->crate.r1 = 110; g->crate.b1 = 90; g->crate.icon = 6; }
    return load_image_tex(g, rel, false, &g->stage, &g->stage_w, &g->stage_h);
}

/* Common assets: effects, stage animations, faces, sounds, text (FormCreate). */
bool osc_assets_load(osc_game *g)
{
    char path[1200], rel[64];

    for (int n = 1; n < OSC_MAX_FX_SPRITES; n++) {
        snprintf(rel, sizeof rel, "Efeitos/Efeito-%03d.bmp", n);
        if (!load_image_tex(g, rel, true, &g->fx_tex[n - 1], &g->fx_w[n - 1], &g->fx_h[n - 1])) break;
        g->n_fx_tex = n;
    }
    if (g->n_fx_tex == 0) return false;
    for (int n = 1; n < OSC_MAX_FX_SPRITES; n++) {
        snprintf(rel, sizeof rel, "Animacoes/Anim-%02d.bmp", n);
        if (!load_image_tex(g, rel, true, &g->anim_tex[n - 1], &g->anim_w[n - 1], &g->anim_h[n - 1])) break;
        g->n_anim_tex = n;
    }
    for (int c = 1; c <= 15; c++) {
        snprintf(rel, sizeof rel, "Rostos/R%02d-a.bmp", c);
        load_image_tex(g, rel, true, &g->face_small[c], NULL, NULL);
        snprintf(rel, sizeof rel, "Rostos/R%02d-b.bmp", c);
        load_image_tex(g, rel, true, &g->face_hurt[c], NULL, NULL);
        snprintf(rel, sizeof rel, "Rostos/R%02d-c.bmp", c);
        load_image_tex(g, rel, true, &g->face_big[c], &g->face_big_w[c], &g->face_big_h[c]);
    }
    SDL_Log("%d effect sprites, %d stage animation sprites loaded", g->n_fx_tex, g->n_anim_tex);

    if (osc_audio_open()) {
        for (int n = 1; n <= 80; n++) {
            snprintf(rel, sizeof rel, "Sons/Som-%02d.wav", n);
            load_path(path, sizeof path, g->data_dir, rel);
            if (!osc_wav_load(path, &g->sounds[g->n_sounds])) break;
            g->n_sounds++;
        }
        SDL_Log("%d sounds loaded", g->n_sounds);
        for (int c = 1; c <= 15; c++)
            for (int k = 0; k < 2; k++) {
                snprintf(rel, sizeof rel, "Falas/Fala-%02d-%c.wav", c, 'a' + k);
                load_path(path, sizeof path, g->data_dir, rel);
                osc_wav_load(path, &g->voices[c][k]);
            }
    }
    osc_text_load(g->data_dir);
    /* intro pictures and icons live inside Chaves.exe (03-assets.md) */
    if (osc_exeimg_load(g->data_dir)) {
        g->n_abertura = osc_exeimg_count("Abertura"); if (g->n_abertura > 16) g->n_abertura = 16;
        for (int i = 0; i < g->n_abertura; i++) {
            osc_image img;
            if (!osc_exeimg_get("Abertura", i, &img)) continue;
            g->abertura[i] = osc_texture_from_image(g->ren, &img); g->abertura_w[i] = img.w; g->abertura_h[i] = img.h;
            osc_image_free(&img);
        }
        g->n_icone = osc_exeimg_count("Icone"); if (g->n_icone > 14) g->n_icone = 14;
        for (int i = 0; i < g->n_icone; i++) {
            osc_image img;
            if (!osc_exeimg_get("Icone", i, &img)) continue;
            g->icone[i] = osc_texture_from_image(g->ren, &img); g->icone_w[i] = img.w; g->icone_h[i] = img.h;
            osc_image_free(&img);
        }
        g->n_final = osc_exeimg_count("Final"); if (g->n_final > 11) g->n_final = 11;
        for (int i = 0; i < g->n_final; i++) {
            osc_image img;
            if (!osc_exeimg_get("Final", i, &img)) continue;
            g->final_tex[i] = osc_texture_from_image(g->ren, &img); g->final_w[i] = img.w; g->final_h[i] = img.h;
            osc_image_free(&img);
        }
        osc_exeimg_free();
    }
    return true;
}

void osc_assets_free(osc_game *g)
{
    osc_audio_stop_music();
    if (g->has_music) osc_wav_free(&g->music);
    for (int i = 0; i < g->n_sounds; i++) osc_wav_free(&g->sounds[i]);
    for (int c = 0; c < 16; c++) for (int k = 0; k < 2; k++) osc_wav_free(&g->voices[c][k]);
    osc_text_free();
    osc_font_quit();
    osc_audio_close();
    for (int i = 0; i < 2; i++) osc_strip_free(&g->strips[i]);
    for (int i = 0; i < g->n_fx_tex; i++) if (g->fx_tex[i]) SDL_DestroyTexture(g->fx_tex[i]);
    if (g->stage) SDL_DestroyTexture(g->stage);
    for (int i = 0; i < 16; i++) if (g->abertura[i]) SDL_DestroyTexture(g->abertura[i]);
    for (int i = 0; i < 14; i++) if (g->icone[i]) SDL_DestroyTexture(g->icone[i]);
    for (int i = 0; i < 11; i++) if (g->final_tex[i]) SDL_DestroyTexture(g->final_tex[i]);
}
