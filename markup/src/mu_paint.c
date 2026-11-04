#include "../include/markup/mu_render.h"

#include <math.h>

/*
 * Painting and damage tracking.
 *
 * A frame repaints the whole tree once per damage rect, clipped to that rect. Per-node
 * regions would be tighter, but repainting everything intersecting a rect in normal order
 * is what keeps overlap and transparency correct: a node is never redrawn without
 * whatever shows through beneath it being redrawn too. Across rects it is MuRegion's
 * disjointness that keeps it correct -- a pixel in two rects would be painted twice.
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

static bool rect_contains(MuRect outer, MuRect inner) {
    return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.w <= outer.x + outer.w &&
           inner.y + inner.h <= outer.y + outer.h;
}

/* Overlapping *or* merely edge-adjacent. Only disjointness is required for correctness,
 * not contact-merging; this is here because stacked widgets routinely share an edge
 * exactly, and keeping those apart would spend the capacity on rectangles that cost the
 * same to clear and paint as one. */
static bool rect_touches(MuRect a, MuRect b) {
    return a.x <= b.x + b.w && b.x <= a.x + a.w && a.y <= b.y + b.h && b.y <= a.y + a.h;
}

static MuRect rect_union(MuRect a, MuRect b) {
    float x0 = a.x < b.x ? a.x : b.x;
    float y0 = a.y < b.y ? a.y : b.y;
    float ax1 = a.x + a.w, bx1 = b.x + b.w;
    float ay1 = a.y + a.h, by1 = b.y + b.h;
    float x1 = ax1 > bx1 ? ax1 : bx1;
    float y1 = ay1 > by1 ? ay1 : by1;
    return (MuRect){x0, y0, x1 - x0, y1 - y0};
}

static float rect_area(MuRect r) {
    return r.w * r.h;
}

/* ------------------------------------------------------------------ */
/* MuRegion */

void mu_region_clear(MuRegion *rg) {
    if (rg) rg->count = 0;
}

bool mu_region_empty(const MuRegion *rg) {
    return !rg || rg->count <= 0;
}

int mu_region_count(const MuRegion *rg) {
    return rg ? rg->count : 0;
}

MuRect mu_region_rect(const MuRegion *rg, int i) {
    if (!rg || i < 0 || i >= rg->count) return (MuRect){0.f, 0.f, 0.f, 0.f};
    return rg->rects[i];
}

MuRect mu_region_bounds(const MuRegion *rg) {
    if (mu_region_empty(rg)) return (MuRect){0.f, 0.f, 0.f, 0.f};
    MuRect out = rg->rects[0];
    for (int i = 1; i < rg->count; i++) out = rect_union(out, rg->rects[i]);
    return out;
}

bool mu_region_overlaps(const MuRegion *rg, MuRect r) {
    if (!rg) return false;
    for (int i = 0; i < rg->count; i++)
        if (rect_overlaps(rg->rects[i], r)) return true;
    return false;
}

/** True if a single rect in the region covers `r` outright. */
static bool region_contains_rect(const MuRegion *rg, MuRect r) {
    if (!rg) return false;
    for (int i = 0; i < rg->count; i++)
        if (rect_contains(rg->rects[i], r)) return true;
    return false;
}

/** Drop rect `i`, filling the hole from the end. Order carries no meaning here. */
static void region_remove(MuRegion *rg, int i) {
    rg->rects[i] = rg->rects[--rg->count];
}

/**
 * Absorb into `*r` every stored rect it touches, cascading, then append the survivor.
 *
 * Each absorption grows `*r`, which can bring a rect into reach that the original did
 * not touch, so the scan restarts until nothing more merges. Returns false when the
 * region is full and `*r` -- by then the fully merged survivor -- still needs a slot.
 */
static bool region_insert(MuRegion *rg, MuRect *r) {
    if (region_contains_rect(rg, *r)) return true;

    bool merged = true;
    while (merged) {
        merged = false;
        for (int i = 0; i < rg->count; i++) {
            if (!rect_touches(rg->rects[i], *r)) continue;
            *r = rect_union(*r, rg->rects[i]);
            region_remove(rg, i);
            merged = true;
            break;
        }
    }

    if (rg->count >= MU_DAMAGE_MAX_RECTS) return false;
    rg->rects[rg->count++] = *r;
    return true;
}

/**
 * Of the stored rects plus the pending one, the pair whose union wastes the least area.
 *
 * Waste is the union area less the two rects own areas, which is exact because every
 * candidate here is disjoint from every other. `*out_j` comes back as rg->count to mean
 * the pending rect.
 */
