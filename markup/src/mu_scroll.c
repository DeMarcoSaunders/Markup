#include "../include/markup/mu_widgets_basic.h"
#include "../include/markup/mu_layout_flex.h"
#include "../include/markup/mu_style.h"
#include "../include/markup/mu_render.h"
#include "../include/markup/mu_input.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * Scrollable viewport with optional horizontal/vertical bars.
 *
 * A scroll node owns one content child. The node itself is the viewport and carries
 * MU_NODE_CLIP_CHILDREN; scroll_layout_children lays the content out at its full natural
 * size and then offsets its bounds by (-offset_x, -offset_y). Nothing is virtualised —
 * every child is laid out and painted, and clipping does the hiding.
 *
 * Bar visibility is circular by nature: showing a vertical bar narrows the viewport,
 * which can force a horizontal bar, which shortens it again. scroll_needs_v/h resolve
 * that in a fixed order rather than iterating to a fixed point, so a content size within
 * one bar-width of the viewport can settle either way. scroll_bar_geom is the single
 * source of truth for the resulting rects and is reused by hit-testing and painting.
 *
 * Bars are drawn in paint_overlay, not paint, so they sit above the clipped content.
 */

#define MU_SCROLL_BAR_SIZE 10.f
#define MU_SCROLL_THUMB_MIN 24.f
#define MU_SCROLL_THUMB_RADIUS 4.f

typedef enum MuScrollDrag {
    MU_SCROLL_DRAG_NONE = 0,
    MU_SCROLL_DRAG_V,
    MU_SCROLL_DRAG_H,
} MuScrollDrag;

typedef struct MuScrollState {
    float scroll_x;
    float scroll_y;
    float content_w;
    float content_h;
    MuScrollAxis axis;
    MuNode *content;
    MuScrollDrag drag;
    float drag_anchor_x;
    float drag_anchor_y;
    float drag_start_scroll_x;
    float drag_start_scroll_y;
    /* Requested-minus-clamped offset, accumulated since the last mu_scroll_get_excess.
     * Positive means an offset change asked to go past the max end, negative past zero. */
    float excess_x;
    float excess_y;
} MuScrollState;

typedef struct MuScrollBarGeom {
    bool show_v;
    bool show_h;
    MuRect view;
    MuRect track_v;
    MuRect thumb_v;
    MuRect track_h;
    MuRect thumb_h;
} MuScrollBarGeom;

static bool is_scroll_node(const MuContext *ctx, const MuNode *node) {
    if (!ctx || !node) return false;
    uint32_t k = mu_kind_scroll(ctx);
    return k != 0 && node->kind == k;
}

static MuScrollState *scroll_state(MuNode *node) {
    return node ? (MuScrollState *)node->state : NULL;
}

