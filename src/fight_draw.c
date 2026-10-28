/* Fight rendering: stage, animations, fighters, effects, super pause pattern. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "canvas.h"
#include "exetext.h"
#include "fight_internal.h"

/* ---- rendering ------------------------------------------------------------------------ */
static void draw_fighter(osc_game *g, const osc_fighter *f)
{
    const osc_strip *s = f->strip;
    /* shadow (Efeitos[0]), rising a little with the jump */
    if (g->assets.n_fx_tex > 0) {
        int sy = OSC_GROUND_Y - 12 - iround((OSC_GROUND_Y - iround(f->y)) / 12.0) - g->cam_y;
        draw_tex(g, g->assets.fx_tex[0], (float)(iround(f->x) - 35 - g->cam_x), (float)sy, 255, false);
    }
    if (f->frame < 1 || f->frame > s->count || !s->tex[f->frame]) return;
    int w = s->w[f->frame];
    int off = (f->frame < 64) ? osc_frame_offset[f->char_id][f->frame] : 0;
    float dx = f->facing == 1 ? (float)(f->x - off - g->cam_x) : (float)(f->x + off - w - g->cam_x);
    float dy = (float)(f->y - f->sprite_h - g->cam_y);
    SDL_FRect dst = {dx, dy, (float)w, (float)s->h};
    SDL_RenderTextureRotated(g->ren, s->tex[f->frame], NULL, &dst, (double)f->rot, NULL,
                             f->facing == 1 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
    if (g->flow.debug_boxes) {
        SDL_FRect hb = {(float)(f->x + f->hurt_l - g->cam_x), (float)(f->y - f->sprite_h + f->hurt_t - g->cam_y),
                        (float)(f->hurt_r - f->hurt_l), (float)(f->hurt_b - f->hurt_t)};
        SDL_SetRenderDrawColor(g->ren, 50, 50, 255, 255);
        SDL_RenderRect(g->ren, &hb);
        if (f->attacking) {
            SDL_FRect ab = {(float)(f->x + f->hit_l - g->cam_x), (float)(f->y - f->sprite_h + f->hit_t - g->cam_y),
                            (float)(f->hit_r - f->hit_l), (float)(f->hit_b - f->hit_t)};
            SDL_SetRenderDrawColor(g->ren, 255, 50, 50, 255);
            SDL_RenderRect(g->ren, &ab);
        }
    }
}

void osc_fx_render(osc_game *g)
{
    for (int i = 0; i < OSC_MAX_FX; i++) {
        const osc_fx_draw *d = &g->fx_draw[i];
        if (d->kind == 2) {        /* pickup popup: MemoTexto.Get(5), centred, size 10, green (0x48fc96) */
            texto(g, d->sx, d->sy, tx(g, 5), 'c', 10, rgb(80, 255, 140));
        } else if (d->kind == 1 && g->assets.fx_tex[d->sprite]) {
            float w = (float)g->assets.fx_w[d->sprite], h = (float)g->assets.fx_h[d->sprite];
            /* desenhaefeitos 0x48fb0e: x - Round(w / 2), y - Round(h / 2), or y - h with the f8 flag */
            float ox = (float)iround(w / 2.0), oy = d->bottom ? h : (float)iround(h / 2.0);
            SDL_FRect r = {(float)d->sx - ox, (float)d->sy - oy, w, h};
            SDL_RenderTextureRotated(g->ren, g->assets.fx_tex[d->sprite], NULL, &r, 0.0, NULL,
                                     d->dir == 2 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        }
        if (d->box) {
            SDL_FRect hb = {(float)d->bx, (float)d->by, (float)d->bw, (float)d->bh};
            SDL_SetRenderDrawColor(g->ren, 255, 200, 0, 255);
            SDL_RenderRect(g->ren, &hb);
        }
    }
}

/* hand-coded draws of desenhaanimacoes that are not table rows (world coords, minus camera) */
static void stage_anim_at(osc_game *g, int item, int x, int y)
{
    if (item >= g->assets.n_anim_tex || !g->assets.anim_tex[item]) return;
    SDL_FRect d = {(float)(x - g->cam_x), (float)(y - g->cam_y), (float)g->assets.anim_w[item], (float)g->assets.anim_h[item]};
    SDL_RenderTexture(g->ren, g->assets.anim_tex[item], NULL, &d);
}

/* called after each table row of the stage (whether or not that row was drawn): the rows
 * missing from 07c sit in the original right after these ones */
static void stage_anim_extra(osc_game *g, const osc_stage_anim_row *r)
{
    int c1 = g->p[1].char_id, c2 = g->p[2].char_id;
    if (r->stage_mask == 1024 && r->item == 38) {            /* 0x49c44c: Anim 39 */
        if (c1 != 11 && c2 != 11 && c1 != 2 && c2 != 2)
            stage_anim_at(g, 39, 150 + osc_random(g, 2), 138);
    } else if (r->stage_mask == 2048 && r->item == 75) {     /* 0x49c520: Anim 76, before Anim 77 */
        if (c1 != 14 && c2 != 14)
            stage_anim_at(g, 76, 120 + osc_random(g, 2), 82);
    } else if (r->stage_mask == 256 && r->item == 26) {      /* 0x49bf04: five flickering lights, last of stage 8 */
        for (int i = 1; i <= 5; i++) {
            int r3 = osc_random(g, 3), r2 = osc_random(g, 2);
            int base = iround((g->p[1].x + g->p[2].x) / 2.0) - 60;
            stage_anim_at(g, 18, base + 15 * i + r2, 269 - 4 * r3);
        }
    } else if (r->stage_mask == 4096 && r->item == 40 && r->ciclo_lo == 23) {   /* 0x49c7fa: Icone[13], every tick */
        if (13 < g->assets.n_icone && g->assets.icone[13])
            draw_tex(g, g->assets.icone[13], (float)(225 - g->cam_x),
                     (float)(134 - iround(-cos((g->ciclo - 11) / 5.0) * 70.0) - g->cam_y), 255, false);
    }
}

void osc_fight_render(osc_game *g)
{
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderClear(g->ren);
    if (g->sp_draw) {
        /* super pause: Icone[7] tiled and sliding instead of the stage (0x4a47a0) */
        if (7 < g->assets.n_icone && g->assets.icone[7])
            for (int i = 0; i < 6; i++)
                for (int j = 0; j < 5; j++)
                    draw_tex(g, g->assets.icone[7], (float)(100 * i - 80 + g->sp_off), (float)(100 * j), 255, false);
    } else {
        if (g->assets.stage) {
            SDL_FRect dst = {(float)-g->cam_x, (float)-g->cam_y, (float)g->assets.stage_w, (float)g->assets.stage_h};
            SDL_RenderTexture(g->ren, g->assets.stage, NULL, &dst);
        }
        /* stage animations (desenhaanimacoes, table 07c) */
        for (int i = 0; i < OSC_N_STAGE_ANIM; i++) {
            const osc_stage_anim_row *r = &osc_stage_anim[i];
            if (!(r->stage_mask & (1u << g->stage_id))) continue;
            if (g->ciclo >= r->ciclo_lo && g->ciclo <= r->ciclo_hi &&
                !(r->not_char && (g->p[1].char_id == r->not_char || g->p[2].char_id == r->not_char))) {
                SDL_Texture *t = r->is_effect ? (r->item < g->assets.n_fx_tex ? g->assets.fx_tex[r->item] : NULL)
                                              : (r->item < g->assets.n_anim_tex ? g->assets.anim_tex[r->item] : NULL);
                if (t) {
                    int w = r->is_effect ? g->assets.fx_w[r->item] : g->assets.anim_w[r->item];
                    int h = r->is_effect ? g->assets.fx_h[r->item] : g->assets.anim_h[r->item];
                    int ax = r->x + r->xmul * g->ciclo, ay = r->y - (r->yjit ? r->yjit * osc_random(g, 3) : 0);
                    SDL_FRect d = {(float)(ax - g->cam_x), (float)(ay - g->cam_y), (float)w, (float)h};
                    SDL_RenderTexture(g->ren, t, NULL, &d);
                }
            }
            stage_anim_extra(g, r);
        }
    }
    draw_hud_top(g);
    /* something crossing the street (Efeitos[0] shadow under Efeitos[9]) */
    if (g->mover.active && g->assets.n_fx_tex > 9) {
        draw_tex(g, g->assets.fx_tex[0], (float)(g->mover.x + 10 - g->cam_x), (float)(g->mover.y + 132 - g->cam_y), 255, false);
        draw_tex(g, g->assets.fx_tex[9], (float)(g->mover.x - g->cam_x), (float)(g->mover.y - g->cam_y), 255, false);
    }
    if (g->crate.icon && !g->super_pause && g->crate.icon < g->assets.n_icone && g->assets.icone[g->crate.icon])
        draw_tex(g, g->assets.icone[g->crate.icon], (float)(g->crate.x - g->cam_x), (float)(g->crate.y - g->cam_y), 255, false);
    /* the one who hit last is drawn on top */
    if (g->p[1].on_top) { draw_fighter(g, &g->p[2]); draw_fighter(g, &g->p[1]); }
    else { draw_fighter(g, &g->p[1]); draw_fighter(g, &g->p[2]); }
    osc_fx_render(g);
    draw_hud_bottom(g);
    draw_round_texts(g);
    draw_dialogue(g);
    if (g->flash_on) barra(g, 0, 0, 400, 300, rgb(255, 0, 0));
    if (g->flow.show_moves) osc_movelist_render(g);   /* 0x4aa31c, state 7 only */
    if (g->challenge > 0) {
        osc_rgb pink = rgb(250, 140, 140);
        barraredonda(g, 80, 100, 320, 170, 10, k_white, k_black);                   /* 0x4ae384 */
        texto(g, 200, 100, osc_text(OSC_MEMO_TEXTO, 9, g->settings.lang), 'c', 15, pink);    /* ESPERE ... */
        texto(g, 200, 120, osc_text(OSC_MEMO_TEXTO, 10, g->settings.lang), 'c', 15, pink);   /* ALGUÉM ENTROU */
        texto(g, 200, 140, osc_text(OSC_MEMO_TEXTO, 11, g->settings.lang), 'c', 15, pink);   /* NO COMBATE ! */
    }
}

void osc_draw_fighter_at(osc_game *g, int slot, int x, int y, int state, int timer)
{
    osc_fighter *f = &g->p[slot];
    if (!f->strip || !f->strip->count) return;
    f->x = x; f->y = y; f->state = state; f->anim_timer = timer; f->in_air = false; f->rot = 0;
    f->sprite_h = k_sprite_h[f->char_id];
    select_pose(g, f);
    draw_fighter(g, f);
}
