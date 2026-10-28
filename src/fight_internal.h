/* Internal interface of the fight engine: the functions and tables the files
 * that used to be one game.c share with each other. Not for use outside the
 * fight (flow, menu, endings use game.h and the small per-module headers). */
#ifndef OSC_FIGHT_INTERNAL_H
#define OSC_FIGHT_INTERNAL_H

#include <math.h>

#include "game.h"
#include "tables_gen.h"

/* ---- per-character constants (02, 03), defined in fighter.c ---- */
extern const int k_sprite_h[16];
extern const double k_speed[16];
extern const double k_meter_unit;
extern const int k_gravity;

static inline int iround(double v) { return (int)rint(v); }   /* Delphi Round: half to even */

static inline osc_fighter *opp(osc_game *g, osc_fighter *f) { return f == &g->p[1] ? &g->p[2] : &g->p[1]; }

/* game.c */
void set_move(osc_game *g, osc_fighter *f, int move);
void select_frame(osc_game *g, osc_fighter *f);
void move_x(osc_game *g, osc_fighter *f, double dx);

/* effects.c */
osc_effect *spawn_fx(osc_game *g, int type, int variant, int owner, double x, double y);
void fx_sounds_for(osc_game *g, osc_fighter *f);
void fx_spawns_for(osc_game *g, osc_fighter *f, int pidx);
void update_fx(osc_game *g);

/* game.c */
void defend(osc_game *g, osc_fighter *f);

/* cpu.c */
bool chance(osc_game *g, double c, double k);
void cpu_think(osc_game *g, osc_fighter *f);

#endif
