#include "../include/markup/mu_layout_flex.h"
#include "../include/markup/mu_layout.h"
#include <math.h>
#include <stddef.h>

/*
 * Flexbox layout.
 *
 * mu_layout_flex_run is a single-line CSS flexbox pass over one container:
 *
 *   1. measure intrinsics — ask each visible child for its desired size
 *   2. grow               — distribute positive free space by flex_grow
 *   2b. shrink            — absorb negative free space weighted by flex_shrink * size
 *   3. position           — place along the main axis per justify, cross axis per align
 *
 * Wrapping (flex_wrap on a row) is a separate path, layout_flex_wrap_run, which slices
 * children into lines and runs the same positioning per line. Column wrapping is not
 * supported.
 *
 * The one non-obvious step is in pass 1: a child with flex_grow > 0 is measured against
 * an *estimated* share of the container rather than the full available size. Without
 * this, a growing child that wraps text (a label) measures at full container width,
 * reports one tall line, and then gets grown again — so the container ends up sized for
 * text that will actually re-wrap narrower. The estimate is deliberately crude; it only
 * has to put the measurement in the right ballpark before pass 2 assigns real sizes.
 *
 * Scratch arrays come from ctx->scratch, the per-frame bump arena reset in mu_frame_begin,
 * so layout does no malloc. A container bails out rather than laying out if the arena is
 * exhausted.
 *
 * Sizes are computed main-axis-first via the is_row/main_size/set_main helpers, which is
 * why the row and column cases mostly share one code path instead of being duplicated.
 */

void mu_layout_init(MuLayoutStyle *ls) {
    if (!ls) return;
    ls->flex_grow = 0.f;
    ls->flex_shrink = 1.f;
    ls->flex_basis = MU_FLEX_BASIS_AUTO;
    ls->margin_top = ls->margin_right = ls->margin_bottom = ls->margin_left = 0.f;
    ls->min_width = ls->min_height = 0.f;
    ls->max_width = ls->max_height = 0.f;
    ls->align_self = MU_ALIGN_SELF_AUTO;
    ls->flex_direction = MU_FLEX_COLUMN;
    ls->justify_content = MU_JUSTIFY_START;
    ls->align_items = MU_ALIGN_STRETCH;
    ls->gap = 0.f;
    ls->flex_wrap = 0;
    ls->padding_top = ls->padding_right = ls->padding_bottom = ls->padding_left = 0.f;
}

void mu_layout_mark_dirty(MuNode *node) {
    /* Also mark paint on the node itself: anything worth re-laying-out is worth
     * repainting, and this covers most widget state changes without each setter having
     * to remember. Ancestors get only the layout bit — damaging their bounds too would
     * escalate every small change to the whole window. */
    if (node) node->flags |= MU_NODE_PAINT_DIRTY;
    for (MuNode *n = node; n; n = n->parent) n->flags |= MU_NODE_LAYOUT_DIRTY;
}