static void region_cheapest_pair(const MuRegion *rg, MuRect pending, int *out_i, int *out_j) {
    int n = rg->count + 1;
    float best = -1.f;
    *out_i = 0;
    *out_j = n - 1;
    for (int i = 0; i < n; i++) {
        MuRect a = (i == rg->count) ? pending : rg->rects[i];
        for (int j = i + 1; j < n; j++) {
            MuRect b = (j == rg->count) ? pending : rg->rects[j];
            float waste = rect_area(rect_union(a, b)) - rect_area(a) - rect_area(b);
            if (best < 0.f || waste < best) {
                best = waste;
                *out_i = i;
                *out_j = j;
            }
        }
    }
}

void mu_region_add(MuRegion *rg, MuRect r) {
    if (!rg || !rect_valid(r)) return;

    /* Two rects can be waiting at once, and no more: the capacity path below frees two
     * slots when it merges two stored rects, and the rect it displaced goes back here
     * rather than being dropped. */
    MuRect pending[2];
    int np = 0;
    pending[np++] = r;

    /* Each round either places a pending rect or frees a slot, which settles in three.
     * The bound is generous, and the fallback after it is the honest degradation if that
     * reasoning is ever wrong -- never a silently dropped rect. */
    for (int guard = 0; guard < 8 && np > 0; guard++) {
        MuRect cur = pending[--np];
        if (region_insert(rg, &cur)) continue;

        int i, j;
        region_cheapest_pair(rg, cur, &i, &j);
        if (j == rg->count) {
            /* The cheapest merge involves the rect we are trying to place. */
            MuRect m = rect_union(rg->rects[i], cur);
            region_remove(rg, i);
            pending[np++] = m;
        } else {
            /* Two stored rects merge instead, and `cur` still needs a home. The merged
             * box can reach a third rect that neither half touched, so it goes back
             * through region_insert rather than straight into the array. */
            MuRect m = rect_union(rg->rects[i], rg->rects[j]);
            region_remove(rg, j); /* j > i, so removing j first leaves i valid */
            region_remove(rg, i);
            pending[np++] = cur;
            pending[np++] = m;
        }
    }

    /* Unreachable in practice. Collapsing to the bounding box of everything, stored and
     * still pending, is exactly the single-rect behaviour this type replaces: coarse,
     * but it covers every pixel that was asked for. */
    if (np > 0) {
        MuRect all = (rg->count > 0) ? mu_region_bounds(rg) : pending[0];
        while (np > 0) all = rect_union(all, pending[--np]);
        rg->count = 1;
        rg->rects[0] = all;
    }
}

/* ------------------------------------------------------------------ */

void mu_damage_add(MuContext *ctx, MuRect r) {
    if (!ctx) return;
    mu_region_add(&ctx->damage, r);
}

void mu_damage_all(MuContext *ctx) {
    if (!ctx) return;
    /* One rect large enough to cover any surface; the backend intersects with its own
     * bounds. Clearing first because it contains everything collected so far, so leaving
     * those in place would only cost a containment test on every later add. */
    mu_region_clear(&ctx->damage);
    mu_region_add(&ctx->damage, (MuRect){-1.0e6f, -1.0e6f, 2.0e6f, 2.0e6f});
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
    return !ctx || mu_region_empty(&ctx->damage);
}

const MuRegion *mu_damage_region(const MuContext *ctx) {
    return ctx ? &ctx->damage : NULL;
}

MuRect mu_damage_rect(const MuContext *ctx) {
    if (!ctx) return (MuRect){0.f, 0.f, 0.f, 0.f};
    return mu_region_bounds(&ctx->damage);
}

