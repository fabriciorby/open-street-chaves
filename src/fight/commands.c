/* Input side: direction helpers, the special-move sequences and the button
 * dispatch (DXTimer1Timer 0x4a5aaa). */
#include <stdlib.h>
#include <string.h>

#include "render/canvas.h"
#include "fight/fight_internal.h"

/* ---- direction helpers (seta) ------------------------------------------------- */
bool held(const osc_fighter *f, const char *dir)
{
    if (!strcmp(dir, "BAIXO")) return f->in & OSC_IN_DOWN;
    if (!strcmp(dir, "CIMA")) return f->in & OSC_IN_UP;
    bool left = f->in & OSC_IN_LEFT, right = f->in & OSC_IN_RIGHT;
    if (!strcmp(dir, "TRAS")) return f->facing == 1 ? left : right;
    return f->facing == 1 ? right : left; /* FRENTE */
}

static bool dir_bit(const osc_fighter *f, int bit)
{
    static const char *names[4] = {"BAIXO", "FRENTE", "TRAS", "CIMA"};
    return held(f, names[bit]);
}

/* ---- sequenciagolpe / sequencia ---------------------------------------------- */
void sequences(osc_game *g, osc_fighter *f)
{
    (void)g;
    if (held(f, "BAIXO")) { if (f->down_hold < 18) f->down_hold++; } else if (f->down_hold > 1) f->down_hold--;
    if (held(f, "TRAS")) { if (f->back_hold < 18) f->back_hold++; } else if (f->back_hold > 1) f->back_hold--;
    for (int i = 0; i < OSC_N_SEQS; i++) {
        const osc_seq_row *r = &osc_seqs[i];
        if (r->ch != f->char_id || r->from != f->seq_state) continue;
        bool ok = true;
        for (int b = 0; b < 4 && ok; b++) {
            bool h = dir_bit(f, b);
            if ((r->req & (1 << b)) && !h) ok = false;
            if ((r->forb & (1 << b)) && h) ok = false;
        }
        if (ok && r->down_hold && f->down_hold <= r->down_hold) ok = false;
        if (ok && r->on_ground && f->y != OSC_GROUND_Y) ok = false;
        if (ok) { f->seq_state = r->to; f->seq_timer = 9; }
    }
}

/* ---- golpe: button -> special move ----------------------------------------------- */
static void golpe(osc_game *g, osc_fighter *f, int btn)
{
    if (g->fight.hit_stop) return;
    for (int i = 0; i < OSC_N_GOLPE; i++) {
        const osc_golpe_row *r = &osc_golpe[i];
        if (r->ch != f->char_id || r->btn != btn) continue;
        bool ok = true;
        if (r->seq >= 0 && f->seq_state != r->seq) ok = false;
        if ((r->extra & 1) && f->mash_p <= 5) ok = false;
        if ((r->extra & 2) && f->mash_k <= 5) ok = false;
        if ((r->extra & 4) && f->vy == 0) ok = false;
        if (ok) set_move(g, f, r->move);
    }
}

/* ---- input dispatch (DXTimer1Timer 0x4a5aaa): walk, crouch, jump, then all the
 * button specials (golpe) before all the plain buttons (setMove 3..6) ---------- */
void input_dispatch(osc_game *g, osc_fighter *f)
{
    uint8_t in = f->in, pressed = (uint8_t)(f->in & ~f->in_prev);
    if (!(in & OSC_IN_DOWN)) {
        if (in & OSC_IN_LEFT) set_move(g, f, 1);
        if (in & OSC_IN_RIGHT) set_move(g, f, 2);
    }
    if (in & OSC_IN_DOWN) set_move(g, f, 7);
    if (!(in & OSC_IN_DOWN) && f->state == 7) f->state = 1;
    if ((in & OSC_IN_UP) && (f->state == 1 || f->state == 2 || f->state == 21 || f->state == 22) &&
        f->vy == 0 && !f->in_air) {
        f->vy = -32; f->in_air = true; f->state = 12; f->anim_timer = 0; f->anim_dur = 99; f->blocking = false;
        f->jump_dir = 1;
        if (in & OSC_IN_LEFT) f->jump_dir = 2;
        if (in & OSC_IN_RIGHT) f->jump_dir = 3;
    }
    if (!(in & (OSC_IN_LEFT | OSC_IN_RIGHT)) && f->state == 2) { f->state = 1; f->anim_timer = 0; }
    for (int b = 0; b < 4; b++)
        if (pressed & (OSC_IN_B1 << b)) golpe(g, f, b + 1);
    for (int b = 0; b < 4; b++)
        if (pressed & (OSC_IN_B1 << b)) set_move(g, f, 3 + b);
    if (pressed & (OSC_IN_B1 | OSC_IN_B3)) f->mash_p += 6;
    if (pressed & (OSC_IN_B2 | OSC_IN_B4)) f->mash_k += 6;
}
