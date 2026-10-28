/* Unlock files next to the game (FormCreate): Churros.dat, Gentalha.dat,
 * Energeticos.dat, Satanas.dat, Fadiga.dat, Mentecapto.dat, Bruxa.dat and the
 * stage override SCnn.dat. Only their existence matters. */
#include <stdio.h>

#include "assets.h"
#include "game.h"
#include "unlocks.h"

/* FormCreate: unlock files next to the game */
void osc_unlocks_load(osc_game *g)
{
    char upath[1200];
    load_path(upath, sizeof upath, g->data_dir, "Churros.dat");
    g->unlock_churros = SDL_GetPathInfo(upath, NULL);
    load_path(upath, sizeof upath, g->data_dir, "Gentalha.dat");
    g->unlock_gentalha = SDL_GetPathInfo(upath, NULL);
    load_path(upath, sizeof upath, g->data_dir, "Energeticos.dat");
    g->unlock_energeticos = SDL_GetPathInfo(upath, NULL);
    load_path(upath, sizeof upath, g->data_dir, "Satanas.dat");
    g->unlock_satanas = SDL_GetPathInfo(upath, NULL);
    load_path(upath, sizeof upath, g->data_dir, "Fadiga.dat");
    g->unlock_fadiga = SDL_GetPathInfo(upath, NULL);
    load_path(upath, sizeof upath, g->data_dir, "Mentecapto.dat");
    g->unlock_mentecapto = SDL_GetPathInfo(upath, NULL);
    load_path(upath, sizeof upath, g->data_dir, "Bruxa.dat");
    g->unlock_bruxa = SDL_GetPathInfo(upath, NULL);
    g->unlock_sc = 0;
    for (int n = 1; n <= 12; n++) {      /* the last file found wins */
        char nm[16];
        snprintf(nm, sizeof nm, "SC%02d.dat", n);
        load_path(upath, sizeof upath, g->data_dir, nm);
        if (SDL_GetPathInfo(upath, NULL)) g->unlock_sc = n;
    }
}
