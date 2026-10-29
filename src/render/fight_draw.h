/* Fight rendering (fight_draw.c). */
#ifndef OSC_FIGHT_DRAW_H
#define OSC_FIGHT_DRAW_H

#include "game.h"

void osc_fight_render(osc_game *g);
void osc_fx_render(osc_game *g);
void osc_draw_fighter_at(osc_game *g, int slot, int x, int y, int state, int timer);   /* helper for the ending scripts (screen_endings.c) */

#endif
