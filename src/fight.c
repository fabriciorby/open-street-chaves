/* Fight flow: start of a fight, the per-tick order of the whole fight (input,
 * CPU, effects, round intro / time over / round end, physics), the Delphi RNG. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arcade.h"
#include "assets.h"
#include "canvas.h"
#include "config.h"
#include "exetext.h"
#include "fight_internal.h"

/* ---- Delphi System.Random -------------------------------------------------- */
int osc_random(osc_game *g, int n)
{
    g->rng = g->rng * 0x8088405u + 1u;
    if (n <= 0) return 0;
    return (int)(((uint64_t)(uint32_t)n * (uint64_t)g->rng) >> 32);
}

static void play_snd(osc_game *g, int idx)
{
    if (idx >= 0 && idx < g->assets.n_sounds && g->assets.sounds[idx].buf) osc_audio_play(&g->assets.sounds[idx]);
}

/* Round/fight init (state 6 -> 7). */
bool osc_fight_start(osc_game *g, int char1, int char2, int stage)
{
    char rel[64];
    osc_image img;
    char path[1200];
    for (int i = 0; i < 2; i++) osc_strip_free(&g->assets.strips[i]);
    memset(g->fx, 0, sizeof g->fx);
    memset(g->fx_draw, 0, sizeof g->fx_draw);

    int chars[2] = {char1, char2};
    for (int i = 0; i < 2; i++) {
        snprintf(rel, sizeof rel, "Lutadores/C%02d.bmp", chars[i]);
        load_path(path, sizeof path, g->data_dir, rel);
        if (!osc_bmp_load(path, true, &img)) return false;
        bool ok = osc_strip_build(g->ren, &img, &g->assets.strips[i]);
        osc_image_free(&img);
        if (!ok) return false;
        bool human = g->p[i + 1].human, had = g->p[i + 1].char_id != 0;
        int wins = g->p[i + 1].wins, score = g->p[i + 1].score;
        init_fighter(&g->p[i + 1], chars[i], i + 1, &g->assets.strips[i]);
        g->p[i + 1].human = had ? human : (i == 0 || !g->settings.ai_p2);
        g->p[i + 1].wins = wins; g->p[i + 1].score = score;
    }
    if (stage < 1 || stage > 12) {
        if (g->p[1].human && g->p[2].human) {                       /* 2P: Round(Random(120)/10)+1 */
            stage = iround(osc_random(g, 120) / 10.0) + 1;
            if (stage > 12) stage = 12;
            if (g->settings.unlocks.sc) stage = g->settings.unlocks.sc;
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
    SDL_Log("fight %d: %s vs %s at %s", g->fight_no, osc_text(OSC_MEMO_NOMES, char1, g->settings.lang),
            osc_text(OSC_MEMO_NOMES, char2, g->settings.lang), osc_text(OSC_MEMO_CENARIOS, stage - 1, g->settings.lang));
    return true;
}

void osc_fight_tick(osc_game *g, uint8_t in1, uint8_t in2)
{
    g->tick++;
    if (g->settings.unlocks.mentecapto) { g->hud_y = -90; g->hud_y2 = 40; }   /* top of DXTimer1Timer */
    if (g->demo > 0) { g->timer_sec = g->settings.cfg_time_mode; g->timer_sub = 25; }   /* attract demo: g_timerA := g_cfg1_mode every tick (0x4aa026) */
    if (g->challenge > 0) {                                        /* "ALGUÉM ENTROU NO COMBATE !" */
        g->no_push = true;
        if (--g->challenge == 0) {
            g->screen = OSC_SCREEN_SELECT; g->screen_tick = 0; g->sel_sub = 0;
            g->sel_locked[1] = g->sel_locked[2] = 0; g->timer_sec = 20; g->timer_sub = 25;
            g->p[1].human = g->p1_in; g->p[2].human = g->p2_in; g->settings.ai_p2 = !g->p2_in;
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
    if (g->settings.cfg_power == 2 || g->settings.cfg_power == 4) g->p[1].stocks = 3;        /* "Poder" 1P MAX / MAX */
    if (g->settings.cfg_power == 3 || g->settings.cfg_power == 4) g->p[2].stocks = 3;
    for (int i = 1; i <= 2; i++) {
        osc_fighter *f = &g->p[i];
        /* meter 102 -> stock (main loop 4572) */
        if (f->stocks < 0) f->stocks = 0;
        if (f->meter >= OSC_METER_MAX && f->stocks < 3) { f->stocks++; f->meter = 0; play_snd(g, 13); }
        if (f->stocks >= 3) { f->meter = OSC_METER_MAX; f->stocks = 3; }
        /* health bars (HUD loop 0x4a4cbe): the red bar waits 20 ticks after a hit, then
         * falls 3 per tick to the health */
        if (g->settings.unlocks.energeticos && g->p[1].health > 0.0 && g->p[2].health > 0.0) f->health += 0.15;   /* g_energeticos */
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
                if (f->human && f->wins == g->settings.rounds_to_win && o->wins == 0 && g->round_end == 45 && calm)
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
            if (f->wins < g->settings.rounds_to_win || g->round_end < 90 || g->round_end > 210) continue;
            if (g->no_push) set_move(g, f, 14);
            /* holding the first button hurries the scene along */
            if ((f->in & OSC_IN_B1) && (g->round_end < 90 || g->round_end > 100)) g->round_end += 2;
            if (g->round_end == 95 && !g->hit_stop && !g->super_pause && f->char_id >= 1 && f->char_id <= 15) {
                /* the victory line, a or b: Round(Random(200) / 200) with banker's rounding */
                const osc_wav *v = &g->assets.voices[f->char_id][osc_random(g, 200) > 100];
                if (v->buf) osc_audio_play(v);
            }
        }
        bool decided = g->p[1].wins >= g->settings.rounds_to_win || g->p[2].wins >= g->settings.rounds_to_win;
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
        if (decided && g->round_end > 220) g->match_over = g->p[1].wins >= g->settings.rounds_to_win ? 1 : 2;
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

void osc_play_snd(osc_game *g, int idx) { play_snd(g, idx); }
