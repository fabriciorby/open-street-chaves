#include "game.h"
#include "assets.h"
#include "canvas.h"
#include "config.h"
#include "arcade.h"
#include "unlocks.h"
#include "fight_internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "exetext.h"
#include "exeimg.h"
#include "font.h"
#include "tables_gen.h"

/* ---- Delphi System.Random -------------------------------------------------- */
int osc_random(osc_game *g, int n)
{
    g->rng = g->rng * 0x8088405u + 1u;
    if (n <= 0) return 0;
    return (int)(((uint64_t)(uint32_t)n * (uint64_t)g->rng) >> 32);
}

static void play_snd(osc_game *g, int idx)
{
    if (idx >= 0 && idx < g->n_sounds && g->sounds[idx].buf) osc_audio_play(&g->sounds[idx]);
}



/* FormCreate: the common assets, the default records, the settings and the unlock files. */
bool osc_game_init(osc_game *g, SDL_Renderer *ren, const char *data_dir)
{
    bool ai_p2 = g->ai_p2;
    int lang = g->lang;
    memset(g, 0, sizeof *g);
    g->ai_p2 = ai_p2;
    g->lang = lang;
    g->speed_delay = OSC_TICK_MS;   /* FormCreate: g_speedDelay = 28 */
    g->ren = ren;
    g->rng = (uint32_t)SDL_GetTicksNS();
    SDL_strlcpy(g->data_dir, data_dir, sizeof g->data_dir);
    if (!osc_assets_load(g)) return false;   /* no data folder */
    /* default high-score table (FormCreate: criarecorde 1..10) */
    {
        static const char *names[10] = {"CHV", "SMD", "CHQ", "QUI", "DFL", "PGR", "SBR", "B71", "PTY", "NHO"};
        for (int i = 0; i < 10; i++) {
            memcpy(g->records[i].name, names[i], 4);
            g->records[i].char_id = i + 1;
            g->records[i].score = i == 0 ? 10000 : 8500 - 500 * i;
        }
    }
    osc_font_init(ren);
    osc_config_load(g);
    g->screen = OSC_SCREEN_ATTRACT;
    g->screen_tick = 0;
    g->sel[1] = 1; g->sel[2] = 2;
    osc_unlocks_load(g);
    g->p1_in = false;   /* nobody has joined until a start button */
    g->p2_in = false;
    return true;
}