static MuRect scroll_inner(const MuNode *node) {
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

static bool scroll_needs_v(const MuScrollState *s, float view_h) {
    return s && (s->axis & MU_SCROLL_VERTICAL) && s->content_h > view_h + 0.5f;
}

static bool scroll_needs_h(const MuScrollState *s, float view_w) {
    return s && (s->axis & MU_SCROLL_HORIZONTAL) && s->content_w > view_w + 0.5f;
}

static MuScrollBarGeom scroll_bar_geom(const MuNode *node, const MuScrollState *s) {
    MuScrollBarGeom g;
    memset(&g, 0, sizeof(g));
    if (!node || !s) return g;

    MuRect inner = scroll_inner(node);
    g.view = inner;

    g.show_v = scroll_needs_v(s, inner.h);
    g.show_h = scroll_needs_h(s, inner.w);

    float sb = MU_SCROLL_BAR_SIZE;
    if (g.show_v) g.view.w -= sb;
    if (g.show_h) g.view.h -= sb;
    if (g.view.w < 0.f) g.view.w = 0.f;
    if (g.view.h < 0.f) g.view.h = 0.f;

    if (g.show_v) {
        g.track_v = (MuRect){inner.x + inner.w - sb, inner.y, sb, g.show_h ? inner.h - sb : inner.h};
        float thumb_h = g.track_v.h * (g.view.h / s->content_h);
        if (thumb_h < MU_SCROLL_THUMB_MIN) thumb_h = MU_SCROLL_THUMB_MIN;
        if (thumb_h > g.track_v.h) thumb_h = g.track_v.h;
        float max_scroll = s->content_h - g.view.h;
        float t = max_scroll > 0.f ? s->scroll_y / max_scroll : 0.f;
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
        float thumb_y = g.track_v.y + t * (g.track_v.h - thumb_h);
        g.thumb_v = (MuRect){g.track_v.x, thumb_y, g.track_v.w, thumb_h};
    }

    if (g.show_h) {
        g.track_h = (MuRect){inner.x, inner.y + inner.h - sb, g.show_v ? inner.w - sb : inner.w, sb};
        float thumb_w = g.track_h.w * (g.view.w / s->content_w);
        if (thumb_w < MU_SCROLL_THUMB_MIN) thumb_w = MU_SCROLL_THUMB_MIN;
        if (thumb_w > g.track_h.w) thumb_w = g.track_h.w;
        float max_scroll = s->content_w - g.view.w;
        float t = max_scroll > 0.f ? s->scroll_x / max_scroll : 0.f;
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
        float thumb_x = g.track_h.x + t * (g.track_h.w - thumb_w);
        g.thumb_h = (MuRect){thumb_x, g.track_h.y, thumb_w, g.track_h.h};
    }

    return g;
}

static void scroll_clamp(MuScrollState *s, float viewport_w, float viewport_h) {
    if (!s) return;
    float max_x = s->content_w - viewport_w;
    float max_y = s->content_h - viewport_h;
    if (max_x < 0.f) max_x = 0.f;
    if (max_y < 0.f) max_y = 0.f;
    if (s->scroll_x < 0.f) s->scroll_x = 0.f;
    if (s->scroll_y < 0.f) s->scroll_y = 0.f;
    if (s->scroll_x > max_x) s->scroll_x = max_x;
    if (s->scroll_y > max_y) s->scroll_y = max_y;
    if (!(s->axis & MU_SCROLL_HORIZONTAL)) s->scroll_x = 0.f;
    if (!(s->axis & MU_SCROLL_VERTICAL)) s->scroll_y = 0.f;
}

static MuScrollDrag scroll_hit_bar(const MuScrollBarGeom *g, MuVec2 pt) {
    if (!g) return MU_SCROLL_DRAG_NONE;
    if (g->show_v && g->thumb_v.h > 0.f && pt.x >= g->thumb_v.x && pt.x < g->thumb_v.x + g->thumb_v.w &&
        pt.y >= g->thumb_v.y && pt.y < g->thumb_v.y + g->thumb_v.h)
        return MU_SCROLL_DRAG_V;
    if (g->show_h && g->thumb_h.w > 0.f && pt.x >= g->thumb_h.x && pt.x < g->thumb_h.x + g->thumb_h.w &&
        pt.y >= g->thumb_h.y && pt.y < g->thumb_h.y + g->thumb_h.h)
        return MU_SCROLL_DRAG_H;
    if (g->show_v && pt.x >= g->track_v.x && pt.x < g->track_v.x + g->track_v.w && pt.y >= g->track_v.y &&
        pt.y < g->track_v.y + g->track_v.h)
        return MU_SCROLL_DRAG_V;
    if (g->show_h && pt.x >= g->track_h.x && pt.x < g->track_h.x + g->track_h.w && pt.y >= g->track_h.y &&
        pt.y < g->track_h.y + g->track_h.h)
        return MU_SCROLL_DRAG_H;
    return MU_SCROLL_DRAG_NONE;
}

static void scroll_jump_track(MuContext *ctx, MuNode *node, MuScrollState *s, const MuScrollBarGeom *g, MuScrollDrag bar,
                              MuVec2 pt) {
    if (!ctx || !node || !s || !g) return;

    if (bar == MU_SCROLL_DRAG_V && g->thumb_v.h > 0.f) {
        float max_scroll = s->content_h - g->view.h;
        if (max_scroll <= 0.f) return;
        float track_len = g->track_v.h;
        float thumb_len = g->thumb_v.h;
        float rel = pt.y - g->track_v.y - thumb_len * 0.5f;
        float denom = track_len - thumb_len;
        if (denom < 1.f) denom = 1.f;
        float t = rel / denom;
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
        mu_scroll_set_offset(ctx, node, s->scroll_x, t * max_scroll);
        return;
    }

    if (bar == MU_SCROLL_DRAG_H && g->thumb_h.w > 0.f) {
        float max_scroll = s->content_w - g->view.w;
        if (max_scroll <= 0.f) return;
        float track_len = g->track_h.w;
        float thumb_len = g->thumb_h.w;
        float rel = pt.x - g->track_h.x - thumb_len * 0.5f;
        float denom = track_len - thumb_len;
        if (denom < 1.f) denom = 1.f;
        float t = rel / denom;
        if (t < 0.f) t = 0.f;
        if (t > 1.f) t = 1.f;
        mu_scroll_set_offset(ctx, node, t * max_scroll, s->scroll_y);
    }
}

static bool scroll_on_pointer(MuContext *ctx, MuNode *node, const void *evp) {
    const MuPointerEvent *ev = (const MuPointerEvent *)evp;
    MuScrollState *s = scroll_state(node);
    if (!s || !ev) return false;

    MuScrollBarGeom g = scroll_bar_geom(node, s);

    if (ev->drag && s->drag != MU_SCROLL_DRAG_NONE) {
        if (s->drag == MU_SCROLL_DRAG_V && g.thumb_v.h > 0.f) {
            float max_scroll = s->content_h - g.view.h;
            float track_len = g.track_v.h;
            float thumb_len = g.thumb_v.h;
            float denom = track_len - thumb_len;
            if (denom < 1.f) denom = 1.f;
            float dy = ev->position.y - s->drag_anchor_y;
            float ny = s->drag_start_scroll_y + dy * (max_scroll / denom);
            mu_scroll_set_offset(ctx, node, s->scroll_x, ny);
        } else if (s->drag == MU_SCROLL_DRAG_H && g.thumb_h.w > 0.f) {
            float max_scroll = s->content_w - g.view.w;
            float track_len = g.track_h.w;
            float thumb_len = g.thumb_h.w;
            float denom = track_len - thumb_len;
            if (denom < 1.f) denom = 1.f;
            float dx = ev->position.x - s->drag_anchor_x;
            float nx = s->drag_start_scroll_x + dx * (max_scroll / denom);
            mu_scroll_set_offset(ctx, node, nx, s->scroll_y);
        }
        return true;
    }

    if (ev->pressed) {
        MuScrollDrag bar = scroll_hit_bar(&g, ev->position);
        if (bar == MU_SCROLL_DRAG_NONE) return false;

        bool on_thumb = (bar == MU_SCROLL_DRAG_V && ev->position.y >= g.thumb_v.y && ev->position.y < g.thumb_v.y + g.thumb_v.h) ||
                        (bar == MU_SCROLL_DRAG_H && ev->position.x >= g.thumb_h.x && ev->position.x < g.thumb_h.x + g.thumb_h.w);

        if (!on_thumb) scroll_jump_track(ctx, node, s, &g, bar, ev->position);

        s->drag = bar;
        s->drag_anchor_x = ev->position.x;
        s->drag_anchor_y = ev->position.y;
        s->drag_start_scroll_x = s->scroll_x;
        s->drag_start_scroll_y = s->scroll_y;
        ctx->captured_pointer_id = node->id;
        return true;
    }

    if (ev->released) {
        s->drag = MU_SCROLL_DRAG_NONE;
        if (ctx->captured_pointer_id == node->id) ctx->captured_pointer_id = 0;
        return true;
    }

    return false;
}

static void scroll_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void scroll_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    (void)ctx;
    const MuLayoutStyle *ls = &node->layout;
    float w = avail.x > 0.f ? avail.x : 240.f;
    float h = ls->min_height > 0.f ? ls->min_height : 160.f;
    if (ls->flex_basis >= 0.f) h = ls->flex_basis;
    if (ls->max_height > 0.f) {
        h = ls->max_height;
    } else if (avail.y > 0.f && ls->flex_grow > 0.f) {
        h = avail.y;
    }
    out->x = w;
    out->y = h;
    if (ls->min_width > 0.f && out->x < ls->min_width) out->x = ls->min_width;
    if (ls->max_width > 0.f && out->x > ls->max_width) out->x = ls->max_width;
    if (ls->max_height > 0.f && out->y > ls->max_height) out->y = ls->max_height;
}

