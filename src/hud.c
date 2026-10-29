/* Fight HUD: top and bottom bars, round texts, the last fight's dialogue. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "canvas.h"
#include "exetext.h"
#include "fight_internal.h"

const char *tx(osc_game *g, int get) { return osc_text(OSC_MEMO_TEXTO, get - 1, g->settings.lang); }   /* MemoTexto.Lines.Get(n) */
static bool even(int v) { return (v & 1) == 0; }

/* Top HUD, drawn before the fighters (DXTimer1Timer 0x4a49ac..0x4a5a61). */
void draw_hud_top(osc_game *g)
{
    int hy = g->fight.hud_y;
    char buf[96];
    snprintf(buf, sizeof buf, "%s %d", tx(g, 29), g->fight.fight_no);                        /* BATALHA n */
    texto(g, 200, hy + 3, buf, 'c', 7, k_white);
    barraredonda(g, 170, hy + 15, 230, hy + 54, 15, k_white, k_white);
    barraredonda(g, 172, hy + 17, 228, hy + 52, 15, k_grey, k_grey);
    barraredonda(g, 174, hy + 19, 226, hy + 50, 15, k_black, k_black);
    if (g->settings.cfg_time_mode != 4) {
        osc_rgb c = g->fight.timer_sub <= 2 ? rgb(255, 180, 180) : rgb(255, 50, 50);
        if (g->fight.timer_sec >= 10) {
            snprintf(buf, sizeof buf, "%d", g->fight.timer_sec);
            texto(g, 200, hy + 10, buf, 'c', 25, c);
        } else {
            snprintf(buf, sizeof buf, "0%d", g->fight.timer_sec);
            texto(g, 200, hy + 10, buf, 'c', 25, c);
            if (even(g->fight.timer_sub)) texto(g, 200, hy + 10, buf, 'c', 25, rgb(255, 180, 180));
        }
    }
    for (int p = 1; p <= 2; p++) {
        const osc_fighter *f = &g->fight.p[p];
        int s = p == 1 ? -1 : 1, c = f->char_id;
        int pulse = iround(cos(g->fight.ciclo / 4.75) * 30.0);
        osc_rgb plate = p == 1 ? rgb(pulse + 50, pulse + 50, 255) : rgb(255, pulse + 30, pulse + 30);
        barra(g, 200 + s * 198, hy + 40, 200 + s * 130, hy + 68, plate);
        retangulo(g, 200 + s * 198, hy + 40, 200 + s * 130, hy + 68, k_white);
        barraredonda(g, 200 + s * 157, hy + 30, 200 + s * 17, hy + 50, 8, k_black, k_white);
        barraredonda(g, 200 + s * 155, hy + 32, 200 + s * 19, hy + 48, 8, k_grey, k_grey);
        barraredonda(g, 200 + s * 153, hy + 34, 200 + s * 21, hy + 46, 8, k_black, k_black);
        if (f->health <= 15.0 && even(g->fight.ciclo))
            barraredonda(g, 200 + s * 153, hy + 34, 200 + s * 21, hy + 46, 8, k_black, rgb(200, 0, 0));
        /* face: Rxx-a, or Rxx-b shaking while the red bar falls; P2's faces are mirrored */
        int shake = 0;
        if (f->health_shown > f->health) shake = even(g->fight.ciclo) ? -1 : 1;
        if (c >= 1 && c <= 15) {
            SDL_Texture *t = shake ? g->assets.face_hurt[c] : g->assets.face_small[c];
            float w, h;
            if (t && SDL_GetTextureSize(t, &w, &h)) {
                SDL_FRect d = {(float)((p == 1 ? 3 : 347) - shake), (float)(hy + 7), w, h};
                SDL_RenderTextureRotated(g->ren, t, NULL, &d, 0.0, NULL, p == 1 ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL);
            }
        }
        /* tick marks (0x4a527b..0x4a5323): drawn on the real health, under the bars, so only the empty part shows them */
        for (int i = 0; i < 7; i++) {
            int tx_ = 200 + s * (40 + 16 * i);
            retangulo(g, tx_, hy + 35, tx_, hy + 45, f->health > 15.0 ? rgb(80, 80, 80) : rgb(150, 30, 30));
        }
        barra(g, 200 + iround(s * (25.0 + f->health_shown)), hy + 35, 200 + s * 25, hy + 45, rgb(170, 0, 0));
        int hx = 200 + iround(s * (25.0 + f->health));
        barra(g, hx, hy + 35, 200 + s * 25, hy + 45, rgb(150, 150, 0));
        barra(g, hx, hy + 36, 200 + s * 25, hy + 44, rgb(220, 220, 70));
        barra(g, hx, hy + 38, 200 + s * 25, hy + 42, rgb(250, 250, 100));
        barra(g, hx, hy + 39, 200 + s * 25, hy + 40, k_white);
        /* round lamps */
        int pulse2 = iround(cos(g->fight.ciclo / 2.735) * 60.0);
        for (int i = 1; i <= g->settings.rounds_to_win; i++) {
            int xa = 200 + s * (195 - 17 * (i - 1)), xb = 200 + s * (180 - 17 * (i - 1));
            barraredonda(g, xa, hy + 70, xb, hy + 85, 5, k_white, k_black);
            if (f->wins >= i) {
                barraredonda(g, xa, hy + 70, xb, hy + 85, 5, k_white, rgb(160 - pulse2, 0, 0));
                if (g->fight.ciclo < 10) barraredonda(g, xa, hy + 70, xb, hy + 85, 5, k_white, rgb(255, 0, 0));
            }
        }
        /* an empty side invites a second player */
        if (!f->human && g->flow.demo == 0 && g->fight.ciclo < 15) {
            if (g->flow.credits == 0) texto(g, 200 + s * 100, 0, tx(g, 6), 'c', 10, k_white);          /* INSIRA FICHAS */
            else if (!g->fight.no_push) texto(g, 200 + s * 100, 0, tx(g, 7), 'c', 10, k_white);      /* ENTRE AGORA */
        }
        const char *name = osc_text(OSC_MEMO_NOMES, c, g->settings.lang);
        if (p == 1) texto(g, 200 + s * 145, hy + 45, name, '-', 12, rgb(120, 120, 255));
        else texto(g, 200 + s * 145, hy + 45, name, 'e', 12, rgb(255, 110, 110));
        if (f->human) {
            int k = iround(cos(g->fight.ciclo / 4.75) * 20.0);
            snprintf(buf, sizeof buf, "%d", f->score);
            if (p == 1) {
                texto(g, 200 + s * 145, hy + 10, "1P", '-', 12, rgb(k + 120, k + 120, 255));
                texto(g, 200 + s * 50, hy + 10, buf, 'e', 12, rgb(k + 120, k + 120, 255));
            } else {
                texto(g, 200 + s * 145, hy + 10, "2P", 'e', 12, rgb(255, k + 110, k + 110));
                texto(g, 200 + s * 50, hy + 10, buf, '-', 12, rgb(255, k + 110, k + 110));
            }
        }
    }
}