/* Round/fight init (state 6 -> 7). */
bool osc_fight_start(osc_game *g, int char1, int char2, int stage)
{
    char rel[64];
    osc_image img;
    char path[1200];
    for (int i = 0; i < 2; i++) osc_strip_free(&g->strips[i]);
    memset(g->fx, 0, sizeof g->fx);
    memset(g->fx_draw, 0, sizeof g->fx_draw);

    int chars[2] = {char1, char2};
    for (int i = 0; i < 2; i++) {
        snprintf(rel, sizeof rel, "Lutadores/C%02d.bmp", chars[i]);
        load_path(path, sizeof path, g->data_dir, rel);
        if (!osc_bmp_load(path, true, &img)) return false;
        bool ok = osc_strip_build(g->ren, &img, &g->strips[i]);
        osc_image_free(&img);
        if (!ok) return false;
        bool human = g->p[i + 1].human, had = g->p[i + 1].char_id != 0;
        int wins = g->p[i + 1].wins, score = g->p[i + 1].score;
        init_fighter(&g->p[i + 1], chars[i], i + 1, &g->strips[i]);
        g->p[i + 1].human = had ? human : (i == 0 || !g->ai_p2);
        g->p[i + 1].wins = wins; g->p[i + 1].score = score;
    }
    if (stage < 1 || stage > 12) {
        if (g->p[1].human && g->p[2].human) {                       /* 2P: Round(Random(120)/10)+1 */
            stage = iround(osc_random(g, 120) / 10.0) + 1;
            if (stage > 12) stage = 12;
            if (g->unlock_sc) stage = g->unlock_sc;
        } else stage = osc_stage_for(g, char2);
    }
    if (!osc_load_stage(g, stage)) return false;

    /* music: Chaves 4, Quico 2, Madruga 3 (CPU side), stage 12 -> 6, else Round(Random(60)/10)+1 */
    int music = 0;
    for (int i = 1; i <= 2; i++) {
        if (g->p[i].human) continue;
        if (g->p[i].char_id == 1) music = 4;
        if (g->p[i].char_id == 4) music = 2;
        if (g->p[i].char_id == 2) music = 3;
    }
    if (stage == 12) music = 6;
    if (!music) music = iround(osc_random(g, 6 * 10) / 10.0) + 1;   /* 6 = Musicas loaded */
    if (music > 6) music = 6;
    if (music == 6 && stage != 12) music = 1;
    if (g->demo == 0) osc_play_music(g, music);   /* the attract demo starts no music (0x49fdb1) */

    g->cam_x = 100; g->cam_y = 20;
    g->timer_sec = osc_round_time(g); g->timer_sub = 25;
    g->round_no = 1;
    if (g->fight_no < 1) g->fight_no = 1;
    g->round_start = 0; g->round_end = 0; g->no_push = true; g->hit_stop = 0; g->super_pause = 0;
    g->match_over = 0;
    g->p[1].wins = g->p[2].wins = 0;
    g->p[1].on_top = true; g->p[2].on_top = false;
    g->mover.active = false; g->mover.freq = 330;
    g->first_hit = false; g->dialogo = 0; g->shake = 0; g->flash = 0; g->flash_on = false;
    g->snd_pending = 0; g->sp_draw = false;
    g->p[1].ai_pref = g->p[2].ai_pref = 200;
    g->screen = OSC_SCREEN_FIGHT;
    g->screen_tick = 0;
    SDL_Log("fight %d: %s vs %s at %s", g->fight_no, osc_text(OSC_MEMO_NOMES, char1, g->lang),
            osc_text(OSC_MEMO_NOMES, char2, g->lang), osc_text(OSC_MEMO_CENARIOS, stage - 1, g->lang));
    return true;
}