static void scroll_layout_children(MuContext *ctx, MuNode *node) {
    MuScrollState *s = scroll_state(node);
    if (!s || !s->content) return;

    MuRect inner = scroll_inner(node);
    MuVec2 measured = {0.f, 0.f};
    MuVec2 avail = {0.f, 0.f};
    if (s->axis & MU_SCROLL_VERTICAL) avail.x = inner.w;
    if (s->axis & MU_SCROLL_HORIZONTAL) avail.y = inner.h;
    mu_layout_measure_container(ctx, s->content, avail, &measured);

    s->content_w = measured.x;
    s->content_h = measured.y;

    MuScrollBarGeom g = scroll_bar_geom(node, s);
    if (g.show_v && avail.x > g.view.w + 0.5f) {
        avail.x = g.view.w;
        mu_layout_measure_container(ctx, s->content, avail, &measured);
        s->content_w = measured.x;
        s->content_h = measured.y;
        g = scroll_bar_geom(node, s);
    }
    if (g.show_h && avail.y > g.view.h + 0.5f) {
        avail.y = g.view.h;
        mu_layout_measure_container(ctx, s->content, avail, &measured);
        s->content_w = measured.x;
        s->content_h = measured.y;
        g = scroll_bar_geom(node, s);
    }

    if (s->content_w < g.view.w) s->content_w = g.view.w;
    scroll_clamp(s, g.view.w, g.view.h);

    s->content->bounds.x = g.view.x - s->scroll_x;
    s->content->bounds.y = g.view.y - s->scroll_y;
    s->content->bounds.w = s->content_w;
    s->content->bounds.h = s->content_h;

    mu_layout_flex_run(ctx, s->content);
}

