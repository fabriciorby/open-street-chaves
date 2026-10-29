/* Settings file (Chaves.dat) and the values derived from it. */
#ifndef OSC_CONFIG_H
#define OSC_CONFIG_H

#include "game.h"

void osc_config_load(osc_game *g);
void osc_config_save(osc_game *g);
int osc_round_time(const osc_game *g);

static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

#endif
