#include "exeimg.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd32(const uint8_t *p) { return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24)); }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

typedef struct {
    char list[24];
    int index;
    const uint8_t *data;   /* Picture.Data: u8 len, class name, DIB */
    size_t len;
    bool transparent;
} item_t;

static uint8_t *g_exe;
static size_t g_exe_len;
static item_t *g_items;
static int g_n_items, g_cap_items;

/* ---- PE helpers ------------------------------------------------------------ */
typedef struct { uint32_t va, vsize, raw, rawsize; } sect_t;
static sect_t g_sec[16];
static int g_nsec;
static uint32_t g_imgbase;

static const uint8_t *rva_ptr(uint32_t rva, size_t need)
{
    for (int i = 0; i < g_nsec; i++) {
        uint32_t span = g_sec[i].vsize > g_sec[i].rawsize ? g_sec[i].vsize : g_sec[i].rawsize;
        if (rva >= g_sec[i].va && rva < g_sec[i].va + span) {
            size_t off = g_sec[i].raw + (rva - g_sec[i].va);
            if (off + need > g_exe_len) return NULL;
            return g_exe + off;
        }
    }
    return NULL;
}

/* ---- DFM (TPF0) reader ----------------------------------------------------- */
typedef struct { const uint8_t *p, *end; bool bad; } rd_t;

static int u8(rd_t *r) { if (r->p >= r->end) { r->bad = true; return 0; } return *r->p++; }
static uint32_t u32(rd_t *r) { if (r->p + 4 > r->end) { r->bad = true; return 0; } uint32_t v = rd32(r->p); r->p += 4; return v; }
static void skip(rd_t *r, size_t n) { if (r->p + n > r->end) { r->bad = true; r->p = r->end; } else r->p += n; }
static void sstr(rd_t *r, char *out, size_t cap)   /* Pascal short string */
{
    int n = u8(r);
    if (r->p + n > r->end) { r->bad = true; n = 0; }
    size_t c = (size_t)n < cap - 1 ? (size_t)n : cap - 1;
    if (out) { memcpy(out, r->p, c); out[c] = 0; }
    r->p += n;
}

/* Skip (or capture) one property value. */
static void read_value(rd_t *r, const uint8_t **bin, size_t *bin_len, int *boolv, char *strv, size_t strcap);

static void read_item_props(rd_t *r, const char *list, int index)
{
    item_t it;
    memset(&it, 0, sizeof it);
    SDL_strlcpy(it.list, list, sizeof it.list);
    it.index = index;
    while (!r->bad && r->p < r->end && *r->p != 0) {
        char pn[64];
        sstr(r, pn, sizeof pn);
        const uint8_t *bin = NULL; size_t bl = 0; int bv = -1;
        read_value(r, &bin, &bl, &bv, NULL, 0);
        if (!strcmp(pn, "Picture.Data")) { it.data = bin; it.len = bl; }
        else if (!strcmp(pn, "Transparent")) it.transparent = bv == 1;
    }
    u8(r);  /* end of item props */
    if (it.data) {
        if (g_n_items == g_cap_items) {
            g_cap_items = g_cap_items ? g_cap_items * 2 : 64;
            g_items = realloc(g_items, (size_t)g_cap_items * sizeof *g_items);
        }
        g_items[g_n_items++] = it;
    }
}

static const char *g_cur_list;   /* set while reading a TDXImageList's Items */

