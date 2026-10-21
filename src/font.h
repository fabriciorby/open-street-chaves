/* Text in the original's style. TForm1.texto draws with "Comic Sans MS" at a
 * point size through GDI. We rasterise a TrueType font with stb_truetype:
 * the system's Comic Sans MS when the machine has it (Windows, macOS), else
 * the bundled Comic Neue (SIL OFL, assets/fonts). Microsoft's font is never
 * shipped with the project. */
#ifndef OSC_FONT_H
#define OSC_FONT_H

#include <SDL3/SDL.h>
#include <stdbool.h>

bool osc_font_init(SDL_Renderer *ren);
void osc_font_quit(void);
bool osc_font_ready(void);
const char *osc_font_name(void);
/* width in pixels of a UTF-8 string at `size` points */
float osc_font_width(const char *utf8, int size);
/* draw at (x, y) = top-left of the text cell like GDI TextOut; black shadow at (+1,+1) */
void osc_font_draw(SDL_Renderer *ren, float x, float y, const char *utf8, int size, Uint8 r, Uint8 g, Uint8 b);

#endif
