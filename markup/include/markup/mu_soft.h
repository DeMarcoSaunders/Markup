#ifndef MU_SOFT_H
#define MU_SOFT_H

/* Backends declare incompatible `struct MuRenderContext` layouts; only one may be visible. */
#ifdef MU_RENDER_BACKEND_SELECTED
#error "Markup: another backend header is already included in this translation unit. struct MuRenderContext layouts conflict; include only one backend (see mu_backend.h)."
#endif
#define MU_RENDER_BACKEND_SELECTED 1

#include "mu_image.h"
#include "mu_render.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Software rasterizer backend.
 *
 * Renders into a plain ARGB8888 buffer using only libc + math.h — no GPU, no
 * windowing system, no external rendering library. This is the backend intended
 * for bare-metal and non-desktop targets: hand it a framebuffer and feed it input.
 *
 * Pixels are 0xAARRGGBB in a uint32_t (byte order B,G,R,A on little-endian), which
 * matches SDL_PIXELFORMAT_ARGB8888 so mu_sdl can blit with no conversion pass.
 */

#define MU_SOFT_CLIP_MAX 64

typedef struct MuSoftClip {
    int x0, y0, x1, y1; /* half-open: [x0,x1) x [y0,y1) */
} MuSoftClip;

typedef struct MuSoftImageSlot {
    uint32_t *pixels; /* ARGB8888, width*height, owned */
    int width, height;
} MuSoftImageSlot;

/*
 * Cached backdrop blurs, one slot per blurred node.
 *
 * Small and fixed: a UI has a handful of glass surfaces, and each slot costs its own area
 * in pixels, so an unbounded cache would be a memory leak shaped like an optimisation.
 * Eviction is least-recently-used, which for a stable set of panels means never.
 */
#define MU_BACKDROP_CACHE_MAX 8

typedef struct MuSoftBackdropSlot {
    const MuNode *node;
    uint32_t node_id; /* paired with the pointer, so a recycled address cannot match */
    uint32_t *pixels; /* owned, w*h: surface contents just after the blur composited */
    int w, h;
    int x0, y0; /* clipped write origin the snapshot was taken at */
    /* The request this was produced for; any difference forces a recompute. */
    float blur_radius, corner_radius;
    uint32_t tint;
    uint64_t stamp; /* LRU ordering */
} MuSoftBackdropSlot;

struct MuRenderContext {
    uint32_t *pixels;
    int width, height;
    int stride; /* pixels per row, not bytes */
    bool owns_pixels;

    /* Clip stack. Depth 0 means "whole surface". Pushes beyond MU_SOFT_CLIP_MAX are
     * counted in scissor_overflow so push/pop stay balanced instead of desynchronising. */
    MuSoftClip clip[MU_SOFT_CLIP_MAX];
    int scissor_depth;
    int scissor_overflow;

    MuSoftImageSlot images[MU_IMAGE_MAX];
    int image_count;

    /* Font slots, glyph cache and atlas. Opaque so stb_truetype stays out of the public
     * headers; owned by mu_soft_text.c and managed by render_init/shutdown. */
    void *text;

    /* Two ping-pong buffers for backdrop blur, each blur_scratch_cap pixels. Kept on the
     * context so a blurred panel does not allocate every frame. */
    uint32_t *blur_scratch;
    int blur_scratch_cap;

    /* Cached blur results, owned here and freed by mu_soft_render_shutdown. Dropped
     * wholesale on resize, since every cached rect refers to the old surface. */
    MuSoftBackdropSlot backdrop_cache[MU_BACKDROP_CACHE_MAX];
    uint64_t backdrop_stamp;
    /* Hit/miss tallies. Diagnostic, but load-bearing for the tests: a cache that silently
     * never engages passes every correctness check, so the tests assert on these to prove
     * the fast path was actually taken. */
    uint32_t backdrop_hits, backdrop_misses;
};

/** Allocate an owned surface of width*height. */
void mu_soft_render_init(MuRenderContext *rc, int width, int height);

/** Render into caller-owned memory (a framebuffer). stride is in pixels, not bytes. */
void mu_soft_render_init_borrowed(MuRenderContext *rc, uint32_t *pixels, int width, int height, int stride);

void mu_soft_render_shutdown(MuRenderContext *rc);

/** No-op when the size is unchanged. Only valid for owned surfaces. */
void mu_soft_resize(MuRenderContext *rc, int width, int height);

/** Clears the surface and resets the clip stack. No matching end call is needed. */
void mu_soft_begin_frame(MuRenderContext *rc, MuColor clear);

/**
 * Clear only `area` (intersected with the surface) and reset the clip stack.
 *
 * The partial-repaint counterpart to mu_soft_begin_frame: pass the damage rect so
 * untouched pixels survive from the previous frame. Clearing the whole surface and then
 * repainting only the damaged part would blank everything else.
 */
void mu_soft_begin_frame_rect(MuRenderContext *rc, MuColor clear, MuRect area);

const void *mu_soft_pixel_data(MuRenderContext *rc, int *out_w, int *out_h, int *out_row_bytes);

/**
 * Load a TTF/OTF into the default font slot (MU_FONT_DEFAULT), used whenever a
 * MuTextStyle does not name another font. There is no platform font enumeration — on a
 * bare-metal target there is nothing to enumerate — so if this fails, text measures
 * using a fallback estimate and draws nothing. Returns false when the file is missing
 * or is not a font stb_truetype can parse.
 */
bool mu_soft_set_font_file(MuRenderContext *rc, const char *path);

/** True once a usable font is loaded in the default slot. */
bool mu_soft_has_font(const MuRenderContext *rc);

/**
 * Register an image from RGBA8 bytes already in memory; the pixels are copied.
 *
 * This is the image path for targets with no PNG decoder and no filesystem: embed the
 * data at build time and hand it straight over. `mu_image_load_file` is a thin wrapper
 * that decodes and then calls this. `stride` is in bytes; pass 0 for tightly packed.
 * Returns MU_IMAGE_INVALID on failure or when all MU_IMAGE_MAX slots are taken.
 */
uint32_t mu_soft_image_from_rgba(MuRenderContext *rc, const uint8_t *rgba, int width, int height,
                                 int stride);

/*
 * Pack/unpack helpers, exposed for tests.
 *
 * Defined here rather than in mu_soft.c so they inline. As out-of-line functions in
 * another translation unit they were a real call per pixel in the compositing loops,
 * which measured at over a third of the backdrop blur's total cost — and a build without
 * whole-program optimisation, which is the likely shape of a bare-metal one, has no way
 * to recover that.
 */
static inline uint32_t mu_soft_pack(MuColor c) {
    return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b;
}

static inline MuColor mu_soft_unpack(uint32_t v) {
    MuColor c;
    c.a = (unsigned char)((v >> 24) & 0xFFu);
    c.r = (unsigned char)((v >> 16) & 0xFFu);
    c.g = (unsigned char)((v >> 8) & 0xFFu);
    c.b = (unsigned char)(v & 0xFFu);
    return c;
}

/** Read a single pixel; returns transparent black when out of bounds. Test helper. */
MuColor mu_soft_get_pixel(const MuRenderContext *rc, int x, int y);

#ifdef __cplusplus
}
#endif

#endif