void mu_damage_reset(MuContext *ctx) {
    if (!ctx) return;
    mu_region_clear(&ctx->damage);
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

static MuRect rect_shift(MuRect r, MuVec2 off) {
    r.x += off.x;
    r.y += off.y;
    return r;
}

/*
 * `inside` is a bitmask of the tracked backdrops whose subtree this node sits in. A node
 * is considered inside its own entry: a glass panel's own hover changes how it paints,
 * but it paints on top of its blur, so it does not invalidate the blur input.
 *
 * `off` is the paint_offset accumulated from the ancestors. Damage is about pixels, so it
 * is the *painted* rect that gets compared, not the layout one: a button lifting on hover
 * never changes its bounds at all, only where it is drawn.
 */
static void collect_node(MuContext *ctx, MuNode *n, uint32_t inside, MuVec2 off) {
    if (!n) return;

    /* Step the node's own motion before looking at it, so a change made here is damaged
     * this frame. Stepping during paint instead would draw outside the rect that was
     * cleared for it — paint only ever covers what collect already reported. */
    const MuNodeOps *ops = mu_get_node_ops(ctx, n->kind);
    if (ops && ops->animate && ops->animate(ctx, n, ctx->frame_dt)) ctx->animating++;

    off.x += n->paint_offset.x;
    off.y += n->paint_offset.y;
    MuRect painted = rect_shift(n->bounds, off);

    uint32_t style = n->flags & MU_NODE_STYLE_FLAGS;
    bool visible = (n->flags & MU_NODE_VISIBLE) != 0;
    bool was_visible = (n->prev_flags & MU_NODE_VISIBLE) != 0;

    bool changed = (n->flags & MU_NODE_PAINT_DIRTY) != 0 || style != (n->prev_flags & MU_NODE_STYLE_FLAGS) ||
                   !rect_equal(painted, n->prev_bounds);

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
            mu_damage_add(ctx, painted);
            backdrop_outside_add(ctx, inside, painted);
        }
    }

    n->prev_bounds = painted;
    n->prev_flags = style;
    n->flags &= ~MU_NODE_PAINT_DIRTY;

    /* Descend even into hidden subtrees: a child that was visible last frame still owes
     * damage for the area it is vacating. */
    for (int i = 0; i < n->child_count; i++) collect_node(ctx, n->children[i], inside, off);
}

static MuRect rect_inflate(MuRect r, float m) {
    return (MuRect){r.x - m, r.y - m, r.w + 2.f * m, r.h + 2.f * m};
}

/**
 * Grow the damage until a *single* rect fully contains any backdrop-blurred node it
 * touches, plus that node's blur reach.
 *
 * Two reasons, both fatal if skipped. A blurred node's appearance depends on content it
 * does not own, so the snapshot comparison in collect_node cannot see that it needs
 * repainting when something behind it moves. And the blur kernel samples beyond the
 * damage, where pixels still hold the *previous frame's composited* result -- which
 * already contains this node's own blur. Feeding that back in smears a little more every
 * frame. Covering the node and its reach means those pixels get cleared and repainted
 * from the backdrop up, which breaks the loop.
 *
 * One rect, not the region as a whole: mu_paint_damaged paints once per rect with the
 * scissor set to it, so a node straddling two rects would blur each half against the
 * other half's stale pixels. Asking for containment in one rect is also what makes this
 * terminate -- mu_region_add merges on overlap, so whatever absorbs `needed` contains it,
 * and the next pass finds containment and stops.
 *
 * Iterated because one expansion can reach another blurred node; it settles immediately
 * in practice, and the pass cap stops a pathological arrangement from spinning.
 */