/* Bottom HUD, after the fighters and effects (0x4a678e..0x4a6fff). */
void draw_hud_bottom(osc_game *g)
{
    int hy2 = g->fight.hud_y2;
    char buf[32];
    for (int p = 1; p <= 2; p++) {
        const osc_fighter *f = &g->fight.p[p];
        int s = p == 1 ? -1 : 1;
        if (f->msg_timer > 0) {
            const char *m = tx(g, f->msg_index);
            if (strcmp(m, "-") != 0) texto(g, 200 + s * 100, 80, m, 'c', 15, rgb(80, 255, 140));
            if (f->msg_index == 1 && f->human) texto(g, 200 + s * 80, 110, tx(g, 2), 'c', 10, k_white);   /* Primeiro Ataque */
        }
        if (f->combo_timer > 0 && f->hits > 1) {
            snprintf(buf, sizeof buf, "%d", f->hits);
            if (f->combo_timer > 15) texto(g, 195 + f->combo_jitter + s * 170, 110, buf, 'c', 25, k_white);
            else texto(g, 200 + s * 170, 110, buf, 'c', 25, k_white);
            texto(g, 200 + s * 150, 145, tx(g, 3), 'c', 12, rgb(80, 255, 140));                       /* Acertos. */
        }
        /* super stocks */
        barraredonda(g, 200 + s * 65, hy2 + 280, 200 + s * 22, hy2 + 288, 6, rgb(10, 20, 30), k_white);
        int pv = even(g->fight.ciclo) && f->stocks > 0 ? 100 : 0;
        for (int i = 1; i <= f->stocks; i++)
            circulopintado(g, 200 + s * (30 + 10 * (i - 1)), hy2 + 286, 200 + s * (35 + 10 * (i - 1)), hy2 + 281,
                           rgb(255, 20, 20), rgb(255, pv + 150, pv + 150));
        int pw = even(g->fight.ciclo) && f->stocks > 0 ? 50 : 0;
        snprintf(buf, sizeof buf, "%d", f->stocks);
        texto(g, 200 + s * 18, hy2 + 270, buf, 'c', 12, rgb(255 - pw, 255 - pw, 255));
        /* special meter */
        barraredonda(g, 200 + s * 170, hy2 + 275, 200 + s * 60, hy2 + 285, 5, k_white, k_white);
        barraredonda(g, 200 + s * 169, hy2 + 276, 200 + s * 61, hy2 + 284, 5, k_grey, k_grey);
        barraredonda(g, 200 + s * 168, hy2 + 277, 200 + s * 62, hy2 + 283, 5, rgb(60, 30, 0), k_black);
        int mx = 200 + iround(s * (64.0 + f->meter));
        if (f->meter < OSC_METER_MAX) {
            barra(g, mx, hy2 + 278, 200 + s * 64, hy2 + 282, rgb(210, 160, 30));
            barra(g, mx, hy2 + 279, 200 + s * 64, hy2 + 281, rgb(250, 220, 140));
        } else {
            int pm = even(g->fight.ciclo) ? 50 : 0, k = f->stocks + 1;
            barra(g, mx, hy2 + 278, 200 + s * 64, hy2 + 282, rgb(240, pm + 190, pm + 60));
            barra(g, mx, hy2 + 279, 200 + s * 64, hy2 + 281, rgb(255, 250, pm + 170));
            osc_rgb c = even(g->fight.ciclo) ? rgb(k + 140, k + 140, 255) : rgb(k + 170, k + 170, 255);
            texto(g, 200 + iround(s * 115.0), hy2 + 262, tx(g, 4), 'c', 15, c);                       /* M A X */
        }
    }
}

