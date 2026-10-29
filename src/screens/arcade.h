/* Arcade ladder and stage selection. */
#ifndef OSC_ARCADE_H
#define OSC_ARCADE_H

#include "game.h"

void osc_make_ladder(osc_game *g, int char_id);
int osc_stage_for(osc_game *g, int opponent);

#endif