static void read_value(rd_t *r, const uint8_t **bin, size_t *bin_len, int *boolv, char *strv, size_t strcap)
{
    int t = u8(r);
    switch (t) {
    case 0: case 13: break;                                  /* null, nil */
    case 1:                                                  /* list */
        while (!r->bad && r->p < r->end && *r->p != 0) read_value(r, NULL, NULL, NULL, NULL, 0);
        u8(r);
        break;
    case 2: skip(r, 1); break;
    case 3: skip(r, 2); break;
    case 4: skip(r, 4); break;
    case 5: skip(r, 10); break;                              /* extended */
    case 6: case 7: sstr(r, strv, strv ? strcap : 1); break; /* string, ident */
    case 8: if (boolv) *boolv = 0; break;
    case 9: if (boolv) *boolv = 1; break;
    case 10: {                                               /* binary */
        uint32_t n = u32(r);
        if (r->p + n > r->end) { r->bad = true; break; }
        if (bin) { *bin = r->p; *bin_len = n; }
        r->p += n;
        break;
    }
    case 11:                                                 /* set */
        for (;;) { int n = u8(r); if (r->bad || n == 0) break; skip(r, (size_t)n); }
        break;
    case 12: { uint32_t n = u32(r); skip(r, n); break; }     /* long string */
    case 14: {                                               /* collection */
        int idx = 0;
        while (!r->bad && r->p < r->end && *r->p != 0) {
            if (*r->p == 2 || *r->p == 3 || *r->p == 4) read_value(r, NULL, NULL, NULL, NULL, 0);  /* item order */
            if (u8(r) != 1) { r->bad = true; break; }        /* vaList */
            if (g_cur_list) read_item_props(r, g_cur_list, idx);
            else {
                while (!r->bad && r->p < r->end && *r->p != 0) { sstr(r, NULL, 1); read_value(r, NULL, NULL, NULL, NULL, 0); }
                u8(r);
            }
            idx++;
        }
        u8(r);
        break;
    }
    case 15: skip(r, 4); break;
    case 16: case 17: case 19: skip(r, 8); break;
    case 18: { uint32_t n = u32(r); skip(r, (size_t)n * 2); break; }
    default: r->bad = true; break;
    }
}

static void read_component(rd_t *r, int depth)
{
    if (depth > 32 || r->bad) { r->bad = true; return; }
    int flags = r->p < r->end ? *r->p : 0;
    if ((flags & 0xF0) == 0xF0) {
        u8(r);
        if (flags & 2) read_value(r, NULL, NULL, NULL, NULL, 0);
    }
    char cls[64], name[64];
    sstr(r, cls, sizeof cls);
    sstr(r, name, sizeof name);
    bool imglist = !strcmp(cls, "TDXImageList") && !strncmp(name, "DXImageList", 11);
    while (!r->bad && r->p < r->end && *r->p != 0) {
        char pn[64];
        sstr(r, pn, sizeof pn);
        g_cur_list = (imglist && !strcmp(pn, "Items")) ? name + 11 : NULL;
        read_value(r, NULL, NULL, NULL, NULL, 0);
        g_cur_list = NULL;
    }
    u8(r);
    while (!r->bad && r->p < r->end && *r->p != 0) read_component(r, depth + 1);
    u8(r);
}

/* ---- resource directory ---------------------------------------------------- */
static bool res_name_is(const uint8_t *base, size_t base_len, uint32_t entry, const char *want)
{
    if (!(entry & 0x80000000u)) return false;
    uint32_t off = entry & 0x7fffffffu;
    if (off + 2 > base_len) return false;
    uint16_t n = rd16(base + off);
    if (off + 2 + 2u * n > base_len) return false;
    size_t wl = strlen(want);
    if (n != wl) return false;
    for (uint16_t i = 0; i < n; i++)
        if (rd16(base + off + 2 + 2u * i) != (uint8_t)want[i]) return false;
    return true;
}