static bool rect_equal(MuRect a, MuRect b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

bool mu_node_set_bounds(MuNode *node, MuRect bounds) {
    if (!node) return false;
    if (rect_equal(node->bounds, bounds)) return false;
    node->bounds = bounds;
    mu_layout_mark_dirty(node);
    return true;
}

void mu_layout_set_padding(MuNode *node, float top, float right, float bottom, float left) {
    if (!node) return;
    node->layout.padding_top = top;
    node->layout.padding_right = right;
    node->layout.padding_bottom = bottom;
    node->layout.padding_left = left;
    mu_layout_mark_dirty(node);
}

void mu_layout_set_padding_all(MuNode *node, float value) {
    mu_layout_set_padding(node, value, value, value, value);
}

void mu_layout_set_margin(MuNode *node, float top, float right, float bottom, float left) {
    if (!node) return;
    node->layout.margin_top = top;
    node->layout.margin_right = right;
    node->layout.margin_bottom = bottom;
    node->layout.margin_left = left;
    mu_layout_mark_dirty(node);
}

void mu_layout_set_gap(MuNode *node, float gap) {
    if (!node) return;
    node->layout.gap = gap;
    mu_layout_mark_dirty(node);
}

void mu_layout_set_flex_direction(MuNode *node, MuFlexDirection dir) {
    if (!node) return;
    node->layout.flex_direction = dir;
    mu_layout_mark_dirty(node);
}

void mu_layout_set_justify(MuNode *node, MuFlexJustify justify) {
    if (!node) return;
    node->layout.justify_content = justify;
    mu_layout_mark_dirty(node);
}

void mu_layout_set_align_items(MuNode *node, MuFlexAlign align) {
    if (!node) return;
    node->layout.align_items = align;
    mu_layout_mark_dirty(node);
}

void mu_layout_set_align_self(MuNode *node, MuAlignSelf align) {
    if (!node) return;
    node->layout.align_self = align;
    mu_layout_mark_dirty(node);
}

void mu_layout_set_flex(MuNode *node, float grow, float shrink, float basis) {
    if (!node) return;
    node->layout.flex_grow = grow;
    node->layout.flex_shrink = shrink;
    node->layout.flex_basis = basis;
    mu_layout_mark_dirty(node);
}

void mu_layout_set_min_size(MuNode *node, float min_w, float min_h) {
    if (!node) return;
    node->layout.min_width = min_w;
    node->layout.min_height = min_h;
    mu_layout_mark_dirty(node);
}

void mu_layout_set_max_size(MuNode *node, float max_w, float max_h) {
    if (!node) return;
    node->layout.max_width = max_w;
    node->layout.max_height = max_h;
    mu_layout_mark_dirty(node);
}

void mu_layout_set_flex_wrap(MuNode *node, bool wrap) {
    if (!node) return;
    node->layout.flex_wrap = wrap ? 1 : 0;
    mu_layout_mark_dirty(node);
}

void mu_node_set_hit_transparent(MuNode *node, bool transparent) {
    if (!node) return;
    if (transparent)
        node->flags |= MU_NODE_HIT_TRANSPARENT;
    else
        node->flags &= ~MU_NODE_HIT_TRANSPARENT;
}

static void layout_subtree(MuContext *ctx, MuNode *node);

static bool is_row(const MuLayoutStyle *ls) {
    return ls->flex_direction == MU_FLEX_ROW;
}

static float main_size(MuVec2 v, bool row) {
    return row ? v.x : v.y;
}

static float cross_size(MuVec2 v, bool row) {
    return row ? v.y : v.x;
}

static void set_main(MuVec2 *v, float m, bool row) {
    if (row)
        v->x = m;
    else
        v->y = m;
}

static void set_cross(MuVec2 *v, float c, bool row) {
    if (row)
        v->y = c;
    else
        v->x = c;
}

static void clamp_main_size(MuVec2 *sz, const MuLayoutStyle *ls, bool row) {
    float m = main_size(*sz, row);
    float max_m = row ? ls->max_width : ls->max_height;
    float min_m = row ? ls->min_width : ls->min_height;
    if (max_m > 0.f && m > max_m) m = max_m;
    if (min_m > 0.f && m < min_m) m = min_m;
    set_main(sz, m, row);
}

static void clamp_vec(MuVec2 *d, const MuLayoutStyle *ls) {
    if (ls->min_width > 0.f && d->x < ls->min_width) d->x = ls->min_width;
    if (ls->min_height > 0.f && d->y < ls->min_height) d->y = ls->min_height;
    if (ls->max_width > 0.f && d->x > ls->max_width) d->x = ls->max_width;
    if (ls->max_height > 0.f && d->y > ls->max_height) d->y = ls->max_height;
}

static MuFlexAlign resolve_align_self(const MuLayoutStyle *child, const MuLayoutStyle *container) {
    switch (child->align_self) {
    case MU_ALIGN_SELF_START:
        return MU_ALIGN_START;
    case MU_ALIGN_SELF_CENTER:
        return MU_ALIGN_CENTER;
    case MU_ALIGN_SELF_END:
        return MU_ALIGN_END;
    case MU_ALIGN_SELF_STRETCH:
        return MU_ALIGN_STRETCH;
    default:
        return container->align_items;
    }
}

static MuVec2 measure_node(MuContext *ctx, MuNode *node, MuVec2 avail) {
    const MuNodeOps *ops = mu_get_node_ops(ctx, node->kind);
    MuVec2 d = {0, 0};
    if (ops && ops->measure) ops->measure(ctx, node, avail, &d);
    clamp_vec(&d, &node->layout);

    if (node->parent && (node->parent->flags & MU_NODE_FLEX_CONTAINER)) {
        const MuLayoutStyle *pl = &node->parent->layout;
        bool row = is_row(pl);
        if (node->layout.flex_basis >= 0.f) set_main(&d, node->layout.flex_basis, row);
    }
    return d;
}

static MuRect inner_content(MuRect bounds, const MuLayoutStyle *ls) {
    MuRect r = bounds;
    r.x += ls->padding_left;
    r.y += ls->padding_top;
    r.w -= ls->padding_left + ls->padding_right;
    r.h -= ls->padding_top + ls->padding_bottom;
    if (r.w < 0.f) r.w = 0.f;
    if (r.h < 0.f) r.h = 0.f;
    return r;
}

static void layout_subtree(MuContext *ctx, MuNode *node) {
    const MuNodeOps *ops = mu_get_node_ops(ctx, node->kind);
    if (ops && ops->layout_children)
        ops->layout_children(ctx, node);
    else if (node->flags & MU_NODE_FLEX_CONTAINER)
        mu_layout_flex_run(ctx, node);
    node->flags &= ~MU_NODE_LAYOUT_DIRTY;
}

void mu_layout_node(MuContext *ctx, MuNode *node) {
    if (!ctx || !node) return;
    if (!(node->flags & MU_NODE_LAYOUT_DIRTY)) return;
    layout_subtree(ctx, node);
}

void mu_layout_measure_container(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    if (!out) return;
    out->x = 20.f;
    out->y = 20.f;
    if (!ctx || !node) return;

    const MuLayoutStyle *fl = &node->layout;
    bool row = is_row(fl);
    float pad_w = fl->padding_left + fl->padding_right;
    float pad_h = fl->padding_top + fl->padding_bottom;

    if (fl->flex_wrap && row && avail.x > 0.f) {
        float inner_w = avail.x - pad_w;
        if (inner_w < 1.f) inner_w = avail.x;

        float line_w = 0.f;
        float line_h = 0.f;
        float max_w = 0.f;
        float total_h = pad_h;
        bool first_on_line = true;

        for (int i = 0; i < node->child_count; i++) {
            MuNode *ch = node->children[i];
            if (!(ch->flags & MU_NODE_VISIBLE)) continue;

            MuVec2 child_avail = {inner_w, avail.y > 0.f ? avail.y - pad_h : 0.f};
            MuVec2 d = measure_node(ctx, ch, child_avail);
            float ml = ch->layout.margin_left + ch->layout.margin_right;
            float mt = ch->layout.margin_top + ch->layout.margin_bottom;
            float need = d.x + ml;

            if (!first_on_line && line_w + fl->gap + need > inner_w && line_w > 0.f) {
                total_h += line_h + fl->gap;
                if (line_w > max_w) max_w = line_w;
                line_w = 0.f;
                line_h = 0.f;
                first_on_line = true;
            }

            if (!first_on_line) line_w += fl->gap;
            line_w += need;
            float ch_h = d.y + mt;
            if (ch_h > line_h) line_h = ch_h;
            first_on_line = false;
        }

        if (line_w > 0.f || line_h > 0.f) {
            total_h += line_h;
            if (line_w > max_w) max_w = line_w;
        }

        out->x = max_w + pad_w;
        out->y = total_h;
        if (avail.x > 0.f && out->x < avail.x) out->x = avail.x;
        clamp_vec(out, fl);
        return;
    }

    float mw = 20.f, mh = 20.f;
    for (int i = 0; i < node->child_count; i++) {
        MuNode *ch = node->children[i];
        if (!(ch->flags & MU_NODE_VISIBLE)) continue;
        MuVec2 d = measure_node(ctx, ch, avail);
        if (row) {
            mw += d.x + fl->gap;
            if (d.y > mh) mh = d.y;
        } else {
            mh += d.y + fl->gap;
            if (d.x > mw) mw = d.x;
        }
    }
    out->x = mw + pad_w;
    out->y = mh + pad_h;
    clamp_vec(out, fl);
}

static void layout_flex_wrap_run(MuContext *ctx, MuNode *container) {
    const MuLayoutStyle *fl = &container->layout;
    MuRect inner = inner_content(container->bounds, fl);
    int n = container->child_count;
    if (n <= 0) return;

    float x = inner.x;
    float y = inner.y;
    float line_h = 0.f;
    bool first_on_line = true;

    for (int i = 0; i < n; i++) {
        MuNode *ch = container->children[i];
        if (!(ch->flags & MU_NODE_VISIBLE)) continue;

        MuVec2 child_avail = {inner.w, inner.h};
        MuVec2 sz = measure_node(ctx, ch, child_avail);
        float ml = ch->layout.margin_left;
        float mr = ch->layout.margin_right;
        float mt = ch->layout.margin_top;
        float need = sz.x + ml + mr;

        if (!first_on_line && x + need > inner.x + inner.w && x > inner.x) {
            y += line_h + fl->gap;
            x = inner.x;
            line_h = 0.f;
            first_on_line = true;
        }

        if (!first_on_line) x += fl->gap;
        ch->bounds.x = x + ml;
        ch->bounds.y = y + mt;
        ch->bounds.w = sz.x;
        ch->bounds.h = sz.y;
        x += need;
        float row_h = sz.y + mt + ch->layout.margin_bottom;
        if (row_h > line_h) line_h = row_h;
        first_on_line = false;

        layout_subtree(ctx, ch);
    }
}

void mu_layout_flex_run(MuContext *ctx, MuNode *container) {
    if (!container || !(container->flags & MU_NODE_FLEX_CONTAINER)) return;

    const MuLayoutStyle *fl = &container->layout;
    if (fl->flex_wrap && is_row(fl)) {
        layout_flex_wrap_run(ctx, container);
        return;
    }

    bool row = is_row(fl);
    MuRect inner = inner_content(container->bounds, fl);

    int n = container->child_count;
    if (n <= 0) return;
    if (!ctx->scratch) return;

    MuVec2 *sizes =
        (MuVec2 *)mu_arena_alloc(ctx->scratch, sizeof(MuVec2) * (size_t)n, _Alignof(MuVec2));
    float *grow = (float *)mu_arena_alloc(ctx->scratch, sizeof(float) * (size_t)n, _Alignof(float));
    float *shrink = (float *)mu_arena_alloc(ctx->scratch, sizeof(float) * (size_t)n, _Alignof(float));
    int *visible = (int *)mu_arena_alloc(ctx->scratch, sizeof(int) * (size_t)n, _Alignof(int));
    if (!sizes || !grow || !shrink || !visible) return;

    MuVec2 avail = {inner.w, inner.h};

    /* How many visible children want to grow — used to estimate each one's share while
     * measuring in pass 1 (see file header). */
    int grow_vis = 0;
    for (int i = 0; i < n; i++) {
        MuNode *ch = container->children[i];
        if (!(ch->flags & MU_NODE_VISIBLE)) continue;
        if (ch->layout.flex_grow > 0.f) grow_vis++;
    }

    /* Pass 1 — measure intrinsics */
    for (int i = 0; i < n; i++) {
        MuNode *ch = container->children[i];
        if (!(ch->flags & MU_NODE_VISIBLE)) {
            visible[i] = 0;
            sizes[i].x = sizes[i].y = 0.f;
            grow[i] = shrink[i] = 0.f;
            continue;
        }
        visible[i] = 1;

        MuVec2 child_avail = avail;
        if (row)
            child_avail.x -= ch->layout.margin_left + ch->layout.margin_right;
        else
            child_avail.y -= ch->layout.margin_top + ch->layout.margin_bottom;

        if (ch->layout.flex_grow > 0.f && grow_vis > 0) {
            float inner_main = row ? inner.w : inner.h;
            if (inner_main > 0.f) {
                float gaps = fl->gap * (float)(n > 1 ? n - 1 : 0);
                float share = (inner_main - gaps) / (float)grow_vis;
                if (share > 0.f) {
                    if (row)
                        child_avail.x = share;
                    else
                        child_avail.y = share;
                }
            }
        }

        sizes[i] = measure_node(ctx, ch, child_avail);
        grow[i] = ch->layout.flex_grow > 0.f ? ch->layout.flex_grow : 0.f;
        shrink[i] = ch->layout.flex_shrink > 0.f ? ch->layout.flex_shrink : 0.f;
    }

    /* Totals over the measured sizes, including gaps and margins. */
    int vis_count = 0;
    float total_main = 0.f;
    float total_grow = 0.f;
    float total_shrink_score = 0.f;
    for (int i = 0; i < n; i++) {
        if (!visible[i]) continue;
        MuNode *ch = container->children[i];
        float main = main_size(sizes[i], row);
        float ml = row ? ch->layout.margin_left + ch->layout.margin_right
                       : ch->layout.margin_top + ch->layout.margin_bottom;
        if (vis_count > 0) total_main += fl->gap;
        total_main += main + ml;
        total_grow += grow[i];
        if (shrink[i] > 0.f && main > 0.f) total_shrink_score += shrink[i] * main;
        vis_count++;
    }

    if (vis_count <= 0) return;

    float inner_main = row ? inner.w : inner.h;

    float free_space = inner_main - total_main;

    /* Pass 2 — grow */
    if (free_space > 0.f && total_grow > 0.f) {
        for (int i = 0; i < n; i++) {
            if (!visible[i] || grow[i] <= 0.f) continue;
            float add = free_space * (grow[i] / total_grow);
            set_main(&sizes[i], main_size(sizes[i], row) + add, row);
            clamp_main_size(&sizes[i], &container->children[i]->layout, row);
        }
    }

    /* Pass 2b — shrink */
    if (free_space < 0.f && total_shrink_score > 0.f) {
        float overflow = -free_space;
        for (int i = 0; i < n; i++) {
            if (!visible[i] || shrink[i] <= 0.f) continue;
            float main = main_size(sizes[i], row);
            if (main <= 0.f) continue;
            float score = shrink[i] * main;
            float sub = overflow * (score / total_shrink_score);
            float next = main - sub;
            if (next < 0.f) next = 0.f;
            set_main(&sizes[i], next, row);
            clamp_main_size(&sizes[i], &container->children[i]->layout, row);
        }
    }

    /* Recompute used main for justify */
    total_main = 0.f;
    vis_count = 0;
    for (int i = 0; i < n; i++) {
        if (!visible[i]) continue;
        MuNode *ch = container->children[i];
        float ml = row ? ch->layout.margin_left + ch->layout.margin_right
                       : ch->layout.margin_top + ch->layout.margin_bottom;
        total_main += main_size(sizes[i], row) + ml;
        if (vis_count > 0) total_main += fl->gap;
        vis_count++;
    }

    float start_offset = 0.f;
    float between = fl->gap;
    if (vis_count > 1 && fl->justify_content == MU_JUSTIFY_SPACE_BETWEEN) {
        between = (inner_main - total_main + fl->gap * (float)(vis_count - 1)) / (float)(vis_count - 1);
        if (between < 0.f) between = 0.f;
    } else if (fl->justify_content == MU_JUSTIFY_SPACE_AROUND && vis_count > 0) {
        float extra = inner_main - total_main;
        if (extra > 0.f) {
            float slot = extra / (float)vis_count;
            start_offset = slot * 0.5f;
            between = fl->gap + slot;
        }
    } else if (row) {
        if (fl->justify_content == MU_JUSTIFY_CENTER)
            start_offset = (inner_main - total_main) * 0.5f;
        else if (fl->justify_content == MU_JUSTIFY_END)
            start_offset = inner_main - total_main;
    } else {
        if (fl->justify_content == MU_JUSTIFY_CENTER)
            start_offset = (inner_main - total_main) * 0.5f;
        else if (fl->justify_content == MU_JUSTIFY_END)
            start_offset = inner_main - total_main;
    }
    if (start_offset < 0.f) start_offset = 0.f;

    /* Pass 3 — place */
    float pos = start_offset;
    bool first_placed = true;
    for (int i = 0; i < n; i++) {
        MuNode *ch = container->children[i];
        if (!visible[i]) continue;

        float main = main_size(sizes[i], row);
        float cross = cross_size(sizes[i], row);
        float m_start = row ? ch->layout.margin_left : ch->layout.margin_top;
        float m_end = row ? ch->layout.margin_right : ch->layout.margin_bottom;

        if (!first_placed) {
            if (fl->justify_content == MU_JUSTIFY_SPACE_BETWEEN ||
                fl->justify_content == MU_JUSTIFY_SPACE_AROUND)
                pos += between;
            else
                pos += fl->gap;
        }
        first_placed = false;

        MuFlexAlign align = resolve_align_self(&ch->layout, fl);
        float inner_cross = row ? inner.h : inner.w;
        float cross_dim = (align == MU_ALIGN_STRETCH) ? inner_cross : cross;

        float cx, cy, cw, chh;
        if (row) {
            cx = inner.x + pos + m_start;
            cw = sizes[i].x;
            chh = cross_dim;
            cy = inner.y + ch->layout.margin_top;
            if (align == MU_ALIGN_CENTER)
                cy = inner.y + (inner_cross - chh) * 0.5f;
            else if (align == MU_ALIGN_END)
                cy = inner.y + inner_cross - chh - ch->layout.margin_bottom;
        } else {
            cy = inner.y + pos + m_start;
            chh = sizes[i].y;
            cw = cross_dim;
            cx = inner.x + ch->layout.margin_left;
            if (align == MU_ALIGN_CENTER)
                cx = inner.x + (inner_cross - cw) * 0.5f;
            else if (align == MU_ALIGN_END)
                cx = inner.x + inner_cross - cw - ch->layout.margin_right;
        }

        pos += m_start + main + m_end;

        ch->bounds.x = cx;
        ch->bounds.y = cy;
        ch->bounds.w = cw;
        ch->bounds.h = chh;

        layout_subtree(ctx, ch);
    }
}

void mu_layout_run(MuContext *ctx) {
    if (!ctx || !ctx->root) return;
    mu_layout_node(ctx, ctx->root);
}