/* Round intro and outro texts, the winner's scene and the wipe (0x4a7005..0x4a83bf). */
void draw_round_texts(osc_game *g)
{
    char buf[96];
    int re = g->fight.round_end;
    if (g->fight.timer_sec == 0) {
        int p = even(g->fight.ciclo) ? 80 : 0;
        if (re < 80) texto(g, 200, 40, tx(g, 27), 'c', 28, rgb(255, p, p));                       /* FIM DO TEMPO */
        if (g->fight.p[1].health == g->fight.p[2].health && re < 90) texto(g, 200, 120, tx(g, 28), 'c', 29, rgb(255, 2 * p, 2 * p));   /* EMPATE */
    }
    if (g->fight.round_start <= 71) {
        if (g->fight.round_start > 5 && g->fight.round_start < 40) {
            int p = even(g->fight.ciclo) ? 30 : 0;
            snprintf(buf, sizeof buf, "%d", g->fight.round_no);
            texto(g, 260, 85, buf, 'c', 35, rgb(255 - p, 255 - p, 255 - p));
            texto(g, 200, 100, "ROUND   ", 'c', 20, k_white);
        }
        if (g->fight.round_start > 42 && g->fight.round_start < 60) {
            int p = even(g->fight.ciclo) ? 120 : 0;
            texto(g, 200, 120, tx(g, 15), 'c', 25, rgb(255, p + 30, p));                            /* LUTEM */
        }
    }
    if (!(g->fight.p[1].health <= 0.0 || (g->fight.p[2].health <= 0.0 && re < 230))) return;
    if (re > 0 && re < 30) {
        int p = even(g->fight.ciclo) ? 100 : 0;
        texto(g, 200, 100, "KO", 'c', 70, rgb(255, p, 0));
    }
    if (re > 30 && re < 90)
        for (int i = 1; i <= 2; i++) {
            const osc_fighter *f = &g->fight.p[i], *o = &g->fight.p[3 - i];
            if (f->health <= 0.0) continue;
            int p = even(g->fight.ciclo) ? 80 : 0;
            texto(g, 200, 110, osc_text(OSC_MEMO_NOMES, f->char_id, g->settings.lang), 'c', 20, rgb(255, p, p));
            texto(g, 200, 140, tx(g, 14), 'c', 14, k_white);                                        /* VENCEU */
            if (f->human && f->health < OSC_HEALTH_MAX) {
                snprintf(buf, sizeof buf, "%sx5=%d PTS.", tx(g, 5), iround(f->health) * 5);         /* VITALIDADE */
                texto(g, 200, 210, buf, 'c', 12, k_white);
            }
            int p3 = even(g->fight.ciclo) ? 120 : 0;
            if (f->health == OSC_HEALTH_MAX && re > 35) texto(g, 200, 170, tx(g, 30), 'c', 28, rgb(255, p3 + 120, p3 + 30));   /* PERFEITO */
            if (f->human && f->health == OSC_HEALTH_MAX) texto(g, 200, 210, "BONUS 1000 PTS.", 'c', 15, k_white);
            if (f->human && f->wins == g->settings.rounds_to_win && o->wins == 0) {
                int p4 = even(g->fight.ciclo) ? 20 : 0;
                snprintf(buf, sizeof buf, "%s  1000 PTS.", tx(g, 32));                              /* COMPLETO */
                texto(g, 200, 240, buf, 'c', 20, rgb(p4 + 170, p4 + 170, p4 + 235));
            }
        }
    for (int i = 1; i <= 2; i++) {
        const osc_fighter *f = &g->fight.p[i];
        if (f->wins < g->settings.rounds_to_win || re < 90 || re > 210) continue;
        int p5 = even(g->fight.ciclo) ? 30 : 0, c = f->char_id;
        /* the winner's big face (Rxx-c) slides in, holds, slides out */
        if (c >= 1 && c <= 15 && g->assets.face_big[c]) {
            int l = re < 100 ? 728 - (re - 90) * 50 : re < 180 ? 128 : 128 - (re - 180) * 50;
            SDL_FRect d = {(float)l, 29.f, 144.f, 189.f};
            SDL_RenderTexture(g->ren, g->assets.face_big[c], NULL, &d);
        }
        if (f->human) {
            texto(g, 10, 20, tx(g, 16), '-', 12, rgb(p5 + 30, p5 + 220, p5 + 30));                    /* MÁXIMO DE ACERTOS */
            snprintf(buf, sizeof buf, "%d", f->max_combo);
            texto(g, 10, 35, buf, '-', 14, rgb(255 - p5, 255 - p5, 255 - p5));
            texto(g, 10, 60, tx(g, 17), '-', 12, rgb(p5 + 30, p5 + 220, p5 + 30));                    /* PONTOS */
            snprintf(buf, sizeof buf, "%d", f->score);
            texto(g, 10, 75, buf, '-', 14, rgb(255 - p5, 255 - p5, 255 - p5));
        }
        barraredonda(g, 5, 220, 395, 280, 5, k_white, k_black);
        texto(g, 200, 237, osc_text(OSC_MEMO_FRASES, (c - 1) * 4 + f->quote, g->settings.lang), 'c', 12, k_white);
    }
    /* wipe to the next round */
    if (g->fight.p[1].wins < g->settings.rounds_to_win && g->fight.p[2].wins < g->settings.rounds_to_win && re > 95) {
        switch (g->fight.transition) {
        case 1:
            barra(g, 0, 0, 400, re * 10 - 950, k_black);
            barra(g, 0, 1250 - re * 10, 400, 300, k_black);
            break;
        case 2:
            barra(g, 0, 0, 200, re * 12 - 1150, k_black);
            barra(g, 200, 1450 - re * 12, 400, 300, k_black);
            break;
        case 3:
            for (int i = 0; i < 4; i++) {
                barra(g, 0, 80 * i, re * 17 - 1670, 80 * i + 40, k_black);
                barra(g, 2070 - re * 17, 80 * i + 40, 400, 80 * i + 80, k_black);
            }
            break;
        case 4:
            barra(g, 0, 0, 400, re * 10 - 1000, k_black);
            barra(g, 0, 1300 - re * 10, 400, 300, k_black);
            barra(g, 0, 0, re * 10 - 1000, 300, k_black);
            barra(g, 1400 - re * 10, 0, 400, 300, k_black);
            break;
        }
    }
}

