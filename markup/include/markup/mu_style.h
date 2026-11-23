#ifndef MU_STYLE_H
#define MU_STYLE_H

#include "mu_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MuStyleSnapshot {
    MuColor background;
    MuColor foreground;
    MuColor border;
    MuColor placeholder;
    float pad_top, pad_right, pad_bottom, pad_left;
    float border_width;
    float radius_tl, radius_tr, radius_br, radius_bl;
    MuTextStyle text;
} MuStyleSnapshot;

struct MuStyleModule {
    MuColor page_bg;
    MuColor panel_bg;
    MuColor panel_border;
    MuColor toolbar_bg;
    MuColor toolbar_border;
    MuColor label_fg;
    MuColor muted_fg;
    MuColor button_bg, button_fg, button_border;
    MuColor button_bg_hover, button_fg_hover;
    MuColor button_bg_pressed;
    MuColor input_bg, input_fg, input_border;
    MuColor slider_track, slider_thumb;
    MuColor checkbox_border, checkbox_fill;
    MuColor modal_overlay;
    MuColor modal_bg;
    /* Frosted panels. Both are translucent by design — the blurred backdrop shows through
     * them, and an opaque colour here would defeat the effect. */
    MuColor glass_bg;
    MuColor glass_border;
    float default_font_size;
    int default_font_weight;
    float default_letter_spacing;
    float default_radius;
};

void mu_style_init(MuContext *ctx, const MuStyleModule *initial);
void mu_style_shutdown(MuContext *ctx);
void mu_style_resolve(MuContext *ctx, MuNode *node, MuStyleSnapshot *out);

/* Overrides apply after the role and interaction state. Setters invalidate painting. */
void mu_node_set_fill(MuNode *node, MuColor color);
void mu_node_set_ink(MuNode *node, MuColor color);
void mu_node_set_border_color(MuNode *node, MuColor color);
void mu_node_set_border_width(MuNode *node, float width);
void mu_node_set_radius(MuNode *node, float radius);
void mu_node_set_placeholder_ink(MuNode *node, MuColor color);
/** Clear selected MU_STYLE_* bits to resume following the theme. */
void mu_node_clear_style(MuNode *node, uint32_t mask);
/** Transparent color disables the shadow. Blur and offset are in layout points. */
void mu_node_set_shadow(MuNode *node, MuColor color, float blur, MuVec2 offset);

#ifdef __cplusplus
}
#endif

#endif
