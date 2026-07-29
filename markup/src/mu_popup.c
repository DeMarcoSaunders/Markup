#include "../include/markup/mu_popup.h"
#include "../include/markup/mu_layout_flex.h"
#include "../include/markup/mu_style.h"
#include "../include/markup/mu_render.h"
#include "../include/markup/mu_widgets_basic.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct MuPopupState {
    bool open;
    uint32_t anchor_id;
    MuPopupPlacement placement;
    MuNode *content;
} MuPopupState;

static uint32_t popup_kind_id = 0;

static bool pt_in_rect(MuVec2 p, MuRect r) {
    return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

static MuPopupState *popup_state(MuNode *node) {
    return node ? (MuPopupState *)node->state : NULL;
}

static MuNode *active_popup(MuContext *ctx) {
    if (!ctx || !ctx->active_popup_id || !ctx->popup_layer) return NULL;
    return mu_context_find_id(ctx, ctx->popup_layer, ctx->active_popup_id);
}

static MuRect popup_inner(const MuNode *node) {
    const MuLayoutStyle *fl = &node->layout;
    MuRect inner = node->bounds;
    inner.x += fl->padding_left;
    inner.y += fl->padding_top;
    inner.w -= fl->padding_left + fl->padding_right;
    inner.h -= fl->padding_top + fl->padding_bottom;
    if (inner.w < 0.f) inner.w = 0.f;
    if (inner.h < 0.f) inner.h = 0.f;
    return inner;
}

static void popup_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void popup_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    MuPopupState *s = popup_state(node);
    if (!out) return;
    out->x = 120.f;
    out->y = 40.f;
    if (!s || !s->content) return;
    MuVec2 measured = {0.f, 0.f};
    float inner_w = avail.x > 0.f ? avail.x : 240.f;
    mu_layout_measure_container(ctx, s->content, (MuVec2){inner_w, 0.f}, &measured);
    const MuLayoutStyle *fl = &node->layout;
    out->x = measured.x + fl->padding_left + fl->padding_right;
    out->y = measured.y + fl->padding_top + fl->padding_bottom;
}

static void popup_layout_children(MuContext *ctx, MuNode *node) {
    MuPopupState *s = popup_state(node);
    if (!s || !s->content || !s->open) return;
    MuRect inner = popup_inner(node);
    s->content->bounds = inner;
    mu_layout_flex_run(ctx, s->content);
}

static void popup_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuPopupState *s = popup_state(node);
    if (!s || !s->open) return;
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    float rad = (st.radius_tl + st.radius_tr + st.radius_br + st.radius_bl) * 0.25f;
    MuColor bg = ctx->style ? ctx->style->panel_bg : st.background;
    MuColor border = ctx->style ? ctx->style->panel_border : st.border;
    mu_draw_rect(rc, node->bounds, bg, border, 1.f, rad);
}

static bool popup_hit(MuContext *ctx, MuNode *node, MuVec2 pt) {
    (void)ctx;
    MuPopupState *s = popup_state(node);
    if (!s || !s->open) return false;
    return pt_in_rect(pt, node->bounds);
}

static const MuNodeOps popup_node_ops = {
    .destroy_state = popup_destroy,
    .measure = popup_measure,
    .layout_children = popup_layout_children,
    .paint = popup_paint,
    .hit_test = popup_hit,
    .on_pointer = NULL,
    .on_key = NULL,
    .on_char = NULL,
};

static MuNode *popup_create_content(MuContext *ctx) {
    MuNode *content = mu_make_panel(ctx, true);
    if (!content) return NULL;
    content->role = "group";
    mu_layout_set_padding_all(content, 4.f);
    mu_layout_set_gap(content, 0.f);
    content->flags |= MU_NODE_CLIP_CHILDREN;
    return content;
}

void mu_popup_register(MuContext *ctx) {
    (void)ctx;
    if (!popup_kind_id) popup_kind_id = mu_register_node_kind(ctx, "popup", &popup_node_ops);
}

uint32_t mu_kind_popup(const MuContext *ctx) {
    (void)ctx;
    return popup_kind_id;
}

MuNode *mu_make_popup_layer(MuContext *ctx) {
    if (!ctx) return NULL;
    MuNode *layer = mu_make_panel(ctx, true);
    if (!layer) return NULL;
    layer->role = "popup-layer";
    layer->flags &= ~MU_NODE_VISIBLE;
    mu_layout_set_padding_all(layer, 0.f);
    mu_layout_set_gap(layer, 0.f);
    return layer;
}

void mu_popup_bind_layer(MuContext *ctx, MuNode *layer) {
    if (ctx) ctx->popup_layer = layer;
}

MuNode *mu_make_popup(MuContext *ctx) {
    if (!ctx || !popup_kind_id) return NULL;

    MuPopupState *s = (MuPopupState *)calloc(1, sizeof(MuPopupState));
    if (!s) return NULL;
    s->placement = MU_POPUP_BELOW;

    MuNode *popup = mu_node_create(ctx, popup_kind_id, s);
    if (!popup) {
        free(s);
        return NULL;
    }

    MuNode *content = popup_create_content(ctx);
    if (!content) {
        mu_node_destroy_recursive(ctx, popup);
        return NULL;
    }
    s->content = content;
    mu_node_add_child(ctx, popup, content);

    popup->role = "popup";
    popup->flags &= ~MU_NODE_VISIBLE;
    popup->flags |= MU_NODE_CLIP_CHILDREN;
    mu_layout_set_padding_all(popup, 0.f);
    mu_layout_set_min_size(popup, 80.f, 0.f);

    if (ctx->popup_layer) mu_node_add_child(ctx, ctx->popup_layer, popup);
    return popup;
}

