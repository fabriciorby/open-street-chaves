/* Effects table (effects.c): what the ending scripts need. */
#ifndef OSC_EFFECTS_H
#define OSC_EFFECTS_H

#include "game.h"

void osc_spawn_fx(osc_game *g, int type, int x, int y);   /* owner 0 */
void osc_fx_tick(osc_game *g);

#endif
