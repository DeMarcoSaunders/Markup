#ifndef MU_LIST_H
#define MU_LIST_H

#include "mu_core.h"

#ifdef __cplusplus
extern "C" {
#endif

void mu_list_register(MuContext *ctx);
uint32_t mu_kind_list(const MuContext *ctx);

MuNode *mu_make_list(MuContext *ctx, float min_height);
void mu_list_add_item(MuContext *ctx, MuNode *list, const char *text);
void mu_list_clear(MuContext *ctx, MuNode *list);
int mu_list_count(const MuNode *list);
int mu_list_get_selection(const MuNode *list);
void mu_list_set_selection(MuContext *ctx, MuNode *list, int index);
const char *mu_list_item_text(const MuNode *list, int index);
void mu_list_set_on_change(MuNode *list, void (*cb)(void *user, int index), void *user);

#ifdef __cplusplus
}
#endif

#endif
