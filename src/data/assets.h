/* Loading of the game's files from the data folder (assets.c). */
#ifndef OSC_ASSETS_H
#define OSC_ASSETS_H

#include "game.h"

bool load_path(char *dst, size_t n, const char *dir, const char *rel);   /* "<dir>/<rel>" into dst, false if it does not fit */
bool load_image_tex(osc_game *g, const char *rel, bool key, SDL_Texture **tex, int *w, int *h);
bool osc_assets_load(osc_game *g);   /* FormCreate: effects, animations, faces, sounds, text, exe pictures; false = no data folder */
void osc_assets_free(osc_game *g);   /* everything osc_assets_load and the fights opened, audio and fonts included */
void osc_play_music(osc_game *g, int n);   /* Musicas/Musica-0n.wav */
bool osc_load_fighter(osc_game *g, int slot, int ch);   /* helper for the ending scripts (screen_endings.c) */
bool osc_load_stage(osc_game *g, int stage);

#endif
