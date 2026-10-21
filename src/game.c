#include "game.h"

#include <stdio.h>
#include <string.h>

/* Per-character constants from the spec (02 / 03): sprite height and speed.
 * Index 1..15; 0 unused. */
static const int k_sprite_h[16] = {0, 156, 162, 147, 149, 144, 190, 160, 160, 150, 155, 155, 165, 155, 160, 165};
static const double k_speed[16] = {0, 8, 8, 8.5, 8, 8, 7.5, 7.5, 8, 8.5, 7.5, 8, 8, 7.5, 7.5, 8};

/* Horizontal draw offset per frame for Chaves (docs/spec/05c-frame-offsets.txt).
 * Other characters will be loaded from the generated tables later. */
static const int k_offset_c01[50] = {
    0, 33, 27, 27, 30, 28, 22, 21, 29, 25, 18, 8, 40, 39, 38, 42, 43, 42, 41, 30, 25, 40, 40, 50, 30,
    40, 30, 40, 40, 55, 35, 20, 40, 40, 15, 48, 80, 48, 38, 40, 55, 42, 40, 60, 60, 60, 50, 40, 30, 30};

static int frame_offset(const osc_fighter *f)
{
    if (f->char_id == 1 && f->frame > 0 && f->frame < 50) return k_offset_c01[f->frame];
    return 0;
}

static bool load_path(char *dst, size_t n, const char *dir, const char *rel)
{
    int r = snprintf(dst, n, "%s/%s", dir, rel);
    return r > 0 && (size_t)r < n;
}

bool osc_game_init(osc_game *g, SDL_Renderer *ren, const char *data_dir)
{
    memset(g, 0, sizeof *g);
    g->ren = ren;
    SDL_strlcpy(g->data_dir, data_dir, sizeof g->data_dir);

    char path[1200];
    osc_image img;

    /* Stage 1 (Vila do Chaves). The list item is opaque: no colour key. */
    load_path(path, sizeof path, data_dir, "Cenarios/Cenario-01.bmp");
    if (!osc_bmp_load(path, false, &img)) return false;
    g->stage = osc_texture_from_image(ren, &img);
    g->stage_w = img.w;
    g->stage_h = img.h;
    osc_image_free(&img);
    if (!g->stage) return false;

    /* Chaves strip, sliced like carregarlutador (03-assets.md). */
    load_path(path, sizeof path, data_dir, "Lutadores/C01.bmp");
    if (!osc_bmp_load(path, true, &img)) return false;
    bool ok = osc_strip_build(ren, &img, &g->strip);
    osc_image_free(&img);
    if (!ok) return false;
    SDL_Log("C01.bmp: %d frames, height %d", g->strip.count, g->strip.h);

    /* zerardados: P1 at x = 220, feet on the ground, facing right, idle. */
    g->p1.x = 220.0;
    g->p1.y = OSC_GROUND_Y;
    g->p1.char_id = 1;
    g->p1.state = 1;
    g->p1.frame = 1;
    g->p1.facing = 1;
    g->p1.sprite_h = k_sprite_h[1];
    g->p1.speed = k_speed[1];
    g->cam_x = 100; /* round init: g_camX = 100 */
    g->cam_y = 0;

    /* Music 1, looped (DXWaveListMusica item 0). Optional. */
    load_path(path, sizeof path, data_dir, "Musicas/Musica-01.wav");
    if (osc_audio_open() && osc_wav_load(path, &g->music)) {
        g->has_music = true;
        osc_audio_play(&g->music, true);
    }
    return true;
}

/* Frame selector for the common states (05a, "char 0" block):
 *   state 1: frame 1
 *   state 2: timer 0 -> 2, 4 -> 3, 8 -> 2, 12 -> 1; timer wraps after 16 */
static void select_frame(osc_fighter *f)
{
    if (f->state == 1) {
        f->frame = 1;
    } else if (f->state == 2) {
        if (f->anim_timer == 0) f->frame = 2;
        else if (f->anim_timer == 4) f->frame = 3;
        else if (f->anim_timer == 8) f->frame = 2;
        else if (f->anim_timer == 12) f->frame = 1;
    }
}

void osc_game_tick(osc_game *g, const bool *keys)
{
    osc_fighter *f = &g->p1;
    g->tick++;
    if (g->paused) return;

    bool left = keys[SDL_SCANCODE_LEFT];
    bool right = keys[SDL_SCANCODE_RIGHT];

    /* Input dispatch (05): left/right -> move 1/2 (state 2), release -> state 1. */
    if (left || right) {
        if (f->state == 1) {
            f->state = 2;
            f->anim_timer = 0;
        }
        f->x += right ? f->speed : -f->speed;
        /* playable band: camX + 30 .. camX + 370 (sub_47648c) */
        if (f->x < g->cam_x + 30) f->x = g->cam_x + 30;
        if (f->x > g->cam_x + 370) f->x = g->cam_x + 370;
    } else if (f->state == 2) {
        f->state = 1;
        f->anim_timer = 0;
    }

    select_frame(f);

    /* setMove tail: in state 2 the timer counts 0..16 and wraps. */
    if (f->state == 2) {
        f->anim_timer++;
        if (f->anim_timer > 16) f->anim_timer = 0;
    }

    /* Camera follows the fighter (single-player stand-in for the midpoint rule),
     * clamped to the stage like the original (0..200 for a 600 px stage). */
    int max_cam = g->stage_w - OSC_SCREEN_W;
    if (max_cam < 0) max_cam = 0;
    g->cam_x = (int)(f->x + 0.5) - 200;
    if (g->cam_x < 0) g->cam_x = 0;
    if (g->cam_x > max_cam) g->cam_x = max_cam;

    osc_audio_update();
}

void osc_game_render(osc_game *g)
{
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderClear(g->ren);

    if (g->stage) {
        SDL_FRect dst = {(float)-g->cam_x, (float)-g->cam_y, (float)g->stage_w, (float)g->stage_h};
        SDL_RenderTexture(g->ren, g->stage, NULL, &dst);
    }

    const osc_fighter *f = &g->p1;
    if (f->frame >= 1 && f->frame <= g->strip.count && g->strip.tex[f->frame]) {
        int w = g->strip.w[f->frame];
        int off = frame_offset(f);
        /* desenhalutador: facing right draws at x - offset, facing left at
         * x + offset - width (mirrored); y = feet - spriteH. */
        float dx = f->facing == 1 ? (float)(f->x - off - g->cam_x) : (float)(f->x + off - w - g->cam_x);
        float dy = (float)(f->y - f->sprite_h - g->cam_y);
        SDL_FRect dst = {dx, dy, (float)w, (float)g->strip.h};
        SDL_RenderTextureRotated(g->ren, g->strip.tex[f->frame], NULL, &dst, 0.0, NULL,
                                 f->facing == 1 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
    }
}

void osc_game_shutdown(osc_game *g)
{
    if (g->has_music) osc_wav_free(&g->music);
    osc_audio_close();
    osc_strip_free(&g->strip);
    if (g->stage) SDL_DestroyTexture(g->stage);
    memset(g, 0, sizeof *g);
}