static void scroll_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    float rad = (st.radius_tl + st.radius_tr + st.radius_br + st.radius_bl) * 0.25f;

    MuScrollState *s = scroll_state(node);
    MuScrollBarGeom g = scroll_bar_geom(node, s);

    if (st.background.a > 0)
        mu_draw_rect(rc, g.view, st.background, (MuColor){0, 0, 0, 0}, 0.f, rad);
    if (st.border.a > 0 && st.border_width > 0.f)
        mu_draw_rect(rc, g.view, (MuColor){0, 0, 0, 0}, st.border, st.border_width, rad);

    if (g.view.w > 0.f && g.view.h > 0.f) mu_push_scissor(rc, g.view);
}

static void scroll_paint_overlay(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuScrollState *s = scroll_state(node);
    MuScrollBarGeom g = scroll_bar_geom(node, s);

    /* Content clip from scroll_paint must be off before drawing gutter bars. */
    if (g.view.w > 0.f && g.view.h > 0.f) mu_pop_scissor(rc);

    if (!g.show_v && !g.show_h) return;

    const MuStyleModule *theme = ctx ? ctx->style : NULL;
    MuColor track = theme ? theme->slider_track : (MuColor){80, 88, 104, 200};
    MuColor thumb = theme ? theme->slider_thumb : (MuColor){120, 140, 180, 255};
    if (s && s->drag != MU_SCROLL_DRAG_NONE && theme) thumb = theme->button_bg_hover;

    if (g.show_v) {
        mu_draw_rect(rc, g.track_v, track, (MuColor){0, 0, 0, 0}, 0.f, 2.f);
        if (g.thumb_v.h > 0.f) mu_draw_rect(rc, g.thumb_v, thumb, thumb, 0.f, MU_SCROLL_THUMB_RADIUS);
    }
    if (g.show_h) {
        mu_draw_rect(rc, g.track_h, track, (MuColor){0, 0, 0, 0}, 0.f, 2.f);
        if (g.thumb_h.w > 0.f) mu_draw_rect(rc, g.thumb_h, thumb, thumb, 0.f, MU_SCROLL_THUMB_RADIUS);
    }
}

static bool scroll_hit(MuContext *ctx, MuNode *node, MuVec2 pt) {
    (void)ctx;
    MuScrollState *s = scroll_state(node);
    MuScrollBarGeom g = scroll_bar_geom(node, s);

    if (scroll_hit_bar(&g, pt) != MU_SCROLL_DRAG_NONE) return true;

    return pt.x >= g.view.x && pt.x < g.view.x + g.view.w && pt.y >= g.view.y && pt.y < g.view.y + g.view.h;
}

const MuNodeOps mu_scroll_node_ops = {
    .destroy_state = scroll_destroy,
    .measure = scroll_measure,
    .layout_children = scroll_layout_children,
    .paint = scroll_paint,
    .paint_overlay = scroll_paint_overlay,
    .hit_test = scroll_hit,
    .on_pointer = scroll_on_pointer,
    .on_key = NULL,
    .on_char = NULL,
};

static MuNode *scroll_create_content(MuContext *ctx) {
    MuNode *content = mu_make_panel(ctx, true);
    if (!content) return NULL;
    content->role = "group";
    mu_layout_set_padding_all(content, 0);
    mu_layout_set_gap(content, 8.f);
    content->flags &= ~MU_NODE_CLIP_CHILDREN;
    return content;
}

