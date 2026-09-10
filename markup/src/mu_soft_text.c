#include "mu_soft_internal.h"
#include "../include/markup/mu_text.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Text rasterization for the software backend.
 *
 * Glyphs come from stb_truetype (vendored, public domain). Rather than baking a fixed
 * atlas up front, glyphs are rasterized on first use and cached by
 * (font, quantized size, codepoint), so arbitrary MuTextStyle.size values cost only what
 * is actually drawn instead of one atlas per size.
 *
 * Coverage bitmaps are packed into a single growable 8-bit atlas by a shelf packer:
 * glyphs are placed left to right on a shelf, a new shelf opens when the row fills, and
 * the atlas doubles in height when shelves run out. Growing only ever appends rows, so
 * previously cached glyph coordinates stay valid and the cache never needs flushing.
 *
 * Deliberately not handled:
 *   - Shaping. Advances are per-codepoint with kerning pairs; no ligatures, no complex
 *     scripts, no bidi. That needs HarfBuzz.
 *   - Subpixel (LCD) antialiasing. It needs to know the panel's subpixel geometry and
 *     complicates every blend; grayscale coverage is used instead.
 *   - Gamma-correct blending. Coverage is applied directly in sRGB, so text renders
 *     slightly lighter than a gamma-aware rasterizer would produce.
 *
 * NOTE: stb_truetype does no bounds checking on font data — see its own warning. Fonts
 * are assumed to be trusted application assets, not untrusted input.
 */

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "../vendor/stb_truetype.h"

#define ATLAS_WIDTH 512
#define ATLAS_INITIAL_HEIGHT 256
#define ATLAS_MAX_HEIGHT 4096
#define GLYPH_SLOTS 2048 /* power of two; open addressing with linear probing */
#define GLYPH_PADDING 1

typedef struct FontSlot {
    unsigned char *data; /* owned file bytes; stbtt indexes into these */
    stbtt_fontinfo info;
    bool loaded;
    char family[64];
} FontSlot;

typedef struct GlyphEntry {
    uint64_t key; /* 0 = empty */
    int16_t ax, ay;      /* position in atlas */
    int16_t w, h;        /* bitmap size */
    int16_t xoff, yoff;  /* offset from pen/baseline */
    float advance;
    bool rendered; /* false for whitespace and unmapped glyphs: advance only */
} GlyphEntry;

typedef struct MuSoftTextState {
    FontSlot fonts[MU_FONT_MAX];
    int font_count;

    unsigned char *atlas; /* 8-bit coverage */
    int atlas_w, atlas_h;
    int shelf_x, shelf_y, shelf_h;

    GlyphEntry glyphs[GLYPH_SLOTS];
    int glyph_count;
} MuSoftTextState;

static MuSoftTextState *text_of(MuRenderContext *rc) {
    return rc ? (MuSoftTextState *)rc->text : NULL;
}

/* ---------------------------------------------------------------------------
 * Lifetime
 * ------------------------------------------------------------------------ */

void mu_soft_text_init(MuRenderContext *rc) {
    if (!rc) return;
    MuSoftTextState *ts = (MuSoftTextState *)calloc(1, sizeof(MuSoftTextState));
    if (!ts) return;
    ts->font_count = 1; /* slot 0 is MU_FONT_DEFAULT, filled by mu_soft_set_font_file */
    rc->text = ts;
}

void mu_soft_text_shutdown(MuRenderContext *rc) {
    MuSoftTextState *ts = text_of(rc);
    if (!ts) return;
    for (int i = 0; i < MU_FONT_MAX; i++) free(ts->fonts[i].data);
    free(ts->atlas);
    free(ts);
    rc->text = NULL;
}

/* ---------------------------------------------------------------------------
 * Font slots
 * ------------------------------------------------------------------------ */

static unsigned char *read_whole_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long n = ftell(f);
    if (n <= 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    unsigned char *buf = (unsigned char *)malloc((size_t)n);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    if (got != (size_t)n) {
        free(buf);
        return NULL;
    }
    if (out_size) *out_size = got;
    return buf;
}

static bool font_load_into(MuSoftTextState *ts, int slot, const char *path, const char *family) {
    if (!ts || slot < 0 || slot >= (int)MU_FONT_MAX || !path || !path[0]) return false;

    size_t size = 0;
    unsigned char *data = read_whole_file(path, &size);
    if (!data) return false;

    stbtt_fontinfo info;
    int offset = stbtt_GetFontOffsetForIndex(data, 0);
    if (offset < 0 || !stbtt_InitFont(&info, data, offset)) {
        free(data);
        return false;
    }

    free(ts->fonts[slot].data);
    ts->fonts[slot].data = data;
    ts->fonts[slot].info = info;
    ts->fonts[slot].loaded = true;
    snprintf(ts->fonts[slot].family, sizeof(ts->fonts[slot].family), "%s", family ? family : path);
    if (slot >= ts->font_count) ts->font_count = slot + 1;
    return true;
}

