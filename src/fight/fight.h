/* Fight flow (fight.c). */
#ifndef OSC_FIGHT_H
#define OSC_FIGHT_H

#include "game.h"

/* Start a fight: characters 1..15, stage 1..12 (0 = by opponent), round 1. */
bool osc_fight_start(osc_game *g, int char1, int char2, int stage);
/* fight-screen internals used by screens/flow.c */
void osc_fight_tick(osc_game *g, uint8_t in1, uint8_t in2);
int osc_random(osc_game *g, int n);      /* Delphi Random(n) */
void osc_play_snd(osc_game *g, int idx);

#endif
