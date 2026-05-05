#ifndef MU_RAYLIB_H
#define MU_RAYLIB_H

#include "mu_core.h"
#include "mu_style.h"
#include <raylib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct MuRenderContext {
    Font font;
    bool font_loaded;
    MuColor scissor_stack[32];
    int scissor_depth;
};

void mu_render_init(MuRenderContext *rc);
void mu_render_shutdown(MuRenderContext *rc);

/** Current font: same as the active render context font, used for MeasureTextEx during layout. */
Font mu_raylib_ui_font(void);

/** Swap the font used for drawing and text metrics. If take_ownership is true, mu_render_shutdown calls
 * UnloadFont (never pass true for GetFontDefault()). */
void mu_render_set_font(MuRenderContext *rc, Font font, bool take_ownership);

void mu_render_begin(MuRenderContext *rc);
void mu_render_end(MuRenderContext *rc);

void mu_push_scissor(MuRenderContext *rc, MuRect r);
void mu_pop_scissor(MuRenderContext *rc);

Color mu_to_ray(MuColor c);
void mu_draw_rect(MuRenderContext *rc, MuRect r, MuColor fill, MuColor border, float border_w, float radius);
void mu_draw_text(MuRenderContext *rc, const char *text, float x, float y, float font_size, MuColor fg);

void mu_paint_all(MuContext *ctx, MuRenderContext *rc);

#ifdef __cplusplus
}
#endif

#endif
