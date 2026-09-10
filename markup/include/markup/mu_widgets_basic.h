#ifndef MU_WIDGETS_BASIC_H
#define MU_WIDGETS_BASIC_H

#include "mu_core.h"
#include "mu_image.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MuScrollAxis {
    MU_SCROLL_VERTICAL = 1,
    MU_SCROLL_HORIZONTAL = 2,
    MU_SCROLL_BOTH = 3,
} MuScrollAxis;

/* Register builtin kinds once per MuContext; frees with mu_context_shutdown. Second call is a no-op. */
void mu_widgets_basic_register(MuContext *ctx);

uint32_t mu_kind_panel(const MuContext *ctx);
uint32_t mu_kind_label(const MuContext *ctx);
uint32_t mu_kind_button(const MuContext *ctx);
uint32_t mu_kind_slider(const MuContext *ctx);
uint32_t mu_kind_checkbox(const MuContext *ctx);
uint32_t mu_kind_dropdown(const MuContext *ctx);
uint32_t mu_kind_textinput(const MuContext *ctx);
uint32_t mu_kind_modal(const MuContext *ctx);
uint32_t mu_kind_toast(const MuContext *ctx);
uint32_t mu_kind_image(const MuContext *ctx);
uint32_t mu_kind_scroll(const MuContext *ctx);

MuNode *mu_make_panel(MuContext *ctx, bool column);
void mu_panel_set_on_click(MuNode *panel, void (*on_click)(void *), void *user);
MuNode *mu_make_label(MuContext *ctx, const char *text);
MuNode *mu_make_button(MuContext *ctx, const char *text, void (*cb)(void *), void *user);
MuNode *mu_make_slider(MuContext *ctx, float mn, float mx, float val);
MuNode *mu_make_checkbox(MuContext *ctx, bool on, void (*on_toggle)(void *, bool), void *user);
MuNode *mu_make_dropdown(MuContext *ctx);
void mu_dropdown_add_option(MuNode *dd, const char *item);
MuNode *mu_make_textinput(MuContext *ctx, const char *initial);
const char *mu_textinput_get_text(const MuNode *input);
void mu_textinput_set_text(MuNode *input, const char *text);
void mu_textinput_set_placeholder(MuNode *input, const char *placeholder);
void mu_textinput_insert_utf8(MuContext *ctx, MuNode *input, const char *utf8);
MuNode *mu_make_modal(MuContext *ctx);
MuNode *mu_make_toast(MuContext *ctx, const char *msg);

void mu_modal_bind_layer(MuContext *ctx, MuNode *modal_root);
void mu_modal_set_visible(MuContext *ctx, MuNode *modal, bool show);

void mu_label_set_text(MuNode *label, const char *text);
float mu_slider_get_value(const MuNode *slider);
const char *mu_dropdown_selected_text(const MuNode *dropdown);
int mu_dropdown_get_selection(const MuNode *dropdown);
void mu_toast_set_message(MuNode *toast, const char *msg);

MuNode *mu_make_image(MuContext *ctx, uint32_t image_id);
void mu_image_set_id(MuNode *image, uint32_t image_id);
bool mu_image_set_source(MuNode *image, MuRenderContext *rc, const char *path);
void mu_image_set_fit(MuNode *image, MuImageFit fit);
void mu_image_set_tint(MuNode *image, MuColor tint);
void mu_image_set_radius(MuNode *image, float radius);
/** Atlas slice in texture pixels; pass w/h <= 0 to use the full image. */
void mu_image_set_src_rect(MuNode *image, MuRect src);

MuNode *mu_make_scroll(MuContext *ctx, MuScrollAxis axis);
MuNode *mu_scroll_content(MuNode *scroll);
void mu_scroll_set_offset(MuContext *ctx, MuNode *scroll, float x, float y);
void mu_scroll_get_offset(const MuNode *scroll, float *out_x, float *out_y);
void mu_scroll_by(MuContext *ctx, MuNode *scroll, float dx, float dy);
void mu_widgets_dispatch_wheel(MuContext *ctx, MuVec2 position, float delta_x, float delta_y);

/**
 * How much of the most recent offset change(s) the clamp absorbed — positive past the
 * max end, negative past zero — accumulated since the last call and reset by reading it.
 * mu_scroll.c has no notion of animation; this just surfaces information the clamp already
 * computes, so a caller can drive its own spring for rubber-banding without this widget
 * depending on markup_anim.
 */
void mu_scroll_get_excess(MuNode *scroll, float *out_x, float *out_y);

#include "mu_list.h"
#include "mu_tabs.h"

#ifdef __cplusplus
}
#endif

#endif
