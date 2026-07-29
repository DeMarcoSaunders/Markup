#ifndef MU_LAYOUT_FLEX_H
#define MU_LAYOUT_FLEX_H

#include "mu_core.h"

#ifdef __cplusplus
extern "C" {
#endif

void mu_layout_init(MuLayoutStyle *ls);

void mu_layout_set_padding(MuNode *node, float top, float right, float bottom, float left);
void mu_layout_set_padding_all(MuNode *node, float value);
void mu_layout_set_margin(MuNode *node, float top, float right, float bottom, float left);
void mu_layout_set_margin_all(MuNode *node, float value);
void mu_layout_set_gap(MuNode *node, float gap);
void mu_layout_set_flex_direction(MuNode *node, MuFlexDirection dir);
void mu_layout_set_justify(MuNode *node, MuFlexJustify justify);
void mu_layout_set_align_items(MuNode *node, MuFlexAlign align);
void mu_layout_set_align_self(MuNode *node, MuAlignSelf align);
void mu_layout_set_flex(MuNode *node, float grow, float shrink, float basis);
void mu_layout_set_min_size(MuNode *node, float min_w, float min_h);
void mu_layout_set_max_size(MuNode *node, float max_w, float max_h);
void mu_layout_set_flex_wrap(MuNode *node, bool wrap);

/** Intrinsic size for flex containers (handles wrap). */
void mu_layout_measure_container(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out);

void mu_node_set_hit_transparent(MuNode *node, bool transparent);
void mu_layout_mark_dirty(MuNode *node);

/** Set bounds; marks dirty when the rect changes. Returns true if bounds changed. */
bool mu_node_set_bounds(MuNode *node, MuRect bounds);

/** Layout one node if dirty; descendants are laid out when a flex pass runs. */
void mu_layout_node(MuContext *ctx, MuNode *node);

void mu_layout_flex_run(MuContext *ctx, MuNode *container);
void mu_layout_run(MuContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
