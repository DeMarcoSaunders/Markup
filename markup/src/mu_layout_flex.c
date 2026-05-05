#include "../include/markup/mu_layout_flex.h"
#include <math.h>
#include <stddef.h>

void mu_layout_run(MuContext *ctx) {
    if (!ctx || !ctx->root) return;
    const MuNodeOps *ops = mu_get_node_ops(ctx, ctx->root->kind);
    if (ops && ops->layout_children)
        ops->layout_children(ctx, ctx->root);
    else if (ctx->root->flags & MU_NODE_FLEX_CONTAINER)
        mu_layout_flex_run(ctx, ctx->root);
}

static MuVec2 measure_node(MuContext *ctx, MuNode *node, MuVec2 avail) {
    const MuNodeOps *ops = mu_get_node_ops(ctx, node->kind);
    MuVec2 d = {0, 0};
    if (ops && ops->measure) ops->measure(ctx, node, avail, &d);
    if (node->flex_basis >= 0) {
        if (node->parent && (node->parent->flags & MU_NODE_FLEX_CONTAINER)) {
            MuFlexLayoutState *fs = (MuFlexLayoutState *)node->parent->state;
            if (fs) {
                if (fs->direction == MU_FLEX_ROW)
                    d.x = node->flex_basis;
                else
                    d.y = node->flex_basis;
            }
        }
    }
    return d;
}

void mu_layout_flex_run(MuContext *ctx, MuNode *container) {
    if (!container || !(container->flags & MU_NODE_FLEX_CONTAINER)) return;
    MuFlexLayoutState *fl = (MuFlexLayoutState *)container->state;
    if (!fl) return;

    float inner_x = container->bounds.x + fl->pad_left;
    float inner_y = container->bounds.y + fl->pad_top;
    float inner_w = container->bounds.w - fl->pad_left - fl->pad_right;
    float inner_h = container->bounds.h - fl->pad_top - fl->pad_bottom;
    if (inner_w < 0) inner_w = 0;
    if (inner_h < 0) inner_h = 0;

    int n = container->child_count;
    if (n <= 0) return;
    if (!ctx->scratch) return;

    MuVec2 *sizes =
        (MuVec2 *)mu_arena_alloc(ctx->scratch, sizeof(MuVec2) * (size_t)n, _Alignof(MuVec2));
    float *grow = (float *)mu_arena_alloc(ctx->scratch, sizeof(float) * (size_t)n, _Alignof(float));
    if (!sizes || !grow) return;

    MuVec2 avail = {inner_w, inner_h};
    float total_main = 0;
    float total_cross_max = 0;
    float total_grow = 0;

    for (int i = 0; i < n; i++) {
        MuNode *ch = container->children[i];
        if (!(ch->flags & MU_NODE_VISIBLE)) {
            sizes[i].x = sizes[i].y = 0;
            grow[i] = 0;
            continue;
        }
        grow[i] = ch->flex_grow > 0 ? ch->flex_grow : 0;
        sizes[i] = measure_node(ctx, ch, avail);
        total_grow += grow[i];
        if (fl->direction == MU_FLEX_ROW) {
            total_main += sizes[i].x + (i > 0 ? fl->gap : 0);
            if (sizes[i].y > total_cross_max) total_cross_max = sizes[i].y;
        } else {
            total_main += sizes[i].y + (i > 0 ? fl->gap : 0);
            if (sizes[i].x > total_cross_max) total_cross_max = sizes[i].x;
        }
    }

    float extra = 0;
    if (fl->direction == MU_FLEX_ROW)
        extra = inner_w - total_main;
    else
        extra = inner_h - total_main;

    if (total_grow > 0 && extra > 0) {
        for (int i = 0; i < n; i++) {
            if (grow[i] <= 0) continue;
            float add = extra * (grow[i] / total_grow);
            if (fl->direction == MU_FLEX_ROW)
                sizes[i].x += add;
            else
                sizes[i].y += add;
        }
    } else if (extra < 0) {
        /* shrink: clamp simple */
        float scale = 1.0f;
        if (fl->direction == MU_FLEX_ROW && total_main > 0)
            scale = inner_w / total_main;
        else if (fl->direction == MU_FLEX_COLUMN && total_main > 0)
            scale = inner_h / total_main;
        for (int i = 0; i < n; i++) {
            if (fl->direction == MU_FLEX_ROW)
                sizes[i].x *= scale;
            else
                sizes[i].y *= scale;
        }
    }

    float pos = 0;
    float total_used = 0;
    for (int i = 0; i < n; i++) {
        if (!(container->children[i]->flags & MU_NODE_VISIBLE)) continue;
        if (fl->direction == MU_FLEX_ROW)
            total_used += sizes[i].x + (total_used > 0 ? fl->gap : 0);
        else
            total_used += sizes[i].y + (total_used > 0 ? fl->gap : 0);
    }

    float start_offset = 0;
    if (fl->direction == MU_FLEX_ROW) {
        if (fl->justify == MU_JUSTIFY_CENTER)
            start_offset = (inner_w - total_used) * 0.5f;
        else if (fl->justify == MU_JUSTIFY_END)
            start_offset = inner_w - total_used;
        else if (fl->justify == MU_JUSTIFY_SPACE_BETWEEN && n > 1)
            start_offset = 0;
    } else {
        if (fl->justify == MU_JUSTIFY_CENTER)
            start_offset = (inner_h - total_used) * 0.5f;
        else if (fl->justify == MU_JUSTIFY_END)
            start_offset = inner_h - total_used;
    }

    pos = start_offset;
    for (int i = 0; i < n; i++) {
        MuNode *ch = container->children[i];
        if (!(ch->flags & MU_NODE_VISIBLE)) continue;

        float mx = sizes[i].x;
        float my = sizes[i].y;
        float cx = inner_x, cy = inner_y, cw = mx, chh = my;

        if (fl->direction == MU_FLEX_ROW) {
            cx = inner_x + pos;
            cw = mx;
            chh = (fl->align_items == MU_ALIGN_STRETCH) ? inner_h : my;
            cy = inner_y;
            if (fl->align_items == MU_ALIGN_CENTER)
                cy = inner_y + (inner_h - chh) * 0.5f;
            else if (fl->align_items == MU_ALIGN_END)
                cy = inner_y + (inner_h - chh);
            pos += mx + fl->gap;
        } else {
            cy = inner_y + pos;
            chh = my;
            cw = (fl->align_items == MU_ALIGN_STRETCH) ? inner_w : mx;
            cx = inner_x;
            if (fl->align_items == MU_ALIGN_CENTER)
                cx = inner_x + (inner_w - cw) * 0.5f;
            else if (fl->align_items == MU_ALIGN_END)
                cx = inner_x + (inner_w - cw);
            pos += my + fl->gap;
        }

        ch->bounds.x = cx;
        ch->bounds.y = cy;
        ch->bounds.w = cw;
        ch->bounds.h = chh;

        const MuNodeOps *ops = mu_get_node_ops(ctx, ch->kind);
        if (ops && ops->layout_children) ops->layout_children(ctx, ch);
        else if (ch->flags & MU_NODE_FLEX_CONTAINER) mu_layout_flex_run(ctx, ch);
    }
}
