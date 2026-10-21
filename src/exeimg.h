/* Images embedded in the user's own Chaves.exe.
 *
 * The intro cinematic, the ending pictures and the HUD icons are not files in
 * the game folder: Delphi stores them inside the executable, in the TFORM1
 * form resource (RCDATA, binary DFM "TPF0"), as DelphiX TDXImageList items
 * whose Picture.Data is a Pascal class name ("TDIB") followed by a Windows
 * DIB. We read them at runtime from <data_dir>/Chaves.exe (03-assets.md), so
 * the repository never contains the pictures themselves.
 */
#ifndef OSC_EXEIMG_H
#define OSC_EXEIMG_H

#include <stdbool.h>

#include "bmp.h"

/* Parse <data_dir>/Chaves.exe. Returns false if the file or the form is missing. */
bool osc_exeimg_load(const char *data_dir);
void osc_exeimg_free(void);
/* Number of items in a list: "Abertura" (16), "Final" (11), "Icone" (14). */
int osc_exeimg_count(const char *list);
/* Decode item `index` of a list into RGBA; white is transparent when the item
 * is marked Transparent in the form. Caller frees with osc_image_free(). */
bool osc_exeimg_get(const char *list, int index, osc_image *out);

#endif
