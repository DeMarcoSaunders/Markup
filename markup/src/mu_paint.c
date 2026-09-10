#include "../include/markup/mu_render.h"

#include <math.h>

/*
 * Painting and damage tracking.
 *
 * A frame repaints the whole tree clipped to one union damage rect. Per-node regions
 * would be tighter, but repainting everything intersecting the rect in normal order is
 * what keeps overlap and transparency correct: a node is never redrawn without whatever
 * shows through beneath it being redrawn too.
 *
 * Damage is mostly *discovered* rather than declared. mu_damage_collect compares each
 * node's bounds and style flags against the previous frame, so movement, resizing,
 * visibility and hover/press/focus are picked up without any call site remembering to
 * mark. That matters because the failure mode of a missed invalidation — stale pixels
 * that persist until something else happens to overlap them — is miserable to debug.
 * Only appearance changes invisible to that comparison (a slider value, a checkbox
 * toggle) need mu_node_mark_paint_dirty.
 *
 * ctx->damage_force_all disables all of it and repaints everything, which is the
 * reference to A/B against when a stale pixel is suspected.
 */

static bool rect_valid(MuRect r) {
    return r.w > 0.f && r.h > 0.f;
}

static bool rect_equal(MuRect a, MuRect b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static bool rect_overlaps(MuRect a, MuRect b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

void mu_damage_add(MuContext *ctx, MuRect r) {
    if (!ctx || !rect_valid(r)) return;
    if (!ctx->damage_any) {
        ctx->damage = r;
        ctx->damage_any = true;
        return;
    }
    float x0 = ctx->damage.x < r.x ? ctx->damage.x : r.x;
    float y0 = ctx->damage.y < r.y ? ctx->damage.y : r.y;
    float x1 = (ctx->damage.x + ctx->damage.w) > (r.x + r.w) ? (ctx->damage.x + ctx->damage.w) : (r.x + r.w);
    float y1 = (ctx->damage.y + ctx->damage.h) > (r.y + r.h) ? (ctx->damage.y + ctx->damage.h) : (r.y + r.h);
    ctx->damage = (MuRect){x0, y0, x1 - x0, y1 - y0};
}

void mu_damage_all(MuContext *ctx) {
    if (!ctx) return;
    ctx->damage_any = true;
    /* Large enough to cover any surface; the backend intersects with its own bounds. */
    ctx->damage = (MuRect){-1.0e6f, -1.0e6f, 2.0e6f, 2.0e6f};
}

void mu_node_mark_paint_dirty(MuNode *node) {
    if (node) node->flags |= MU_NODE_PAINT_DIRTY;
}

void mu_node_set_backdrop_blur(MuNode *node, float radius) {
    if (!node) return;
    node->backdrop_blur = radius > 0.f ? radius : 0.f;
    /* BACKDROP_DIRTY as well as PAINT_DIRTY: a node that has just been given a blur has
     * nothing cached, and one whose radius changed has a cache built at the old radius. */
    node->flags |= MU_NODE_PAINT_DIRTY | MU_NODE_BACKDROP_DIRTY;
}

bool mu_damage_empty(const MuContext *ctx) {
    return !ctx || !ctx->damage_any || !rect_valid(ctx->damage);
}

MuRect mu_damage_rect(const MuContext *ctx) {
    if (!ctx || !ctx->damage_any) return (MuRect){0.f, 0.f, 0.f, 0.f};
    return ctx->damage;
}

void mu_damage_reset(MuContext *ctx) {
    if (!ctx) return;
    ctx->damage_any = false;
    ctx->damage = (MuRect){0.f, 0.f, 0.f, 0.f};
}

/** Register every blurred node, so collect_node can attribute damage relative to each. */
static void gather_backdrops(MuContext *ctx, MuNode *n) {
    if (!n) return;
    if (n->backdrop_blur > 0.f) {
        if (ctx->backdrop_count >= MU_BACKDROP_TRACK_MAX) {
            ctx->backdrop_overflow = true;
        } else {
            MuBackdropTrack *t = &ctx->backdrop[ctx->backdrop_count++];
            t->node = n;
            t->outside = (MuRect){0.f, 0.f, 0.f, 0.f};
            t->outside_any = false;
        }
    }
    for (int i = 0; i < n->child_count; i++) gather_backdrops(ctx, n->children[i]);
}

/** Union `r` into every tracked backdrop whose subtree we are *not* currently inside. */
static void backdrop_outside_add(MuContext *ctx, uint32_t inside, MuRect r) {
    if (!rect_valid(r)) return;
    for (int i = 0; i < ctx->backdrop_count; i++) {
        if (inside & (1u << i)) continue;
        MuBackdropTrack *t = &ctx->backdrop[i];
        if (!t->outside_any) {
            t->outside = r;
            t->outside_any = true;
            continue;
        }
        float x0 = t->outside.x < r.x ? t->outside.x : r.x;
        float y0 = t->outside.y < r.y ? t->outside.y : r.y;
        float x1 = (t->outside.x + t->outside.w) > (r.x + r.w) ? (t->outside.x + t->outside.w) : (r.x + r.w);
        float y1 = (t->outside.y + t->outside.h) > (r.y + r.h) ? (t->outside.y + t->outside.h) : (r.y + r.h);
        t->outside = (MuRect){x0, y0, x1 - x0, y1 - y0};
    }
}

/*
 * `inside` is a bitmask of the tracked backdrops whose subtree this node sits in. A node
 * is considered inside its own entry: a glass panel's own hover changes how it paints,
 * but it paints on top of its blur, so it does not invalidate the blur input.
 */
static void collect_node(MuContext *ctx, MuNode *n, uint32_t inside) {
    if (!n) return;

    uint32_t style = n->flags & MU_NODE_STYLE_FLAGS;
    bool visible = (n->flags & MU_NODE_VISIBLE) != 0;
    bool was_visible = (n->prev_flags & MU_NODE_VISIBLE) != 0;

    bool changed = (n->flags & MU_NODE_PAINT_DIRTY) != 0 || style != (n->prev_flags & MU_NODE_STYLE_FLAGS) ||
                   !rect_equal(n->bounds, n->prev_bounds);

    /* A node counts as inside its own entry: a glass panel's own hover changes how it
     * paints, but it paints on top of its blur, so it cannot invalidate the blur input. */
    if (n->backdrop_blur > 0.f) {
        for (int i = 0; i < ctx->backdrop_count; i++) {
            if (ctx->backdrop[i].node == n) {
                inside |= (1u << i);
                break;
            }
        }
    }

    if (changed) {
        /* Both regions: whatever the node used to cover has to be repainted too, or a
         * move leaves a copy of the node behind at its old position. */
        if (was_visible) {
            mu_damage_add(ctx, n->prev_bounds);
            backdrop_outside_add(ctx, inside, n->prev_bounds);
        }
        if (visible) {
            mu_damage_add(ctx, n->bounds);
            backdrop_outside_add(ctx, inside, n->bounds);
        }
    }

    n->prev_bounds = n->bounds;
    n->prev_flags = style;
    n->flags &= ~MU_NODE_PAINT_DIRTY;

    /* Descend even into hidden subtrees: a child that was visible last frame still owes
     * damage for the area it is vacating. */
    for (int i = 0; i < n->child_count; i++) collect_node(ctx, n->children[i], inside);
}

static bool rect_contains(MuRect outer, MuRect inner) {
    return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.w <= outer.x + outer.w &&
           inner.y + inner.h <= outer.y + outer.h;
}

static MuRect rect_inflate(MuRect r, float m) {
    return (MuRect){r.x - m, r.y - m, r.w + 2.f * m, r.h + 2.f * m};
}

/**
 * Grow the damage to fully contain any backdrop-blurred node it touches.
 *
 * Two reasons, both fatal if skipped. A blurred node's appearance depends on content it
 * does not own, so the snapshot comparison in collect_node cannot see that it needs
 * repainting when something behind it moves. And the blur kernel samples beyond the
 * damage rect, where pixels still hold the *previous frame's composited* result — which
 * already contains this node's own blur. Feeding that back in smears a little more every
 * frame. Covering the node and its reach means the region gets cleared and repainted from
 * the backdrop up, which breaks the loop.
 *
 * Iterated because one expansion can reach another blurred node; it settles immediately
 * in practice, and the pass cap stops a pathological arrangement from spinning.
 */
static void expand_for_backdrops(MuContext *ctx, MuNode *n, bool *grew) {
    if (!n) return;
    if (n->backdrop_blur > 0.f && (n->flags & MU_NODE_VISIBLE) &&
        rect_overlaps(n->bounds, ctx->damage)) {
        MuRect needed = rect_inflate(n->bounds, n->backdrop_blur + 1.f);
        if (!rect_contains(ctx->damage, needed)) {
            mu_damage_add(ctx, needed);
            *grew = true;
        }
    }
    for (int i = 0; i < n->child_count; i++) expand_for_backdrops(ctx, n->children[i], grew);
}

/**
 * Record, on each blurred node, whether this frame's damage touched what its blur reads.
 *
 * The verdict is latched onto the node rather than left in the per-frame track, because
 * the node may not repaint for several frames after the damage that invalidated it.
 */
static void mark_backdrops_dirty(MuContext *ctx) {
    for (int i = 0; i < ctx->backdrop_count; i++) {
        MuBackdropTrack *t = &ctx->backdrop[i];
        if (!t->node || !t->outside_any) continue;
        /* The kernel reads past the node's edge, so damage merely adjacent to it still
         * changes the result. Same reach expand_for_backdrops uses. */
        MuRect reach = rect_inflate(t->node->bounds, t->node->backdrop_blur + 1.f);
        if (rect_overlaps(reach, t->outside)) t->node->flags |= MU_NODE_BACKDROP_DIRTY;
    }
}

void mu_node_backdrop_mark_clean(MuNode *node) {
    if (node) node->flags &= ~MU_NODE_BACKDROP_DIRTY;
}

bool mu_node_backdrop_unchanged(const MuContext *ctx, const MuNode *node) {
    if (!ctx || !node) return false;
    /* The full-repaint reference must never be served from a cache, or the A/B check that
     * validates the cache would be comparing it against itself. Overflow means some
     * blurred nodes were never tracked, so no verdict here can be trusted. */
    if (ctx->damage_force_all || ctx->backdrop_overflow) return false;
    return (node->flags & MU_NODE_BACKDROP_DIRTY) == 0;
}

void mu_damage_collect(MuContext *ctx) {
    if (!ctx) return;
    if (ctx->damage_force_all) {
        mu_damage_all(ctx);
        /* Still walk, so snapshots stay current and switching the flag off mid-run does
         * not report a frame's worth of spurious damage. */
    }

    ctx->backdrop_count = 0;
    ctx->backdrop_overflow = false;
    gather_backdrops(ctx, ctx->root);
    gather_backdrops(ctx, ctx->popup_layer);
    gather_backdrops(ctx, ctx->modal_layer);

    collect_node(ctx, ctx->root, 0u);
    collect_node(ctx, ctx->popup_layer, 0u);
    collect_node(ctx, ctx->modal_layer, 0u);

    mark_backdrops_dirty(ctx);

    if (ctx->damage_any) {
        for (int pass = 0; pass < 4; pass++) {
            bool grew = false;
            expand_for_backdrops(ctx, ctx->root, &grew);
            expand_for_backdrops(ctx, ctx->popup_layer, &grew);
            expand_for_backdrops(ctx, ctx->modal_layer, &grew);
            if (!grew) break;
        }
    }
}

/** Paint a subtree, skipping anything that cannot touch `area`. */
static void paint_tree_clipped(MuContext *ctx, MuRenderContext *rc, MuNode *node, MuRect area) {
    if (!node || !(node->flags & MU_NODE_VISIBLE)) return;
    /* A clipping container confines its children, so it can be pruned wholesale.
     * Non-clipping nodes are only pruned on their own bounds; children are tested
     * individually because they may extend beyond the parent. */
    if ((node->flags & MU_NODE_CLIP_CHILDREN) && !rect_overlaps(node->bounds, area)) return;

    const MuNodeOps *ops = mu_get_node_ops(ctx, node->kind);
    bool self_visible = rect_overlaps(node->bounds, area);

    if (node->flags & MU_NODE_CLIP_CHILDREN) mu_push_scissor(rc, node->bounds);
    if (self_visible && ops && ops->paint) ops->paint(ctx, node, rc);

    for (int i = 0; i < node->child_count; i++) paint_tree_clipped(ctx, rc, node->children[i], area);

    if (self_visible && ops && ops->paint_overlay) ops->paint_overlay(ctx, node, rc);
    if (node->flags & MU_NODE_CLIP_CHILDREN) mu_pop_scissor(rc);
}

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

void mu_paint_damaged(MuContext *ctx, MuRenderContext *rc) {
    if (!ctx || !rc) return;
    if (mu_damage_empty(ctx)) return;

    MuRect area = ctx->damage;
    mu_push_scissor(rc, area);
    if (ctx->root) paint_tree_clipped(ctx, rc, ctx->root, area);
    if (ctx->popup_layer && (ctx->popup_layer->flags & MU_NODE_VISIBLE))
        paint_tree_clipped(ctx, rc, ctx->popup_layer, area);
    if (ctx->modal_layer && (ctx->modal_layer->flags & MU_NODE_VISIBLE))
        paint_tree_clipped(ctx, rc, ctx->modal_layer, area);
    mu_pop_scissor(rc);
}
