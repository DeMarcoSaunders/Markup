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
void mu_paint_all(MuContext *ctx, MuRenderContext *rc);

#ifdef __cplusplus
}
#endif

#endif