MuNode *mu_make_scroll(MuContext *ctx, MuScrollAxis axis) {
    if (!ctx || !ctx->widgets_basic_kinds) return NULL;
    uint32_t scroll_kind = mu_kind_scroll(ctx);
    if (!scroll_kind) return NULL;

    MuScrollState *s = (MuScrollState *)calloc(1, sizeof(MuScrollState));
    if (!s) return NULL;
    s->axis = axis == 0 ? MU_SCROLL_VERTICAL : axis;

    MuNode *scroll = mu_node_create(ctx, scroll_kind, s);
    if (!scroll) {
        free(s);
        return NULL;
    }

    MuNode *content = scroll_create_content(ctx);
    if (!content) {
        mu_node_destroy_recursive(ctx, scroll);
        return NULL;
    }
    s->content = content;
    mu_node_add_child(ctx, scroll, content);

    scroll->role = "scroll";
    scroll->flags |= MU_NODE_CLIP_CHILDREN;
    mu_layout_set_padding_all(scroll, 0.f);
    mu_layout_set_flex(scroll, 1.f, 1.f, MU_FLEX_BASIS_AUTO);
    mu_layout_set_min_size(scroll, 0.f, 120.f);
    return scroll;
}

MuNode *mu_scroll_content(MuNode *scroll) {
    MuScrollState *s = scroll_state(scroll);
    return s ? s->content : NULL;
}

void mu_scroll_set_offset(MuContext *ctx, MuNode *scroll, float x, float y) {
    MuScrollState *s = scroll_state(scroll);
    if (!s) return;
    if (s->axis & MU_SCROLL_HORIZONTAL) s->scroll_x = x;
    if (s->axis & MU_SCROLL_VERTICAL) s->scroll_y = y;
    MuScrollBarGeom g = scroll_bar_geom(scroll, s);
    float pre_x = s->scroll_x, pre_y = s->scroll_y;
    scroll_clamp(s, g.view.w, g.view.h);
    s->excess_x += pre_x - s->scroll_x;
    s->excess_y += pre_y - s->scroll_y;
    /* The content child moving is caught by the damage snapshot, but the scrollbar thumb
     * is painted by this node's overlay at unchanged bounds, so mark it explicitly. */
    mu_node_mark_paint_dirty(scroll);
    if (ctx) {
        scroll_layout_children(ctx, scroll);
        scroll->flags &= ~MU_NODE_LAYOUT_DIRTY;
    } else {
        mu_layout_mark_dirty(scroll);
    }
}

void mu_scroll_get_offset(const MuNode *scroll, float *out_x, float *out_y) {
    const MuScrollState *s = scroll ? (const MuScrollState *)scroll->state : NULL;
    if (out_x) *out_x = s ? s->scroll_x : 0.f;
    if (out_y) *out_y = s ? s->scroll_y : 0.f;
}

void mu_scroll_get_excess(MuNode *scroll, float *out_x, float *out_y) {
    MuScrollState *s = scroll_state(scroll);
    if (out_x) *out_x = s ? s->excess_x : 0.f;
    if (out_y) *out_y = s ? s->excess_y : 0.f;
    if (s) {
        s->excess_x = 0.f;
        s->excess_y = 0.f;
    }
}

void mu_scroll_by(MuContext *ctx, MuNode *scroll, float dx, float dy) {
    MuScrollState *s = scroll_state(scroll);
    if (!s) return;
    float nx = s->scroll_x;
    float ny = s->scroll_y;
    if (s->axis & MU_SCROLL_HORIZONTAL) nx -= dx;
    if (s->axis & MU_SCROLL_VERTICAL) ny -= dy;
    mu_scroll_set_offset(ctx, scroll, nx, ny);
}

void mu_widgets_dispatch_wheel(MuContext *ctx, MuVec2 position, float delta_x, float delta_y) {
    if (!ctx || (delta_x == 0.f && delta_y == 0.f)) return;

    MuNode *n = mu_input_node_at(ctx, position);
    if (!n) return;

    const float line = 32.f;
    float dx = delta_x * line;
    float dy = delta_y * line;

    for (MuNode *p = n; p; p = p->parent) {
        if (!is_scroll_node(ctx, p)) continue;
        MuRect inner = scroll_inner(p);
        if (position.x < inner.x || position.x >= inner.x + inner.w || position.y < inner.y ||
            position.y >= inner.y + inner.h)
            continue;
        mu_scroll_by(ctx, p, dx, dy);
        return;
    }
}
