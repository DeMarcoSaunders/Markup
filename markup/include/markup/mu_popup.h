#ifndef MU_POPUP_H
#define MU_POPUP_H

#include "mu_core.h"
#include "mu_input.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MuPopupPlacement {
    MU_POPUP_BELOW = 0,
    MU_POPUP_ABOVE = 1,
    MU_POPUP_AUTO = 2,
} MuPopupPlacement;

void mu_popup_register(MuContext *ctx);
uint32_t mu_kind_popup(const MuContext *ctx);

MuNode *mu_make_popup_layer(MuContext *ctx);
void mu_popup_bind_layer(MuContext *ctx, MuNode *layer);

MuNode *mu_make_popup(MuContext *ctx);
MuNode *mu_popup_content(MuNode *popup);
void mu_popup_set_anchor(MuNode *popup, MuNode *anchor);
void mu_popup_set_placement(MuNode *popup, MuPopupPlacement placement);
bool mu_popup_is_open(const MuNode *popup);

void mu_popup_open(MuContext *ctx, MuNode *popup);
void mu_popup_close(MuContext *ctx, MuNode *popup);
void mu_popup_toggle(MuContext *ctx, MuNode *popup);

/** Position open popups after layout; sizes popup layer to root bounds. */
void mu_popups_sync(MuContext *ctx);
/** Dismiss on click outside the open popup panel (call before pointer dispatch). */
void mu_popups_dispatch_pointer(MuContext *ctx, const MuPointerEvent *ev);

#ifdef __cplusplus
}
#endif

#endif