bool mu_soft_set_font_file(MuRenderContext *rc, const char *path) {
    MuSoftTextState *ts = text_of(rc);
    if (!ts) return false;
    return font_load_into(ts, (int)MU_FONT_DEFAULT, path, "default");
}

bool mu_soft_has_font(const MuRenderContext *rc) {
    const MuSoftTextState *ts = rc ? (const MuSoftTextState *)rc->text : NULL;
    return ts && ts->fonts[MU_FONT_DEFAULT].loaded;
}

uint32_t mu_font_load_file(MuRenderContext *rc, const char *path, const char *family_name) {
    MuSoftTextState *ts = text_of(rc);
    if (!ts) return MU_FONT_DEFAULT;

    /* Slot 0 is the default; additional fonts get the next free slot. */
    int slot = ts->fonts[MU_FONT_DEFAULT].loaded ? ts->font_count : (int)MU_FONT_DEFAULT;
    if (slot >= (int)MU_FONT_MAX) return MU_FONT_DEFAULT;
    if (!font_load_into(ts, slot, path, family_name)) return MU_FONT_DEFAULT;
    return (uint32_t)slot;
}

static FontSlot *resolve_font(MuSoftTextState *ts, const MuTextStyle *style) {
    uint32_t id = style ? style->font_id : MU_FONT_DEFAULT;
    if (id >= MU_FONT_MAX || !ts->fonts[id].loaded) id = MU_FONT_DEFAULT;
    return ts->fonts[id].loaded ? &ts->fonts[id] : NULL;
}

/* ---------------------------------------------------------------------------
 * Atlas
 * ------------------------------------------------------------------------ */

static bool atlas_grow(MuSoftTextState *ts, int min_height) {
    int h = ts->atlas_h > 0 ? ts->atlas_h : ATLAS_INITIAL_HEIGHT;
    while (h < min_height) h *= 2;
    if (h > ATLAS_MAX_HEIGHT) return false;

    unsigned char *next = (unsigned char *)calloc((size_t)ATLAS_WIDTH * (size_t)h, 1);
    if (!next) return false;
    if (ts->atlas) {
        memcpy(next, ts->atlas, (size_t)ATLAS_WIDTH * (size_t)ts->atlas_h);
        free(ts->atlas);
    }
    ts->atlas = next;
    ts->atlas_w = ATLAS_WIDTH;
    ts->atlas_h = h;
    return true;
}

/** Shelf packer. Only ever appends rows, so cached glyph coordinates stay valid. */
static bool atlas_alloc(MuSoftTextState *ts, int w, int h, int *out_x, int *out_y) {
    if (w <= 0 || h <= 0 || w > ATLAS_WIDTH) return false;
    if (!ts->atlas && !atlas_grow(ts, ATLAS_INITIAL_HEIGHT)) return false;

    if (ts->shelf_x + w > ts->atlas_w) {
        ts->shelf_y += ts->shelf_h;
        ts->shelf_x = 0;
        ts->shelf_h = 0;
    }
    if (ts->shelf_y + h > ts->atlas_h && !atlas_grow(ts, ts->shelf_y + h)) return false;

    *out_x = ts->shelf_x;
    *out_y = ts->shelf_y;
    ts->shelf_x += w + GLYPH_PADDING;
    if (h > ts->shelf_h) ts->shelf_h = h + GLYPH_PADDING;
    return true;
}

/* ---------------------------------------------------------------------------
 * Glyph cache
 * ------------------------------------------------------------------------ */

/** Quantize to 0.5px so nearby sizes share entries instead of thrashing the cache. */
static uint32_t quantize_size(float size) {
    int q = (int)(size * 2.f + 0.5f);
    if (q < 1) q = 1;
    if (q > 0xFFFF) q = 0xFFFF;
    return (uint32_t)q;
}

static uint64_t glyph_key(uint32_t font_id, uint32_t size_q, uint32_t cp) {
    return ((uint64_t)(font_id + 1u) << 48) | ((uint64_t)size_q << 32) | (uint64_t)cp;
}

static GlyphEntry *glyph_lookup(MuSoftTextState *ts, uint64_t key) {
    size_t i = (size_t)(key * 1099511628211ull) & (GLYPH_SLOTS - 1u);
    for (int probe = 0; probe < GLYPH_SLOTS; probe++) {
        GlyphEntry *g = &ts->glyphs[i];
        if (g->key == key || g->key == 0) return g;
        i = (i + 1u) & (GLYPH_SLOTS - 1u);
    }
    return NULL;
}