/* ---- the last fight's dialogue (0x4a99de) ------------------------------------------
 * A is the human, B the CPU; four exchanges of 150 ticks, three lines each, from the
 * memo the port's text index calls Finais (the form field at 0x300, MemoDialogo). */
void draw_dialogue(osc_game *g)
{
    int d = g->fight.dialogo;
    if (d <= 0) return;
    int a = g->fight.p[1].human ? 1 : 2, b = g->fight.p[1].human ? 2 : 1;
    int ca = g->fight.p[a].char_id, cb = g->fight.p[b].char_id;
    barraredonda(g, 5, 165, 290, 215, 20, k_white, k_black);
    if (ca >= 1 && ca <= 15 && g->assets.face_big[ca])
        draw_tex(g, g->assets.face_big[ca], (float)(d > 580 ? 15 - (d - 580) * 10 : 15), 85.f, 255, false);
    barraredonda(g, 110, 220, 395, 270, 20, k_white, k_black);
    if (cb >= 1 && cb <= 15 && g->assets.face_big[cb])
        draw_tex(g, g->assets.face_big[cb], (float)(d > 580 ? 288 + (d - 580) * 10 : 288), 140.f, 255, false);
    /* on the exact boundaries the line and speaker keep their last values */
    if (d > 450 && d < 600) { g->fight.dialog_line = (ca - 1) * 12; g->fight.dialog_speaker = 1; }
    if (d > 300 && d < 450) { g->fight.dialog_line = (ca - 1) * 12 + 3; g->fight.dialog_speaker = 2; }
    if (d > 150 && d < 300) { g->fight.dialog_line = (ca - 1) * 12 + 6; g->fight.dialog_speaker = 1; }
    if (d > 0 && d < 150) { g->fight.dialog_line = (ca - 1) * 12 + 9; g->fight.dialog_speaker = 2; }
    for (int k = 0; k < 3; k++) {
        const char *s = osc_text(OSC_MEMO_FINAIS, g->fight.dialog_line + k, g->settings.lang);
        if (g->fight.dialog_speaker == 1) texto(g, 115, 168 + 14 * k, s, '-', 10, k_white);
        if (g->fight.dialog_speaker == 2) texto(g, 120, 223 + 14 * k, s, '-', 10, k_white);
    }
}
