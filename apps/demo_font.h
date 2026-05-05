#ifndef DEMO_FONT_H
#define DEMO_FONT_H

#ifdef __cplusplus
extern "C" {
#endif

struct MuRenderContext;
typedef struct MuStyleModule MuStyleModule;

/** Load bundled Inter Variable (SIL OFL) — falls through to raylib default if unreadable. */
void demo_font_try_load_ui(struct MuRenderContext *rc, const MuStyleModule *style);

#ifdef __cplusplus
}
#endif

#endif
