#ifndef MU_RENDER_H
#define MU_RENDER_H

#include "mu_core.h"
#include "mu_image.h"
#include "mu_style.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MuRenderContext MuRenderContext;

typedef struct MuTextMetrics {
    float width;
    float height;
} MuTextMetrics;

/** Optional: bind rc so mu_text_measure(NULL, ...) uses the loaded UI font during layout. */
void mu_render_bind_measure(MuRenderContext *rc);

void mu_text_measure(MuRenderContext *rc, const char *text, const MuTextStyle *style, MuTextMetrics *out);

/** Word-wrap layout when max_width > 0; single line otherwise. */
void mu_text_measure_wrapped(MuRenderContext *rc, const char *text, const MuTextStyle *style, float max_width,
                             MuTextMetrics *out);

void mu_draw_text_wrapped(MuRenderContext *rc, const char *text, MuRect bounds, const MuTextStyle *style,
                          MuColor fg);

/** Load an additional font file; returns id >= 1, or 0 on failure. */
uint32_t mu_font_load_file(MuRenderContext *rc, const char *path, const char *family_name);

void mu_push_scissor(MuRenderContext *rc, MuRect r);
void mu_pop_scissor(MuRenderContext *rc);

void mu_draw_rect(MuRenderContext *rc, MuRect r, MuColor fill, MuColor border, float border_w, float radius);
void mu_draw_text(MuRenderContext *rc, const char *text, float x, float y, const MuTextStyle *style,
                  MuColor fg);

void mu_paint_tree(MuContext *ctx, MuRenderContext *rc, MuNode *node);

/**
 * Replace the contents of `area` with a blurred, tinted copy of what is already there.
 *
 * The frosted-glass primitive. Reads the surface, blurs it by `blur_radius`, mixes in
 * `tint` (its alpha is the mix amount: 0 leaves the blur untouched, 255 is a solid fill),
 * and writes the result back masked by a rounded rect of `corner_radius`.
 *
 * Must be called from a node's paint op, so that everything meant to show through has
 * already been drawn. Backends without cheap framebuffer read-back degrade to a flat
 * tint — see the notes in each backend.
 */
void mu_draw_backdrop_blur(MuRenderContext *rc, MuRect area, float blur_radius, MuColor tint,
                           float corner_radius);

/*
 * Backdrop-blur cache.
 *
 * A blur is expensive, and its input changes far less often than its output is painted:
 * anything happening *inside* a glass panel repaints the panel without touching what the
 * blur reads. These let a caller replay the previous result instead of recomputing it.
 *
 * mu_backdrop_cache_store snapshots the area straight after mu_draw_backdrop_blur wrote
 * it. mu_backdrop_cache_try replays that snapshot, returning false if it cannot — no
 * entry, different geometry, different blur parameters, or a different clip. The whole
 * rect is replayed, including pixels outside the rounded corners: those are backdrop, and
 * the caller has already established that the backdrop is unchanged.
 *
 * The caller owns the "has the backdrop changed" question — see
 * mu_node_backdrop_unchanged. These only verify that the cached pixels were produced for
 * the same request; they cannot tell that the content underneath has moved. Calling try
 * without that check will happily replay a stale blur.
 *
 * Entries are identified by node pointer *and* node id, so a destroyed node whose address
 * is later recycled cannot be handed the dead node's pixels.
 *
 * Backends whose blur degrades to a flat tint implement these as no-ops.
 */
bool mu_backdrop_cache_try(MuRenderContext *rc, const MuNode *node, MuRect area, float blur_radius,
                           MuColor tint, float corner_radius);
void mu_backdrop_cache_store(MuRenderContext *rc, const MuNode *node, MuRect area, float blur_radius,
                             MuColor tint, float corner_radius);

/** Forget any cached blur for `node`. Safe for nodes that were never stored. */
void mu_backdrop_cache_drop(MuRenderContext *rc, const MuNode *node);

/** Repaint the entire tree. Unconditional; ignores damage. */
void mu_paint_all(MuContext *ctx, MuRenderContext *rc);

/**
 * Repaint only what intersects the accumulated damage rect, pruning subtrees that
 * cannot touch it. Opt-in: call mu_damage_collect first, and clear/present the same
 * rect. Callers that do neither should keep using mu_paint_all.
 */
void mu_paint_damaged(MuContext *ctx, MuRenderContext *rc);

/** Backend-neutral present hook. CPU backends return this frame's pixels as ARGB8888
 * (0xAARRGGBB) for blitting to a window or framebuffer; row length is reported in bytes.
 * GPU backends that present directly return NULL. Lets host harnesses (mu_sdl, and later
 * a framebuffer target) drive any backend without naming one. */
const void *mu_present_pixel_data(MuRenderContext *rc, int *out_w, int *out_h, int *out_row_bytes);

#ifdef __cplusplus
}
#endif

#endif
