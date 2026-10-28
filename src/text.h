/* Text helpers: osc_text_at and osc_text_ex (TForm1.texto). */
#ifndef OSC_TEXT_H
#define OSC_TEXT_H

#include "game.h"

void osc_text_at(osc_game *g, float x, float y, const char *s, bool centered);
/* TForm1.texto: align 'c' centred, 'e' right-aligned, else left; size in points of the original font. */
void osc_text_ex(osc_game *g, float x, float y, const char *s, char align, int size, Uint8 r, Uint8 gg, Uint8 b);

#endif