/** Fetch or rasterize the glyph for `cp`. Returns NULL only when the cache is full. */
static GlyphEntry *glyph_get(MuSoftTextState *ts, FontSlot *font, uint32_t font_id, float size,
                             uint32_t cp) {
    uint32_t size_q = quantize_size(size);
    uint64_t key = glyph_key(font_id, size_q, cp);

    GlyphEntry *g = glyph_lookup(ts, key);
    if (!g) return NULL;
    if (g->key == key) return g;

    /* Cache is full enough that eviction would be needed — refuse rather than thrash.
     * At 2048 entries this needs ~1400 distinct (font,size,glyph) triples on screen. */
    if (ts->glyph_count >= (GLYPH_SLOTS * 3) / 4) return NULL;

    float scale = stbtt_ScaleForPixelHeight(&font->info, size);
    int adv = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(&font->info, (int)cp, &adv, &lsb);

    memset(g, 0, sizeof(*g));
    g->key = key;
    g->advance = (float)adv * scale;
    ts->glyph_count++;

    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetCodepointBitmapBox(&font->info, (int)cp, scale, scale, &x0, &y0, &x1, &y1);
    int w = x1 - x0, h = y1 - y0;
    if (w <= 0 || h <= 0) return g; /* whitespace or unmapped: advance only */

    int ax = 0, ay = 0;
    if (!atlas_alloc(ts, w, h, &ax, &ay)) return g; /* out of atlas: advance only */

    stbtt_MakeCodepointBitmap(&font->info, ts->atlas + (size_t)ay * (size_t)ts->atlas_w + (size_t)ax, w,
                              h, ts->atlas_w, scale, scale, (int)cp);
    g->ax = (int16_t)ax;
    g->ay = (int16_t)ay;
    g->w = (int16_t)w;
    g->h = (int16_t)h;
    g->xoff = (int16_t)x0;
    g->yoff = (int16_t)y0;
    g->rendered = true;
    return g;
}

/* ---------------------------------------------------------------------------
 * UTF-8
 * ------------------------------------------------------------------------ */

/** Decode one codepoint; advances *s. Invalid bytes yield U+FFFD and consume one byte. */
static uint32_t utf8_next_cp(const char **s) {
    const unsigned char *p = (const unsigned char *)*s;
    uint32_t c = *p;
    int n;
    if (c < 0x80u) n = 0;
    else if ((c & 0xE0u) == 0xC0u) { c &= 0x1Fu; n = 1; }
    else if ((c & 0xF0u) == 0xE0u) { c &= 0x0Fu; n = 2; }
    else if ((c & 0xF8u) == 0xF0u) { c &= 0x07u; n = 3; }
    else { *s += 1; return 0xFFFDu; }

    for (int i = 1; i <= n; i++) {
        if ((p[i] & 0xC0u) != 0x80u) { *s += 1; return 0xFFFDu; }
        c = (c << 6) | (uint32_t)(p[i] & 0x3Fu);
    }
    *s += n + 1;
    return c;
}

/* ---------------------------------------------------------------------------
 * Metrics
 * ------------------------------------------------------------------------ */

static MuRenderContext *g_text_rc;

/** mu_soft.c owns the bound measure context; mirror it so measure(NULL, ...) works. */
void mu_soft_text_bind(MuRenderContext *rc) {
    g_text_rc = rc;
}

static void style_defaults(const MuTextStyle **style, MuTextStyle *storage) {
    if (*style) return;
    mu_text_style_init(storage);
    storage->size = 16.f;
    storage->letter_spacing = 1.f;
    *style = storage;
}

/** Fallback when no font is loaded: keeps layout plausible instead of collapsing to 0. */
static void estimate_metrics(const char *text, float size, float spacing, MuTextMetrics *out) {
    size_t n = strlen(text);
    out->height = size * 1.2f;
    if (n == 0) return;
    out->width = (float)n * (size * 0.52f) + (float)(n - 1) * spacing;
}

