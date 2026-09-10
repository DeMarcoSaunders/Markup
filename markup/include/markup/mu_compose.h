#ifndef MU_COMPOSE_H
#define MU_COMPOSE_H

#include "mu_widgets_basic.h"
#include "mu_layout_flex.h"
#include "mu_image.h"
#include "mu_render.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Parameterized icon descriptor — shared by tiles, strips, taskbars, carousels. */
typedef struct MuIconDesc {
    uint32_t image_id;
    const char *path;
    const char *label;
    MuRect src;
    float size;
    float width;
    float height;
    MuImageFit fit;
    float radius;
    MuColor tint;
    void (*on_click)(void *);
    void *user;
} MuIconDesc;

void mu_icon_desc_init(MuIconDesc *desc);

/** Mark a node so pointer hits pass through to its clickable parent. */
void mu_compose_decorative(MuNode *node);

/** Vertical chip: icon + optional label; clickable when desc->on_click is set. */
MuNode *mu_compose_icon_chip(MuContext *ctx, MuRenderContext *rc, const MuIconDesc *desc);

/** Horizontal row of icons/chips (taskbar, dock). */
MuNode *mu_compose_icon_strip(MuContext *ctx, MuRenderContext *rc, const MuIconDesc *items, int count, float gap);

/** Horizontally scrollable strip of icon chips. */
MuNode *mu_compose_icon_carousel(MuContext *ctx, MuRenderContext *rc, const MuIconDesc *items, int count,
                                 float item_size, float viewport_w);

/** Row flex container that wraps children (icon grids, chip rows). */
MuNode *mu_compose_flow(MuContext *ctx, float gap);

/** Scrollable single-select list with fixed viewport height. */
MuNode *mu_compose_list(MuContext *ctx, float min_height);

/** Tab strip with one visible pane; add panes via mu_tabs_add. */
MuNode *mu_compose_tabs(MuContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
