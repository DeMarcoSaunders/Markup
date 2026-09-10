#ifndef MU_SOFT_INTERNAL_H
#define MU_SOFT_INTERNAL_H

#include "../include/markup/mu_soft.h"

/*
 * Shared between mu_soft.c (surface, shapes) and mu_soft_text.c (glyphs).
 * Not installed — nothing outside the software backend should include this.
 */

/** Current clip rect: the top of the stack, or the whole surface at depth 0. */
MuSoftClip mu_soft_clip_now(const MuRenderContext *rc);

/*
 * Per-pixel blending primitives.
 *
 * Defined inline here rather than out-of-line in mu_soft.c on purpose. They sit in the
 * innermost loop of every blit, glyph and blur composite, and as cross-translation-unit
 * calls they cost more than the arithmetic they perform: measured on the backdrop blur,
 * the calls alone were 13 of 32 ticks per pixel. Inlining them made that loop 41% faster.
 */

/** (x * a) / 255, exact for all 8-bit inputs without a divide. */
static inline uint32_t mu_soft_mul255(uint32_t x, uint32_t a) {
    uint32_t t = x * a + 0x80u;
    return (t + (t >> 8)) >> 8;
}

/** Source-over with a straight (non-premultiplied) source. */
static inline uint32_t mu_soft_blend_over(uint32_t dst, MuColor src, uint32_t alpha) {
    if (alpha == 0u) return dst;
    if (alpha == 255u) return mu_soft_pack(src);

    uint32_t inv = 255u - alpha;
    uint32_t dr = (dst >> 16) & 0xFFu, dg = (dst >> 8) & 0xFFu, db = dst & 0xFFu;
    uint32_t da = (dst >> 24) & 0xFFu;

    uint32_t r = mu_soft_mul255(src.r, alpha) + mu_soft_mul255(dr, inv);
    uint32_t g = mu_soft_mul255(src.g, alpha) + mu_soft_mul255(dg, inv);
    uint32_t b = mu_soft_mul255(src.b, alpha) + mu_soft_mul255(db, inv);
    uint32_t a = alpha + mu_soft_mul255(da, inv);

    return (a << 24) | (r << 16) | (g << 8) | b;
}

/* Glyph cache lifetime, driven by mu_soft_render_init / mu_soft_render_shutdown so
 * callers never manage it directly. */
void mu_soft_text_init(MuRenderContext *rc);
void mu_soft_text_shutdown(MuRenderContext *rc);

/** Mirrors mu_render_bind_measure so mu_text_measure(NULL, ...) resolves a font. */
void mu_soft_text_bind(MuRenderContext *rc);

/** Release every cached backdrop blur. For shutdown, and for resize — a cached rect
 * describes a position on the old surface and means nothing on a new one. */
void mu_soft_backdrop_cache_free_all(MuRenderContext *rc);

#endif
