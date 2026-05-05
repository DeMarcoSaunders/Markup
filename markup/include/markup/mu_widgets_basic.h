#ifndef MU_WIDGETS_BASIC_H
#define MU_WIDGETS_BASIC_H

#include "mu_core.h"

#ifdef __cplusplus
extern "C" {
#endif

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

MuNode *mu_make_panel(MuContext *ctx, bool column);
MuNode *mu_make_label(MuContext *ctx, const char *text);
MuNode *mu_make_button(MuContext *ctx, const char *text, void (*cb)(void *), void *user);
MuNode *mu_make_slider(MuContext *ctx, float mn, float mx, float val);
MuNode *mu_make_checkbox(MuContext *ctx, bool on, void (*on_toggle)(void *, bool), void *user);
MuNode *mu_make_dropdown(MuContext *ctx);
void mu_dropdown_add_option(MuNode *dd, const char *item);
MuNode *mu_make_textinput(MuContext *ctx, const char *initial);
MuNode *mu_make_modal(MuContext *ctx);
MuNode *mu_make_toast(MuContext *ctx, const char *msg);

void mu_modal_bind_layer(MuContext *ctx, MuNode *modal_root);
void mu_modal_set_visible(MuContext *ctx, MuNode *modal, bool show);

#ifdef __cplusplus
}
#endif

#endif
