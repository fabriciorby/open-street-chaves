/* Internal header shared by the screen_*.c files and the dispatcher in flow.c.
 * One screen = one file; each exports a tick and a render with the signatures
 * the dispatch table in flow.c expects. */
#ifndef OSC_SCREENS_H
#define OSC_SCREENS_H

#include <math.h>
#include <stdint.h>

#include "game.h"

/* Delphi Round: half to even */
static inline int lround_i(double v) { return (int)rint(v); }

void osc_select_tick(osc_game *g, uint8_t in1, uint8_t in2);       /* screen_select.c */
void osc_select_render(osc_game *g);
void osc_vs_tick(osc_game *g, uint8_t in1, uint8_t in2);           /* screen_vs.c */
void osc_vs_render(osc_game *g);
void osc_continue_tick(osc_game *g, uint8_t in1, uint8_t in2);     /* screen_continue.c */
void osc_continue_render(osc_game *g);
void osc_attract_tick(osc_game *g, uint8_t in1, uint8_t in2);      /* screen_attract.c (osc_go_attract: game.h) */
void osc_attract_render(osc_game *g);
void osc_records_tick(osc_game *g, uint8_t in1, uint8_t in2);      /* screen_records.c */
void osc_records_render(osc_game *g);
void osc_name_tick(osc_game *g, uint8_t in1, uint8_t in2);         /* screen_name.c */
void osc_name_render(osc_game *g);
void osc_name_leave(osc_game *g);                                  /* leave_name_entry, also used by the fight screen */
void osc_fight_screen_tick(osc_game *g, uint8_t in1, uint8_t in2); /* screen_fight.c */
void osc_fight_screen_render(osc_game *g);

#endif
