#include "../include/markup/mu_render.h"

void mu_paint_tree(MuContext *ctx, MuRenderContext *rc, MuNode *node) {
    if (!node || !(node->flags & MU_NODE_VISIBLE)) return;
    if (node->flags & MU_NODE_CLIP_CHILDREN) mu_push_scissor(rc, node->bounds);

    const MuNodeOps *ops = mu_get_node_ops(ctx, node->kind);
    if (ops && ops->paint) ops->paint(ctx, node, rc);

    for (int i = 0; i < node->child_count; i++) mu_paint_tree(ctx, rc, node->children[i]);

    if (ops && ops->paint_overlay) ops->paint_overlay(ctx, node, rc);

    if (node->flags & MU_NODE_CLIP_CHILDREN) mu_pop_scissor(rc);
}

void mu_paint_all(MuContext *ctx, MuRenderContext *rc) {
    if (!ctx || !rc) return;
    if (ctx->root) mu_paint_tree(ctx, rc, ctx->root);
    if (ctx->popup_layer && (ctx->popup_layer->flags & MU_NODE_VISIBLE))
        mu_paint_tree(ctx, rc, ctx->popup_layer);
    if (ctx->modal_layer && (ctx->modal_layer->flags & MU_NODE_VISIBLE))
        mu_paint_tree(ctx, rc, ctx->modal_layer);
}
