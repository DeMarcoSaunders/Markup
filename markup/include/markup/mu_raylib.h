#ifndef MU_RAYLIB_H
#define MU_RAYLIB_H

/*
 * DEPRECATED. Use mu_soft.h (markup_soft) instead.
 *
 * This backend is kept building for existing consumers and is no longer developed. What
 * it does not do, none of it planned:
 *
 *   - No backdrop blur. mu_draw_backdrop_blur degrades to a flat tint, and the backdrop
 *     cache entry points are no-ops, so frosted glass renders as a solid panel.
 *   - No host-readable framebuffer. mu_present_pixel_data returns NULL, so the shared
 *     mu_sdl window harness cannot drive it.
 *   - No test coverage at all. Nothing in tests/ links it, which is how it drifted
 *     behind the backend interface far enough to stop compiling.
 *
 * Damage tracking does work, since mu_paint_damaged is backend-neutral, but there is no
 * partial clear to pair with it the way mu_soft_begin_frame_region does.
 */

/* Backends declare incompatible `struct MuRenderContext` layouts; only one may be visible. */
#ifdef MU_RENDER_BACKEND_SELECTED
#error "Markup: another backend header is already included in this translation unit. struct MuRenderContext layouts conflict; include only one backend (see mu_backend.h)."
#endif
#define MU_RENDER_BACKEND_SELECTED 1

#include "mu_render.h"
#include "mu_image.h"
#include <raylib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MuRaylibImageSlot {
    Texture2D texture;
    bool owned;
    float width;
    float height;
} MuRaylibImageSlot;

typedef struct MuRaylibFontSlot {
    Font font;
    bool owned;
    char family[64];
} MuRaylibFontSlot;

struct MuRenderContext {
    Font font;
    bool font_loaded;
    int scissor_depth;
    MuRaylibFontSlot fonts[MU_FONT_MAX];
    int font_count;
    MuRaylibImageSlot images[MU_IMAGE_MAX];
    int image_count;
};

void mu_render_init(MuRenderContext *rc);
void mu_render_shutdown(MuRenderContext *rc);

/** Swap the font used for drawing and text metrics. If take_ownership is true, mu_render_shutdown calls
 * UnloadFont (never pass true for GetFontDefault()). */
void mu_render_set_font(MuRenderContext *rc, Font font, bool take_ownership);

Color mu_to_ray(MuColor c);

void mu_raylib_frame(MuContext *ctx, MuRenderContext *rc);

#ifdef __cplusplus
}
#endif

#endif
