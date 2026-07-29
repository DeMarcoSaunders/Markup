#ifndef MU_TABS_H
#define MU_TABS_H

#include "mu_core.h"

#ifdef __cplusplus
extern "C" {
#endif

void mu_tabs_register(MuContext *ctx);
uint32_t mu_kind_tabs(const MuContext *ctx);

/** Tab strip + body; add panes with mu_tabs_add. */
MuNode *mu_make_tabs(MuContext *ctx);

/** Append a tab; returns the content panel to fill with children. */
MuNode *mu_tabs_add(MuContext *ctx, MuNode *tabs, const char *label);
MuNode *mu_tabs_content(MuNode *tabs, int index);

int mu_tabs_count(const MuNode *tabs);
int mu_tabs_get_selection(const MuNode *tabs);
void mu_tabs_set_selection(MuContext *ctx, MuNode *tabs, int index);
const char *mu_tabs_label(const MuNode *tabs, int index);
void mu_tabs_set_on_change(MuNode *tabs, void (*cb)(void *user, int index), void *user);

#ifdef __cplusplus
}
#endif

#endif