void osc_fight_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    g->tick++;
    if (g->unlock_mentecapto) { g->hud_y = -90; g->hud_y2 = 40; }   /* top of DXTimer1Timer */
    if (g->demo > 0) { g->timer_sec = g->cfg_time_mode; g->timer_sub = 25; }   /* attract demo: g_timerA := g_cfg1_mode every tick (0x4aa026) */
    if (g->challenge > 0) {                                        /* "ALGUÉM ENTROU NO COMBATE !" */
        g->no_push = true;
        if (--g->challenge == 0) {
            g->screen = OSC_SCREEN_SELECT; g->screen_tick = 0; g->sel_sub = 0;
            g->sel_locked[1] = g->sel_locked[2] = 0; g->timer_sec = 20; g->timer_sub = 25;
            g->p[1].human = g->p1_in; g->p[2].human = g->p2_in; g->ai_p2 = !g->p2_in;
            g->sel[1] = g->p[1].char_id; g->sel[2] = g->p[2].char_id;
            g->no_push = false;
        }
        return;
    }
    g->p[1].in = in1; g->p[2].in = in2;
    /* round 1: both fighters jump in from the stage edges (0x4a44cb) */
    if (g->round_no == 1 && g->round_start == 0) {
        for (int i = 1; i <= 2; i++) {
            osc_fighter *f = &g->p[i];
            f->x = i == 1 ? 0.0 : 600.0;
            f->jump_dir = i == 1 ? 3 : 2;
            f->y = OSC_GROUND_Y; f->in_air = true; f->vy = -25; f->frame = 21;
            f->state = 12; f->anim_timer = 0; f->anim_dur = 99;
        }
        g->no_push = true;
        g->cam_y = iround((g->p[1].y + g->p[2].y) / 8.0) - 55;
    }
    /* the last fight against the CPU opens with a dialogue (0x4a4591) */
    if (g->fight_no == 8 && g->round_no == 1 && g->round_start == 1 && (!g->p[1].human || !g->p[2].human)) {
        g->dialogo = 600; g->round_start = 2; g->hud_y = -120; g->hud_y2 = 60;
    }
    if (g->dialogo > 0) g->dialogo--;
    /* the sound asked for last tick (g_sndIndexA) */
    if (g->snd_pending) { play_snd(g, g->snd_pending); g->snd_pending = 0; }
    /* super pause: the stage gives way to a scrolling pattern (0x4a4682) */
    g->sp_draw = g->super_pause > 0;
    if (g->super_pause > 0) {
        g->super_pause--;
        int sp = g->super_pause;
        g->sp_off = 0;
        if (sp % 2 == 0) g->sp_off = 20;
        if (sp % 3 == 0) g->sp_off = 40;
        if (sp % 4 == 0) g->sp_off = 60;
        if (sp % 5 == 0) g->sp_off = 80;
    }
    /* camera: midpoint of the fighters, the shake, then the stage limits (0x4a47f1) */
    if (!g->no_push) {
        g->cam_x = iround((g->p[1].x + g->p[2].x) / 2.0) - 200;
        g->cam_y = iround((g->p[1].y + g->p[2].y) / 8.0) - 55;
    }
    if (g->shake > 0) {
        g->shake--;
        g->cam_x = g->cam_x - 10 + osc_random(g, 20);
        g->cam_y = g->cam_y - 10 + osc_random(g, 20);
    }
    /* fixed limits of the original (0x4a47f1): x 0..200, y 0..20 */
    if (g->cam_x < 0) g->cam_x = 0; if (g->cam_x > 200) g->cam_x = 200;
    if (g->cam_y < 0) g->cam_y = 0; if (g->cam_y > 20) g->cam_y = 20;
    bool frozen = g->hit_stop || g->super_pause;
    if (g->demo > 0) {                                                     /* nobody falls in the demo (0x4a4cfe) */
        g->p[1].health = g->p[2].health = OSC_HEALTH_MAX;
        g->p[1].stocks = g->p[2].stocks = 3;
    }
    if (g->cfg_power == 2 || g->cfg_power == 4) g->p[1].stocks = 3;        /* "Poder" 1P MAX / MAX */
    if (g->cfg_power == 3 || g->cfg_power == 4) g->p[2].stocks = 3;
    for (int i = 1; i <= 2; i++) {
        osc_fighter *f = &g->p[i];
        /* meter 102 -> stock (main loop 4572) */
        if (f->stocks < 0) f->stocks = 0;
        if (f->meter >= OSC_METER_MAX && f->stocks < 3) { f->stocks++; f->meter = 0; play_snd(g, 13); }
        if (f->stocks >= 3) { f->meter = OSC_METER_MAX; f->stocks = 3; }
        /* health bars (HUD loop 0x4a4cbe): the red bar waits 20 ticks after a hit, then
         * falls 3 per tick to the health */
        if (g->unlock_energeticos && g->p[1].health > 0.0 && g->p[2].health > 0.0) f->health += 0.15;   /* g_energeticos */
        if (f->health < 0.0) f->health = 0.0;
        if (f->health > OSC_HEALTH_MAX) f->health = OSC_HEALTH_MAX;
        if (f->health_shown > OSC_HEALTH_MAX) f->health_shown = OSC_HEALTH_MAX;
        if (!g->super_pause && !g->hit_stop && f->trail_delay == 0) {
            if (f->health_shown > f->health) f->health_shown -= 3.0;
            if (f->health_shown < f->health) f->health_shown = f->health;
        }
        if (f->trail_delay > 0) f->trail_delay--;
        if (f->msg_timer > 0) f->msg_timer--;
        if (f->combo_timer > 15 && f->hits > 1) f->combo_jitter = osc_random(g, 10);
    }

    /* the clock (0x4a4c66) stops during pauses and while nobody may move */
    if (!frozen && !g->no_push) g->timer_sub--;
    if (g->timer_sub <= 0) { g->timer_sub = 25; if (g->timer_sec > 0) g->timer_sec--; }

    /* input (0x4a5a61): humans, then the sequences, then the CPU */
    if (!frozen && !g->challenge && !g->no_push)
        for (int i = 1; i <= 2; i++)
            if (g->p[i].human) input_dispatch(g, &g->p[i]);
    for (int i = 1; i <= 2; i++) sequences(g, &g->p[i]);
    if (!frozen && !g->challenge)
        for (int i = 1; i <= 2; i++) {
            osc_fighter *f = &g->p[i];
            if (f->human || g->no_push) continue;
            cpu_think(g, f);
        }
    for (int i = 1; i <= 2; i++) counters(g, &g->p[i]);
    stage_objects(g);
    /* fighters in drawing order: the one who hit last goes on top */
    if (g->p[1].on_top) { fighter_frame(g, &g->p[2]); fighter_frame(g, &g->p[1]); }
    else { fighter_frame(g, &g->p[1]); fighter_frame(g, &g->p[2]); }
    if (!frozen && !g->challenge && !g->no_push)
        for (int i = 1; i <= 2; i++) defend(g, &g->p[i]);
    update_fx(g);

    /* round intro (DXTimer1Timer 0x4a71d0): g_roundStart counts to 71, the bars
     * fill from tick 6, no input until tick 55 */
    if (g->round_start <= 70 && g->dialogo == 0) {   /* the dialogue holds the intro */
        g->round_start++;
        if (g->round_start < 10) g->no_push = true;
        g->hud_y = g->hud_y2 = 0;
        for (int i = 1; i <= 2; i++) {
            osc_fighter *f = &g->p[i];
            if (g->round_start < 5) f->health = f->health_shown = 1.0;
            if (g->round_start > 5 && g->round_start < 55 && f->health < OSC_HEALTH_MAX) f->health += 3.0;
        }
        if (g->round_start == 55) {
            g->no_push = false;
            g->p[1].health = g->p[2].health = OSC_HEALTH_MAX;
        }
    }

    /* time over (0x4a7005): the lower health drops to 0, a draw empties both */
    if (g->timer_sec == 0) {
        if (g->round_end < 30) { g->round_end = 30; play_snd(g, 4); }
        if (g->hud_y > -120) g->hud_y -= 10;
        if (g->hud_y2 < 490) g->hud_y2 += 10;
        g->no_push = true;
        double h1 = g->p[1].health, h2 = g->p[2].health;
        if (h1 < h2) g->p[1].health = 0.0;
        if (h2 < h1) g->p[2].health = 0.0;
        if (h1 == h2) g->p[1].health = g->p[2].health = 0.0;
    }

    /* round end (0x4a7496): runs once a fighter is down */
    if (g->p[1].health <= 0.0 || (g->p[2].health <= 0.0 && g->round_end < 230)) {
        bool calm = !g->hit_stop && !g->super_pause;
        if (calm) g->round_end++;
        if (g->round_end > 40) g->no_push = true;
        if (g->round_end == 1) { g->hit_stop = 30; g->round_end = 2; }
        if (g->p[1].health <= 0.0) { g->p[2].invuln = true; g->p[1].invuln = false; }
        if (g->p[2].health <= 0.0) { g->p[1].invuln = true; g->p[2].invuln = false; }
        if (g->round_end > 0 && g->round_end < 30) { g->hud_y -= 5; g->hud_y2 += 5; }
        /* the round's winner: win pose and bonuses */
        if (g->round_end > 30 && g->round_end < 90)
            for (int i = 1; i <= 2; i++) {
                osc_fighter *f = &g->p[i], *o = opp(g, f);
                if (f->health <= 0.0) continue;
                if (g->no_push) set_move(g, f, 14);
                calm = !g->hit_stop && !g->super_pause;
                if (f->human && f->health < OSC_HEALTH_MAX && g->round_end == 35 && calm) {
                    f->score += iround(f->health) * 5;   /* VITALIDADE x5 */
                    g->round_end++;
                }
                if (f->human && f->health == OSC_HEALTH_MAX && g->round_end == 45 && calm) {
                    f->score += 1000;                     /* PERFEITO */
                    g->round_end++;
                }
                if (f->human && f->wins == g->rounds_to_win && o->wins == 0 && g->round_end == 45 && calm)
                    f->score += 1000;                     /* COMPLETO */
            }
        if (g->round_end == 40) {
            for (int i = 1; i <= 2; i++) {
                osc_fighter *f = &g->p[i];
                if (f->health > 0.0 && !g->hit_stop && !g->super_pause) { f->wins++; g->round_end++; }
                g->transition = iround(osc_random(g, 40) / 10.0) + 1;
                if (g->transition > 4) g->transition = 4;
                f->quote = iround(osc_random(g, 30) / 10.0) + 1;
                if (f->quote > 3) f->quote = 3;
            }
        }
        /* the match winner: pose, big portrait and quote (0x4a7b5d) */
        for (int i = 1; i <= 2; i++) {
            osc_fighter *f = &g->p[i];
            if (f->wins < g->rounds_to_win || g->round_end < 90 || g->round_end > 210) continue;
            if (g->no_push) set_move(g, f, 14);
            /* holding the first button hurries the scene along */
            if ((f->in & OSC_IN_B1) && (g->round_end < 90 || g->round_end > 100)) g->round_end += 2;
            if (g->round_end == 95 && !g->hit_stop && !g->super_pause && f->char_id >= 1 && f->char_id <= 15) {
                /* the victory line, a or b: Round(Random(200) / 200) with banker's rounding */
                const osc_wav *v = &g->voices[f->char_id][osc_random(g, 200) > 100];
                if (v->buf) osc_audio_play(v);
            }
        }
        bool decided = g->p[1].wins >= g->rounds_to_win || g->p[2].wins >= g->rounds_to_win;
        if (!decided && g->round_end > 130) {
            /* next round (zerardados for both) */
            for (int i = 1; i <= 2; i++) {
                osc_fighter *f = &g->p[i];
                int wins = f->wins, ch = f->char_id, st = f->stocks, score = f->score, mc = f->max_combo;
                double meter = f->meter; bool human = f->human;
                const osc_strip *st_ = f->strip;
                osc_reset_fighter(f, ch, i, st_);
                f->wins = wins; f->stocks = st; f->score = score; f->human = human; f->meter = meter; f->max_combo = mc;
            }
            g->round_no++;
            g->round_end = 0; g->round_start = 0; g->hit_stop = 0; g->super_pause = 0;
            g->hud_y = g->hud_y2 = 0; g->first_hit = false;
            g->p[1].on_top = true; g->p[2].on_top = false; g->mover.active = false;
            g->timer_sec = osc_round_time(g); g->timer_sub = 25; g->cam_x = 100;
        }
        if (decided && g->round_end > 220) g->match_over = g->p[1].wins >= g->rounds_to_win ? 1 : 2;
    }

    /* body hits, then the clocks and the physics (0x4a85ec..0x4a99d8) */
    for (int i = 1; i <= 2; i++) melee_step(g, &g->p[i]);
    if (g->hit_stop > 0) g->hit_stop--;
    for (int i = 1; i <= 2; i++) move_clock(g, &g->p[i]);
    for (int i = 1; i <= 2; i++) {
        osc_fighter *f = &g->p[i];
        if (f->y > OSC_GROUND_Y) {
            f->y = OSC_GROUND_Y; f->rot = 0; f->rot_step = 0; f->vy = 0; f->in_air = false;
            f->state = 1; f->anim_timer = 0; f->attacking = false; f->knockdown = false;
        }
    }
    for (int i = 1; i <= 2; i++) physics(g, &g->p[i]);
    /* dialogue skip: the human's first button hurries it (0x4a9f83) */
    if (g->dialogo > 0)
        for (int i = 1; i <= 2; i++)
            if (g->p[i].human && (g->p[i].in & OSC_IN_B1) && g->dialogo > 10) g->dialogo -= 5;
    for (int i = 1; i <= 2; i++) g->p[i].in_prev = g->p[i].in;
    if (!g->super_pause && !g->hit_stop) g->ciclo++;
    if (g->ciclo > 30) g->ciclo = 0;   /* state 7 wraps the blink counter at 31 */
    /* red flash on strong hits (0x4aa29a), shown on even ticks */
    g->flash_on = false;
    if (g->flash > 0) { g->flash_on = (g->ciclo % 2) == 0; g->flash--; }
    osc_audio_update();
}

