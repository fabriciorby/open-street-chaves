/* Arcade ladder: the CPU opponent's stage (osc_stage_for, 07 state 5) and the
 * opponent order for a run (osc_make_ladder, criaadversario, 02). */
#include "arcade.h"
#include "fight.h"
#include "game.h"

/* Stage chosen by the CPU opponent's character (07, state 5). */
int osc_stage_for(osc_game *g, int opponent)
{
    static const int base[16] = {0, 1, 2, 2, 3, 3, 4, 11, 1, 1, 11, 6, 1, 1, 2, 1};
    static const int alt[16] = {0, 0, 7, 10, 10, 8, 8, 5, 9, 10, 10, 5, 9, 5, 1, 6};
    int st = base[opponent];
    if (osc_random(g, 5) < 2 && g->fight.fight_no < 8 && alt[opponent]) st = alt[opponent];
    if (osc_random(g, 6) < 2 && g->fight.fight_no < 8 &&
        (opponent == 3 || opponent == 4 || opponent == 9 || opponent == 10 || opponent == 11 || opponent == 14)) st = 12;
    if (g->settings.unlocks.sc && g->fight.fight_no < 8) st = g->settings.unlocks.sc;   /* SCnn.dat, not on the Gentalha path */
    return st;
}

/* criaadversario: 7 random distinct opponents plus the rival (02). */
void osc_make_ladder(osc_game *g, int ch)
{
    static const int rival[16] = {0, 2, 5, 9, 1, 2, 7, 2, 5, 3, 1, 3, 1, 5, 5, 3};
    static const int excl[16][6] = {{0}, {3, 9}, {3, 8, 9, 14, 15}, {1, 2, 14}, {5, 6}, {4, 6}, {4, 5, 15},
                                    {8, 10}, {2, 7}, {1, 2, 15}, {7, 9}, {1, 9}, {5, 8}, {1, 8, 14}, {2, 3, 13}, {1, 2, 9}};
    bool avail[16];
    for (int i = 1; i <= 15; i++) avail[i] = true;
    avail[ch] = false;
    avail[rival[ch]] = false;
    for (int i = 0; i < 6 && excl[ch][i]; i++) avail[excl[ch][i]] = false;
    for (int slot = 1; slot <= 7; slot++) {
        int c;
        do { c = osc_random(g, 15) + 1; } while (!avail[c]);
        g->flow.ladder[slot] = c;
        avail[c] = false;
    }
    g->flow.ladder[8] = rival[ch];
}
