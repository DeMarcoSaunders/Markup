#ifndef MU_STYLE_H
#define MU_STYLE_H

#include "mu_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MuColor {
    unsigned char r, g, b, a;
} MuColor;

typedef struct MuStyleSnapshot {
    MuColor background;
    MuColor foreground;
    MuColor border;
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

#ifdef __cplusplus
}
#endif

#endif