/* ---- rendering ------------------------------------------------------------------------ */
static void draw_fighter(osc_game *g, const osc_fighter *f)
{
    const osc_strip *s = f->strip;
    /* shadow (Efeitos[0]), rising a little with the jump */
    if (g->n_fx_tex > 0) {
        int sy = OSC_GROUND_Y - 12 - iround((OSC_GROUND_Y - iround(f->y)) / 12.0) - g->cam_y;
        draw_tex(g, g->fx_tex[0], (float)(iround(f->x) - 35 - g->cam_x), (float)sy, 255, false);
    }
    if (f->frame < 1 || f->frame > s->count || !s->tex[f->frame]) return;
    int w = s->w[f->frame];
    int off = (f->frame < 64) ? osc_frame_offset[f->char_id][f->frame] : 0;
    float dx = f->facing == 1 ? (float)(f->x - off - g->cam_x) : (float)(f->x + off - w - g->cam_x);
    float dy = (float)(f->y - f->sprite_h - g->cam_y);
    SDL_FRect dst = {dx, dy, (float)w, (float)s->h};
    SDL_RenderTextureRotated(g->ren, s->tex[f->frame], NULL, &dst, (double)f->rot, NULL,
                             f->facing == 1 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
    if (g->debug_boxes) {
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
        } else if (d->kind == 1 && g->fx_tex[d->sprite]) {
            float w = (float)g->fx_w[d->sprite], h = (float)g->fx_h[d->sprite];
            /* desenhaefeitos 0x48fb0e: x - Round(w / 2), y - Round(h / 2), or y - h with the f8 flag */
            float ox = (float)iround(w / 2.0), oy = d->bottom ? h : (float)iround(h / 2.0);
            SDL_FRect r = {(float)d->sx - ox, (float)d->sy - oy, w, h};
            SDL_RenderTextureRotated(g->ren, g->fx_tex[d->sprite], NULL, &r, 0.0, NULL,
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
    if (item >= g->n_anim_tex || !g->anim_tex[item]) return;
    SDL_FRect d = {(float)(x - g->cam_x), (float)(y - g->cam_y), (float)g->anim_w[item], (float)g->anim_h[item]};
    SDL_RenderTexture(g->ren, g->anim_tex[item], NULL, &d);
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
        if (13 < g->n_icone && g->icone[13])
            draw_tex(g, g->icone[13], (float)(225 - g->cam_x),
                     (float)(134 - iround(-cos((g->ciclo - 11) / 5.0) * 70.0) - g->cam_y), 255, false);
    }
}

void osc_fight_render(osc_game *g)
{
    SDL_SetRenderDrawColor(g->ren, 0, 0, 0, 255);
    SDL_RenderClear(g->ren);
    if (g->sp_draw) {
        /* super pause: Icone[7] tiled and sliding instead of the stage (0x4a47a0) */
        if (7 < g->n_icone && g->icone[7])
            for (int i = 0; i < 6; i++)
                for (int j = 0; j < 5; j++)
                    draw_tex(g, g->icone[7], (float)(100 * i - 80 + g->sp_off), (float)(100 * j), 255, false);
    } else {
        if (g->stage) {
            SDL_FRect dst = {(float)-g->cam_x, (float)-g->cam_y, (float)g->stage_w, (float)g->stage_h};
            SDL_RenderTexture(g->ren, g->stage, NULL, &dst);
        }
        /* stage animations (desenhaanimacoes, table 07c) */
        for (int i = 0; i < OSC_N_STAGE_ANIM; i++) {
            const osc_stage_anim_row *r = &osc_stage_anim[i];
            if (!(r->stage_mask & (1u << g->stage_id))) continue;
            if (g->ciclo >= r->ciclo_lo && g->ciclo <= r->ciclo_hi &&
                !(r->not_char && (g->p[1].char_id == r->not_char || g->p[2].char_id == r->not_char))) {
                SDL_Texture *t = r->is_effect ? (r->item < g->n_fx_tex ? g->fx_tex[r->item] : NULL)
                                              : (r->item < g->n_anim_tex ? g->anim_tex[r->item] : NULL);
                if (t) {
                    int w = r->is_effect ? g->fx_w[r->item] : g->anim_w[r->item];
                    int h = r->is_effect ? g->fx_h[r->item] : g->anim_h[r->item];
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
    if (g->mover.active && g->n_fx_tex > 9) {
        draw_tex(g, g->fx_tex[0], (float)(g->mover.x + 10 - g->cam_x), (float)(g->mover.y + 132 - g->cam_y), 255, false);
        draw_tex(g, g->fx_tex[9], (float)(g->mover.x - g->cam_x), (float)(g->mover.y - g->cam_y), 255, false);
    }
    if (g->crate.icon && !g->super_pause && g->crate.icon < g->n_icone && g->icone[g->crate.icon])
        draw_tex(g, g->icone[g->crate.icon], (float)(g->crate.x - g->cam_x), (float)(g->crate.y - g->cam_y), 255, false);
    /* the one who hit last is drawn on top */
    if (g->p[1].on_top) { draw_fighter(g, &g->p[2]); draw_fighter(g, &g->p[1]); }
    else { draw_fighter(g, &g->p[1]); draw_fighter(g, &g->p[2]); }
    osc_fx_render(g);
    draw_hud_bottom(g);
    draw_round_texts(g);
    draw_dialogue(g);
    if (g->flash_on) barra(g, 0, 0, 400, 300, rgb(255, 0, 0));
    if (g->show_moves) osc_movelist_render(g);   /* 0x4aa31c, state 7 only */
    if (g->challenge > 0) {
        osc_rgb pink = rgb(250, 140, 140);
        barraredonda(g, 80, 100, 320, 170, 10, k_white, k_black);                   /* 0x4ae384 */
        texto(g, 200, 100, osc_text(OSC_MEMO_TEXTO, 9, g->lang), 'c', 15, pink);    /* ESPERE ... */
        texto(g, 200, 120, osc_text(OSC_MEMO_TEXTO, 10, g->lang), 'c', 15, pink);   /* ALGUÉM ENTROU */
        texto(g, 200, 140, osc_text(OSC_MEMO_TEXTO, 11, g->lang), 'c', 15, pink);   /* NO COMBATE ! */
    }
}

void osc_game_shutdown(osc_game *g)
{
    osc_assets_free(g);
    memset(g, 0, sizeof *g);
}

void osc_play_snd(osc_game *g, int idx) { play_snd(g, idx); }

void osc_draw_fighter_at(osc_game *g, int slot, int x, int y, int state, int timer)
{
    osc_fighter *f = &g->p[slot];
    if (!f->strip || !f->strip->count) return;
    f->x = x; f->y = y; f->state = state; f->anim_timer = timer; f->in_air = false; f->rot = 0;
    f->sprite_h = k_sprite_h[f->char_id];
    select_pose(g, f);
    draw_fighter(g, f);
}
