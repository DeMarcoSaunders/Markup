#ifndef MU_SDL_H
#define MU_SDL_H

#include "mu_core.h"
#include "mu_skia.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MuSdlApp MuSdlApp;

MuSdlApp *mu_sdl_create(const char *title, int width, int height);
void mu_sdl_destroy(MuSdlApp *app);

bool mu_sdl_poll(MuSdlApp *app, MuContext *ctx);
void mu_sdl_present(MuSdlApp *app, MuRenderContext *rc);

int mu_sdl_width(const MuSdlApp *app);
int mu_sdl_height(const MuSdlApp *app);

/** Feed SDL pointer/keyboard events into Markup input. Call once per frame after poll. */
void mu_sdl_frame(MuContext *ctx, MuSdlApp *app);

#ifdef __cplusplus
}
#endif

#endif
