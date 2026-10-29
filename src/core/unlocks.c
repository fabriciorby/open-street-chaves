/* Unlock files next to the game (FormCreate): Churros.dat, Gentalha.dat,
 * Energeticos.dat, Satanas.dat, Fadiga.dat, Mentecapto.dat, Bruxa.dat and the
 * stage override SCnn.dat. Only their existence matters. */
#include <stdio.h>

#include "data/assets.h"
#include "game.h"
#include "core/unlocks.h"

/* FormCreate: unlock files next to the game */
void osc_unlocks_load(osc_game *g)
{
    char upath[1200];
    load_path(upath, sizeof upath, g->data_dir, "Churros.dat");
    g->settings.unlocks.churros = SDL_GetPathInfo(upath, NULL);
    load_path(upath, sizeof upath, g->data_dir, "Gentalha.dat");
    g->settings.unlocks.gentalha = SDL_GetPathInfo(upath, NULL);
    load_path(upath, sizeof upath, g->data_dir, "Energeticos.dat");
    g->settings.unlocks.energeticos = SDL_GetPathInfo(upath, NULL);
    load_path(upath, sizeof upath, g->data_dir, "Satanas.dat");
    g->settings.unlocks.satanas = SDL_GetPathInfo(upath, NULL);
    load_path(upath, sizeof upath, g->data_dir, "Fadiga.dat");
    g->settings.unlocks.fadiga = SDL_GetPathInfo(upath, NULL);
    load_path(upath, sizeof upath, g->data_dir, "Mentecapto.dat");
    g->settings.unlocks.mentecapto = SDL_GetPathInfo(upath, NULL);
    load_path(upath, sizeof upath, g->data_dir, "Bruxa.dat");
    g->settings.unlocks.bruxa = SDL_GetPathInfo(upath, NULL);
    g->settings.unlocks.sc = 0;
    for (int n = 1; n <= 12; n++) {      /* the last file found wins */
        char nm[16];
        snprintf(nm, sizeof nm, "SC%02d.dat", n);
        load_path(upath, sizeof upath, g->data_dir, nm);
        if (SDL_GetPathInfo(upath, NULL)) g->settings.unlocks.sc = n;
    }
}
