#ifndef MU_RAYLIB_H
#define MU_RAYLIB_H

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

/** Current font: same as the active render context font, used for text metrics during layout. */
Font mu_raylib_ui_font(void);

/** Swap the font used for drawing and text metrics. If take_ownership is true, mu_render_shutdown calls
 * UnloadFont (never pass true for GetFontDefault()). */
void mu_render_set_font(MuRenderContext *rc, Font font, bool take_ownership);

void mu_render_begin(MuRenderContext *rc);
void mu_render_end(MuRenderContext *rc);

Color mu_to_ray(MuColor c);

void mu_raylib_frame(MuContext *ctx, MuRenderContext *rc);

#ifdef __cplusplus
}
#endif

#endif