void mu_text_measure(MuRenderContext *rc, const char *text, const MuTextStyle *style, MuTextMetrics *out) {
    if (!out) return;
    out->width = 0.f;
    out->height = 0.f;
    if (!text) return;
    if (!rc) rc = g_text_rc;

    MuTextStyle storage;
    style_defaults(&style, &storage);
    float size = style->size > 0.f ? style->size : 16.f;
    float spacing = style->letter_spacing >= 0.f ? style->letter_spacing : 1.f;

    MuSoftTextState *ts = text_of(rc);
    FontSlot *font = ts ? resolve_font(ts, style) : NULL;
    if (!font) {
        estimate_metrics(text, size, spacing, out);
        return;
    }

    float scale = stbtt_ScaleForPixelHeight(&font->info, size);
    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);
    (void)line_gap;
    out->height = (float)(ascent - descent) * scale;

    uint32_t font_id = style->font_id < MU_FONT_MAX ? style->font_id : MU_FONT_DEFAULT;
    float x = 0.f;
    uint32_t prev = 0;
    const char *p = text;
    while (*p) {
        uint32_t cp = utf8_next_cp(&p);
        GlyphEntry *g = glyph_get(ts, font, font_id, size, cp);
        if (!g) continue;
        if (prev) x += (float)stbtt_GetCodepointKernAdvance(&font->info, (int)prev, (int)cp) * scale;
        x += g->advance;
        if (*p) x += spacing;
        prev = cp;
    }

    /* Match the raylib backend's synthetic-style allowance so layout agrees across backends. */
    if (style->weight >= MU_TEXT_WEIGHT_BOLD) x += 1.f;
    if (style->italic > 0) x += 2.f;
    out->width = x;
}

/* ---------------------------------------------------------------------------
 * Drawing
 * ------------------------------------------------------------------------ */

/** Blit one cached glyph, modulating its coverage by `fg`. `shear` skews for italics. */
static void blit_glyph(MuRenderContext *rc, MuSoftTextState *ts, const GlyphEntry *g, float pen_x,
                       float baseline_y, MuColor fg, float shear) {
    if (!g->rendered) return;

    MuSoftClip clip = mu_soft_clip_now(rc);
    int gx = (int)lroundf(pen_x) + g->xoff;
    int gy = (int)lroundf(baseline_y) + g->yoff;

    for (int row = 0; row < g->h; row++) {
        int y = gy + row;
        if (y < clip.y0 || y >= clip.y1) continue;

        /* Italic skew: shift more at the top of the glyph than the bottom. */
        int slant = shear != 0.f ? (int)lroundf(shear * (float)(g->h - row)) : 0;
        const unsigned char *src = ts->atlas + (size_t)(g->ay + row) * (size_t)ts->atlas_w + (size_t)g->ax;
        uint32_t *dst = rc->pixels + (size_t)y * (size_t)rc->stride;

        for (int col = 0; col < g->w; col++) {
            unsigned char cov = src[col];
            if (!cov) continue;
            int x = gx + col + slant;
            if (x < clip.x0 || x >= clip.x1) continue;
            uint32_t alpha = mu_soft_mul255(fg.a, cov);
            if (!alpha) continue;
            dst[x] = mu_soft_blend_over(dst[x], fg, alpha);
        }
    }
}

void mu_draw_text(MuRenderContext *rc, const char *text, float x, float y, const MuTextStyle *style,
                  MuColor fg) {
    if (!rc || !rc->pixels || !text || !text[0] || fg.a == 0) return;

    MuTextStyle storage;
    style_defaults(&style, &storage);
    float size = style->size > 0.f ? style->size : 16.f;
    float spacing = style->letter_spacing >= 0.f ? style->letter_spacing : 1.f;

    MuSoftTextState *ts = text_of(rc);
    FontSlot *font = ts ? resolve_font(ts, style) : NULL;
    if (!font) return; /* no font: measure still reports a size, but nothing paints */

    float scale = stbtt_ScaleForPixelHeight(&font->info, size);
    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(&font->info, &ascent, &descent, &line_gap);
    (void)line_gap;

    /* `y` is the top of the text box, matching the raylib backend and the widgets. */
    float baseline = y + (float)ascent * scale;
    float shear = style->italic > 0 ? 0.2f : 0.f;
    bool bold = style->weight >= MU_TEXT_WEIGHT_BOLD;

    uint32_t font_id = style->font_id < MU_FONT_MAX ? style->font_id : MU_FONT_DEFAULT;
    float pen = x;
    uint32_t prev = 0;
    const char *p = text;
    while (*p) {
        uint32_t cp = utf8_next_cp(&p);
        GlyphEntry *g = glyph_get(ts, font, font_id, size, cp);
        if (!g) continue;
        if (prev) pen += (float)stbtt_GetCodepointKernAdvance(&font->info, (int)prev, (int)cp) * scale;

        blit_glyph(rc, ts, g, pen, baseline, fg, shear);
        /* Synthetic bold: redraw a pixel to the right, as the raylib backend does. */
        if (bold) blit_glyph(rc, ts, g, pen + 1.f, baseline, fg, shear);

        pen += g->advance;
        if (*p) pen += spacing;
        prev = cp;
    }
}
