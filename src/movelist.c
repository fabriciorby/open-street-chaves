/* G key in a fight: the move list (mostragolpe 0x491edc + mostrasequencia 0x491b5c).
 *
 * For each of the two fighters a dark panel with five lines "<move> - <inputs>".
 * The move names and input strings are Delphi string constants of Chaves.exe,
 * addressed by their index in the exetext scan (docs/spec/07f-movelist.txt);
 * the repository holds no text. Input letters: C B D E Y Z are arrow icons
 * (Icone[0..5]: up, down, right, left, diagonals), P K R ? are text tags. */
#include <stdio.h>
#include <string.h>

#include "canvas.h"
#include "exetext.h"
#include "font.h"

#include <string.h>

enum { K_SEP = 1362, K_TAG_P = 1363, K_TAG_K = 1364, K_TAG_R = 1365, K_TAG_Q = 1366 };

/* [char - 1][line] = {name constant, inputs constant} */
static const short k_moves[15][5][2] = {
    {{1367, 1368}, {1369, 1370}, {1371, 1372}, {1373, 1374}, {1375, 1376}},   /* 1 */
    {{1377, 1368}, {1378, 1379}, {1380, 1381}, {1382, 1383}, {1384, 1385}},   /* 2 */
    {{1386, 1368}, {1387, 1379}, {1388, 1370}, {1389, 1374}, {1390, 1391}},   /* 3 */
    {{1392, 1370}, {1393, 1394}, {1395, 1396}, {1397, 1368}, {1398, 1399}},   /* 4 */
    {{1400, 1368}, {1401, 1374}, {1402, 1403}, {1404, 1405}, {1406, 1407}},   /* 5 */
    {{1408, 1368}, {1409, 1410}, {1411, 1379}, {1412, 1413}, {1414, 1407}},   /* 6 */
    {{1415, 1416}, {1417, 1418}, {1419, 1403}, {1420, 1372}, {1421, 1385}},   /* 7 */
    {{1422, 1368}, {1423, 1424}, {1425, 1379}, {1426, 1374}, {1427, 1407}},   /* 8 */
    {{1428, 1410}, {1429, 1413}, {1430, 1368}, {1431, 1383}, {1432, 1407}},   /* 9 */
    {{1433, 1416}, {1434, 1418}, {1435, 1403}, {1436, 1410}, {1437, 1438}},   /* 10 */
    {{1439, 1368}, {1440, 1370}, {1441, 1410}, {1442, 1374}, {1443, 1407}},   /* 11 */
    {{1444, 1368}, {1445, 1379}, {1446, 1374}, {1447, 1383}, {1448, 1407}},   /* 12 */
    {{1449, 1368}, {1450, 1410}, {1451, 1370}, {1452, 1453}, {1454, 1438}},   /* 13 */
    {{1455, 1370}, {1456, 1374}, {1457, 1374}, {1458, 1379}, {1459, 1460}},   /* 14 */
    {{1461, 1368}, {1462, 1370}, {1463, 1413}, {1464, 1410}, {1465, 1399}},   /* 15 */
};

/* mostrasequencia: name at (x, y) then the inputs, 12 px per input */
static void sequence(osc_game *g, int x, int y, const char *name, const char *keys)
{
    char buf[160];
    osc_rgb c = name[0] == '*' ? rgb(255, 120, 120) : k_white;
    snprintf(buf, sizeof buf, "%s%s", name, osc_text_const(K_SEP));
    texto(g, x, y, buf, '-', 9, c);
    int px = x + (int)floorf(osc_font_width(buf, 9) + 0.5f);   /* Canvas.TextWidth */
    for (size_t i = 0; keys[i]; i++, px += 12) {
        static const char k_icon[] = "CBDEYZ";
        const char *ic = strchr(k_icon, keys[i]);
        if (ic && keys[i]) {
            int n = (int)(ic - k_icon);
            if (n < g->assets.n_icone && g->assets.icone[n]) draw_tex(g, g->assets.icone[n], (float)px, (float)(y + 3), 255, false);
        }
        int tag = keys[i] == 'P' ? K_TAG_P : keys[i] == 'K' ? K_TAG_K : keys[i] == 'R' ? K_TAG_R : keys[i] == '?' ? K_TAG_Q : 0;
        if (tag) texto(g, px, y, osc_text_const(tag), '-', 9, k_white);
    }
}

void osc_movelist_render(osc_game *g)
{
    for (int s = 1; s <= 2; s++) {
        int x = (s - 1) * 200, c = g->p[s].char_id;
        barra(g, x + 3, 115, x + 197, 200, rgb(20, 40, 60));
        retangulo(g, x + 3, 115, x + 197, 200, k_white);
        if (c < 1 || c > 15) continue;
        for (int l = 0; l < 5; l++)
            sequence(g, x + 6, 120 + 15 * l, osc_text_const(k_moves[c - 1][l][0]), osc_text_const(k_moves[c - 1][l][1]));
    }
}
