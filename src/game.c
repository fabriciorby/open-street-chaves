/* Lifecycle of the game state (FormCreate / FormDestroy). */
#include <string.h>

#include "data/assets.h"
#include "core/config.h"
#include "render/font.h"
#include "game.h"
#include "core/unlocks.h"

/* FormCreate: the common assets, the default records, the settings and the unlock files. */
bool osc_game_init(osc_game *g, SDL_Renderer *ren, const char *data_dir)
{
    bool ai_p2 = g->settings.ai_p2;
    int lang = g->settings.lang;
    memset(g, 0, sizeof *g);
    g->settings.ai_p2 = ai_p2;
    g->settings.lang = lang;
    g->flow.speed_delay = OSC_TICK_MS;   /* FormCreate: g_speedDelay = 28 */
    g->ren = ren;
    g->fight.rng = (uint32_t)SDL_GetTicksNS();
    SDL_strlcpy(g->data_dir, data_dir, sizeof g->data_dir);
    if (!osc_assets_load(g)) return false;   /* no data folder */
    /* default high-score table (FormCreate: criarecorde 1..10) */
    {
        static const char *names[10] = {"CHV", "SMD", "CHQ", "QUI", "DFL", "PGR", "SBR", "B71", "PTY", "NHO"};
        for (int i = 0; i < 10; i++) {
            memcpy(g->flow.records[i].name, names[i], 4);
            g->flow.records[i].char_id = i + 1;
            g->flow.records[i].score = i == 0 ? 10000 : 8500 - 500 * i;
        }
    }
    osc_font_init(ren);
    osc_config_load(g);
    g->flow.screen = OSC_SCREEN_ATTRACT;
    g->flow.screen_tick = 0;
    g->flow.sel[1] = 1; g->flow.sel[2] = 2;
    osc_unlocks_load(g);
    g->flow.p1_in = false;   /* nobody has joined until a start button */
    g->flow.p2_in = false;
    return true;
}

void osc_game_shutdown(osc_game *g)
{
    osc_assets_free(g);
    memset(g, 0, sizeof *g);
}