MuNode *mu_popup_content(MuNode *popup) {
    MuPopupState *s = popup_state(popup);
    return s ? s->content : NULL;
}

void mu_popup_set_anchor(MuNode *popup, MuNode *anchor) {
    MuPopupState *s = popup_state(popup);
    if (!s) return;
    s->anchor_id = anchor ? anchor->id : 0;
}

void mu_popup_set_placement(MuNode *popup, MuPopupPlacement placement) {
    MuPopupState *s = popup_state(popup);
    if (!s) return;
    s->placement = placement;
}

bool mu_popup_is_open(const MuNode *popup) {
    const MuPopupState *s = popup ? (const MuPopupState *)popup->state : NULL;
    return s && s->open;
}

static void popup_set_open(MuContext *ctx, MuNode *popup, bool open) {
    MuPopupState *s = popup_state(popup);
    if (!s) return;
    s->open = open;
    if (open) {
        popup->flags |= MU_NODE_VISIBLE;
        ctx->active_popup_id = popup->id;
        if (ctx->popup_layer) ctx->popup_layer->flags |= MU_NODE_VISIBLE;
    } else {
        popup->flags &= ~MU_NODE_VISIBLE;
        if (ctx->active_popup_id == popup->id) ctx->active_popup_id = 0;
        if (ctx->popup_layer && ctx->popup_layer->child_count > 0) {
            bool any = false;
            for (int i = 0; i < ctx->popup_layer->child_count; i++) {
                MuNode *ch = ctx->popup_layer->children[i];
                MuPopupState *cs = popup_state(ch);
                if (cs && cs->open) {
                    any = true;
                    break;
                }
            }
            if (!any) ctx->popup_layer->flags &= ~MU_NODE_VISIBLE;
        } else if (ctx->popup_layer) {
            ctx->popup_layer->flags &= ~MU_NODE_VISIBLE;
        }
    }
    mu_layout_mark_dirty(popup);
}

void mu_popup_open(MuContext *ctx, MuNode *popup) {
    if (!ctx || !popup) return;
    if (ctx->active_popup_id && ctx->active_popup_id != popup->id) {
        MuNode *prev = active_popup(ctx);
        if (prev) popup_set_open(ctx, prev, false);
    }
    popup_set_open(ctx, popup, true);
}

void mu_popup_close(MuContext *ctx, MuNode *popup) {
    if (!ctx || !popup) return;
    popup_set_open(ctx, popup, false);
}

void mu_popup_toggle(MuContext *ctx, MuNode *popup) {
    if (!ctx || !popup) return;
    if (mu_popup_is_open(popup))
        mu_popup_close(ctx, popup);
    else
        mu_popup_open(ctx, popup);
}

void mu_popups_sync(MuContext *ctx) {
    if (!ctx || !ctx->active_popup_id || !ctx->popup_layer || !ctx->root) return;

    MuNode *popup = active_popup(ctx);
    if (!popup) return;

    MuPopupState *s = popup_state(popup);
    if (!s || !s->open || !s->content) return;

    MuRect root = ctx->root->bounds;
    mu_node_set_bounds(ctx->popup_layer, root);

    MuNode *anchor = s->anchor_id ? mu_context_find_id(ctx, NULL, s->anchor_id) : NULL;
    if (!anchor) return;

    MuVec2 measured = {0.f, 0.f};
    float max_w = root.w > 16.f ? root.w - 16.f : root.w;
    float anchor_w = anchor->bounds.w > 0.f ? anchor->bounds.w : 120.f;
  float inner_w = anchor_w > max_w ? max_w : anchor_w;
    mu_layout_measure_container(ctx, s->content, (MuVec2){inner_w, 0.f}, &measured);

    const MuLayoutStyle *fl = &popup->layout;
    float pw = measured.x + fl->padding_left + fl->padding_right;
    float ph = measured.y + fl->padding_top + fl->padding_bottom;
    if (pw < anchor_w) pw = anchor_w;
    if (pw < 80.f) pw = 80.f;

    float gap = 4.f;
    float x = anchor->bounds.x;
    float y = anchor->bounds.y + anchor->bounds.h + gap;

    MuPopupPlacement place = s->placement;
    if (place == MU_POPUP_AUTO) {
        place = (y + ph > root.y + root.h) ? MU_POPUP_ABOVE : MU_POPUP_BELOW;
    }
    if (place == MU_POPUP_ABOVE) y = anchor->bounds.y - ph - gap;

    if (x + pw > root.x + root.w - 8.f) x = root.x + root.w - 8.f - pw;
    if (x < root.x + 8.f) x = root.x + 8.f;
    if (y + ph > root.y + root.h - 8.f) y = root.y + root.h - 8.f - ph;
    if (y < root.y + 8.f) y = root.y + 8.f;

    mu_node_set_bounds(popup, (MuRect){x, y, pw, ph});
    popup_layout_children(ctx, popup);
    popup->flags &= ~MU_NODE_LAYOUT_DIRTY;
}

void mu_popups_dispatch_pointer(MuContext *ctx, const MuPointerEvent *ev) {
    if (!ctx || !ev || !ctx->active_popup_id) return;

    MuNode *popup = active_popup(ctx);
    if (!popup || !mu_popup_is_open(popup)) return;
    if (pt_in_rect(ev->position, popup->bounds)) return;

    MuPopupState *s = popup_state(popup);
    if (s && s->anchor_id) {
        MuNode *anchor = mu_context_find_id(ctx, NULL, s->anchor_id);
        if (anchor && pt_in_rect(ev->position, anchor->bounds)) return;
    }

    if (ev->pressed || ev->released) mu_popup_close(ctx, popup);
}
