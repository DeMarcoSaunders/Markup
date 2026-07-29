#ifndef MU_SKIA_H
#define MU_SKIA_H

#include "mu_render.h"

#ifdef __cplusplus
extern "C" {
#endif

struct MuRenderContext {
    void *backend;
    void *canvas;
    int scissor_depth;
};

void mu_skia_render_init(MuRenderContext *rc, int width, int height);
void mu_skia_render_shutdown(MuRenderContext *rc);
void mu_skia_resize(MuRenderContext *rc, int width, int height);

/** Clear the raster surface and obtain the SkCanvas for this frame. */
void mu_skia_begin_frame(MuRenderContext *rc, MuColor clear);

/** After painting, copy pixels to an SDL texture via mu_sdl_present. */
void mu_skia_end_frame(MuRenderContext *rc);

/** Load a TTF/OTF. Falls back to the platform default typeface when path is NULL or missing. */
bool mu_skia_set_font_file(MuRenderContext *rc, const char *path, float size);

const void *mu_skia_pixel_data(MuRenderContext *rc, int *out_w, int *out_h, int *out_row_bytes);

#ifdef __cplusplus
}
#endif

#endif
