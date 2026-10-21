/* In-game text read from the user's own Chaves.exe.
 *
 * The original keeps every string (fighter names, HUD texts, menus, stage
 * names, dialogue, endings, win quotes, in Portuguese and Spanish) as Delphi
 * string constants in the CODE section. We scan them in file order with the
 * same rule as re/tools/textconst.py and address them through the numeric
 * index table generated from docs/spec/07d-text-index.txt, so the repository
 * never contains the text itself.
 */
#ifndef OSC_EXETEXT_H
#define OSC_EXETEXT_H

#include <stdbool.h>

enum { OSC_MEMO_NOMES = 0, OSC_MEMO_TEXTO, OSC_MEMO_CENARIOS, OSC_MEMO_DIALOGO, OSC_MEMO_FINAIS,
       OSC_MEMO_FRASES, OSC_MEMO_CONFIG };

/* Load <data_dir>/Chaves.exe. Returns false (and osc_text() yields "") if missing. */
bool osc_text_load(const char *data_dir);
void osc_text_free(void);
/* Memo line as in docs/spec/07, language 0 = Portuguese, 1 = Spanish. UTF-8. */
const char *osc_text(int memo, int line, int lang);
int osc_text_count(void);

#endif