static const uint8_t *find_form(size_t *out_len)
{
    const uint8_t *d = g_exe;
    if (g_exe_len < 0x40 || d[0] != 'M' || d[1] != 'Z') return NULL;
    uint32_t pe = rd32(d + 0x3c);
    if (pe + 24 + 96 + 8 * 3 > g_exe_len || rd32(d + pe) != 0x4550) return NULL;
    uint16_t nsec = rd16(d + pe + 6), optsz = rd16(d + pe + 20);
    const uint8_t *opt = d + pe + 24;
    g_imgbase = rd32(opt + 28);
    uint32_t res_rva = rd32(opt + 96 + 8 * 2), res_size = rd32(opt + 96 + 8 * 2 + 4);
    g_nsec = nsec < 16 ? nsec : 16;
    for (int i = 0; i < g_nsec; i++) {
        const uint8_t *s = opt + optsz + 40 * i;
        if ((size_t)(s - d) + 40 > g_exe_len) return NULL;
        g_sec[i].vsize = rd32(s + 8); g_sec[i].va = rd32(s + 12);
        g_sec[i].rawsize = rd32(s + 16); g_sec[i].raw = rd32(s + 20);
    }
    const uint8_t *base = rva_ptr(res_rva, res_size);
    if (!base) return NULL;
    size_t blen = res_size;
    /* level 1: types; level 2: names; level 3: languages */
    uint16_t nn = rd16(base + 12), ni = rd16(base + 14);
    for (uint32_t i = 0; i < (uint32_t)nn + ni; i++) {
        uint32_t e = 16 + 8 * i;
        if (e + 8 > blen) return NULL;
        if (rd32(base + e) != 10) continue;                       /* RT_RCDATA */
        uint32_t o2 = rd32(base + e + 4);
        if (!(o2 & 0x80000000u)) continue;
        o2 &= 0x7fffffffu;
        if (o2 + 16 > blen) return NULL;
        uint16_t nn2 = rd16(base + o2 + 12), ni2 = rd16(base + o2 + 14);
        for (uint32_t j = 0; j < (uint32_t)nn2 + ni2; j++) {
            uint32_t e2 = o2 + 16 + 8 * j;
            if (e2 + 8 > blen) return NULL;
            if (!res_name_is(base, blen, rd32(base + e2), "TFORM1")) continue;
            uint32_t o3 = rd32(base + e2 + 4) & 0x7fffffffu;
            if (o3 + 16 + 8 > blen) return NULL;
            uint32_t e3 = o3 + 16;
            uint32_t o4 = rd32(base + e3 + 4);                    /* data entry */
            if (o4 + 16 > blen) return NULL;
            uint32_t drva = rd32(base + o4), dsz = rd32(base + o4 + 4);
            const uint8_t *data = rva_ptr(drva, dsz);
            if (!data) return NULL;
            *out_len = dsz;
            return data;
        }
    }
    return NULL;
}

bool osc_exeimg_load(const char *data_dir)
{
    osc_exeimg_free();
    char path[1200];
    snprintf(path, sizeof path, "%s/Chaves.exe", data_dir);
    g_exe = SDL_LoadFile(path, &g_exe_len);
    if (!g_exe) { SDL_Log("exeimg: cannot read %s", path); return false; }
    size_t flen = 0;
    const uint8_t *form = find_form(&flen);
    if (!form || flen < 4 || memcmp(form, "TPF0", 4) != 0) {
        SDL_Log("exeimg: TFORM1 resource not found in %s", path);
        return false;
    }
    rd_t r = { form + 4, form + flen, false };
    read_component(&r, 0);
    SDL_Log("exeimg: %d embedded images (%s)", g_n_items, r.bad ? "form truncated" : "ok");
    return g_n_items > 0;
}

void osc_exeimg_free(void)
{
    free(g_items); g_items = NULL; g_n_items = g_cap_items = 0;
    SDL_free(g_exe); g_exe = NULL; g_exe_len = 0;
}

int osc_exeimg_count(const char *list)
{
    int n = 0;
    for (int i = 0; i < g_n_items; i++) if (!strcmp(g_items[i].list, list) && g_items[i].index + 1 > n) n = g_items[i].index + 1;
    return n;
}

bool osc_exeimg_get(const char *list, int index, osc_image *out)
{
    memset(out, 0, sizeof *out);
    for (int i = 0; i < g_n_items; i++) {
        const item_t *it = &g_items[i];
        if (strcmp(it->list, list) != 0 || it->index != index) continue;
        if (it->len < 2) return false;
        size_t cn = it->data[0];
        if (1 + cn + 40 > it->len) return false;
        return osc_bmp_decode(it->data + 1 + cn, it->len - 1 - cn, it->transparent, out);
    }
    return false;
}
