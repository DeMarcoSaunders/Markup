#ifndef MU_SDL_H
#define MU_SDL_H

/* Backend-neutral: the harness only ever holds MuRenderContext*, never dereferences it,
 * and presents via mu_present_pixel_data(). No backend header is needed or wanted here. */
#include "mu_core.h"
#include "mu_render.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MuSdlApp MuSdlApp;

MuSdlApp *mu_sdl_create(const char *title, int width, int height);
void mu_sdl_destroy(MuSdlApp *app);

bool mu_sdl_poll(MuSdlApp *app, MuContext *ctx);
void mu_sdl_present(MuSdlApp *app, MuRenderContext *rc);

/**
 * Upload only `area` before presenting. Pairs with damage tracking: with a partial
 * repaint the rest of the texture already holds the previous frame, so re-uploading it
 * is wasted bandwidth.
 */
void mu_sdl_present_rect(MuSdlApp *app, MuRenderContext *rc, MuRect area);

int mu_sdl_width(const MuSdlApp *app);
int mu_sdl_height(const MuSdlApp *app);

/** Feed SDL pointer/keyboard events into Markup input. Call once per frame after poll. */
void mu_sdl_frame(MuContext *ctx, MuSdlApp *app);

#ifdef __cplusplus
}
#endif

#endif