static void expand_for_backdrops(MuContext *ctx, MuNode *n, bool *grew) {
    if (!n) return;
    /* prev_bounds, freshly set by collect_node, is where the node is drawn this frame. */
    if (n->backdrop_blur > 0.f && (n->flags & MU_NODE_VISIBLE) &&
        mu_region_overlaps(&ctx->damage, n->prev_bounds)) {
        MuRect needed = rect_inflate(n->prev_bounds, n->backdrop_blur + 1.f);
        if (!region_contains_rect(&ctx->damage, needed)) {
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
        MuRect reach = rect_inflate(t->node->prev_bounds, t->node->backdrop_blur + 1.f);
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
    ctx->animating = 0;
    gather_backdrops(ctx, ctx->root);
    gather_backdrops(ctx, ctx->popup_layer);
    gather_backdrops(ctx, ctx->modal_layer);

    const MuVec2 none = {0.f, 0.f};
    collect_node(ctx, ctx->root, 0u, none);
    collect_node(ctx, ctx->popup_layer, 0u, none);
    collect_node(ctx, ctx->modal_layer, 0u, none);

    mark_backdrops_dirty(ctx);

    if (!mu_region_empty(&ctx->damage)) {
        for (int pass = 0; pass < 4; pass++) {
            bool grew = false;
            expand_for_backdrops(ctx, ctx->root, &grew);
            expand_for_backdrops(ctx, ctx->popup_layer, &grew);
            expand_for_backdrops(ctx, ctx->modal_layer, &grew);
            if (!grew) break;
        }
    }
}

/*
 * paint_offset is applied by moving each node's bounds to where it is drawn for exactly
 * the duration of its own paint, and putting the saved value back afterwards. Every paint
 * op and every backend already draws from node->bounds, so this needs nothing from either
 * — the alternative, a translation threaded through every mu_draw_* in every backend, is
 * a lot of surface to keep in step. The saved copy is restored by assignment rather than
 * by subtracting the offset back out, because x + d - d is not always x in floating point
 * and a drifting layout rect would read as damage every frame.
 *
 * Shifting happens per node as the walk reaches it, so a paint op that reads its
 * *children's* bounds sees them where layout put them, not where they will be drawn. No
 * built-in widget does that; one that ever needs to should add node->paint_offset itself.
 */

/** Paint a subtree, skipping anything that cannot touch `area`. */
static void paint_tree_clipped(MuContext *ctx, MuRenderContext *rc, MuNode *node, MuRect area, MuVec2 off) {
    if (!node || !(node->flags & MU_NODE_VISIBLE)) return;

    off.x += node->paint_offset.x;
    off.y += node->paint_offset.y;
    MuRect layout_bounds = node->bounds;
    node->bounds = rect_shift(layout_bounds, off);

    /* A clipping container confines its children, so it can be pruned wholesale.
     * Non-clipping nodes are only pruned on their own bounds; children are tested
     * individually because they may extend beyond the parent. */
    if (!(node->flags & MU_NODE_CLIP_CHILDREN) || rect_overlaps(node->bounds, area)) {
        const MuNodeOps *ops = mu_get_node_ops(ctx, node->kind);
        bool self_visible = rect_overlaps(node->bounds, area);

        if (node->flags & MU_NODE_CLIP_CHILDREN) mu_push_scissor(rc, node->bounds);
        if (self_visible && ops && ops->paint) ops->paint(ctx, node, rc);

        for (int i = 0; i < node->child_count; i++)
            paint_tree_clipped(ctx, rc, node->children[i], area, off);

        if (self_visible && ops && ops->paint_overlay) ops->paint_overlay(ctx, node, rc);
        if (node->flags & MU_NODE_CLIP_CHILDREN) mu_pop_scissor(rc);
    }

    node->bounds = layout_bounds;
}

static void paint_tree_offset(MuContext *ctx, MuRenderContext *rc, MuNode *node, MuVec2 off) {
    if (!node || !(node->flags & MU_NODE_VISIBLE)) return;

    off.x += node->paint_offset.x;
    off.y += node->paint_offset.y;
    MuRect layout_bounds = node->bounds;
    node->bounds = rect_shift(layout_bounds, off);

    if (node->flags & MU_NODE_CLIP_CHILDREN) mu_push_scissor(rc, node->bounds);

    const MuNodeOps *ops = mu_get_node_ops(ctx, node->kind);
    if (ops && ops->paint) ops->paint(ctx, node, rc);

    for (int i = 0; i < node->child_count; i++) paint_tree_offset(ctx, rc, node->children[i], off);

    if (ops && ops->paint_overlay) ops->paint_overlay(ctx, node, rc);

    if (node->flags & MU_NODE_CLIP_CHILDREN) mu_pop_scissor(rc);

    node->bounds = layout_bounds;
}

void mu_paint_tree(MuContext *ctx, MuRenderContext *rc, MuNode *node) {
    paint_tree_offset(ctx, rc, node, (MuVec2){0.f, 0.f});
}

void mu_paint_all(MuContext *ctx, MuRenderContext *rc) {
    if (!ctx || !rc) return;
    if (ctx->root) mu_paint_tree(ctx, rc, ctx->root);
    if (ctx->popup_layer && (ctx->popup_layer->flags & MU_NODE_VISIBLE))
        mu_paint_tree(ctx, rc, ctx->popup_layer);
    if (ctx->modal_layer && (ctx->modal_layer->flags & MU_NODE_VISIBLE))
        mu_paint_tree(ctx, rc, ctx->modal_layer);
}

/*
 * Once per damage rect, with the scissor set to that rect.
 *
 * The rects are disjoint, so no pixel is visited twice -- which is the whole reason
 * mu_region_add works to keep them that way. An overlap here would repaint a pixel and
 * blend anything translucent over it a second time, and nothing downstream would notice.
 */
void mu_paint_damaged(MuContext *ctx, MuRenderContext *rc) {
    if (!ctx || !rc) return;

    const MuVec2 none = {0.f, 0.f};
    for (int i = 0; i < ctx->damage.count; i++) {
        MuRect area = ctx->damage.rects[i];
        mu_push_scissor(rc, area);
        if (ctx->root) paint_tree_clipped(ctx, rc, ctx->root, area, none);
        if (ctx->popup_layer && (ctx->popup_layer->flags & MU_NODE_VISIBLE))
            paint_tree_clipped(ctx, rc, ctx->popup_layer, area, none);
        if (ctx->modal_layer && (ctx->modal_layer->flags & MU_NODE_VISIBLE))
            paint_tree_clipped(ctx, rc, ctx->modal_layer, area, none);
        mu_pop_scissor(rc);
    }
}
