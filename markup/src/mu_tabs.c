#include "../include/markup/mu_tabs.h"
#include "../include/markup/mu_widgets_basic.h"
#include "../include/markup/mu_layout_flex.h"
#include "../include/markup/mu_style.h"
#include "../include/markup/mu_render.h"
#include "../include/markup/mu_input.h"
#include <stdlib.h>
#include <string.h>

#define MU_TABS_MAX 16
#define MU_TABS_LABEL_LEN 64
#define MU_TABS_BAR_HEIGHT 36.f

typedef struct MuTabsState {
    MuContext *ctx;
    MuNode *bar;
    MuNode *body;
    int n;
    int sel;
    char labels[MU_TABS_MAX][MU_TABS_LABEL_LEN];
    MuNode *bar_items[MU_TABS_MAX];
    MuNode *contents[MU_TABS_MAX];
    void (*on_change)(void *user, int index);
    void *user;
    struct {
        MuContext *ctx;
        MuNode *tabs;
        int index;
    } picks[MU_TABS_MAX];
} MuTabsState;

static uint32_t tabs_kind_id = 0;

static MuTabsState *tabs_state(MuNode *node) {
    return node ? (MuTabsState *)node->state : NULL;
}

static bool pt_in_rect(MuVec2 p, MuRect r) {
    return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

static MuRect tabs_inner(const MuNode *node) {
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

static void tabs_update_styles(MuNode *tabs) {
    MuTabsState *s = tabs_state(tabs);
    if (!s) return;
    for (int i = 0; i < s->n; i++) {
        if (s->bar_items[i]) s->bar_items[i]->role = (i == s->sel) ? "tab-selected" : "tab";
        if (s->contents[i]) {
            if (i == s->sel)
                s->contents[i]->flags |= MU_NODE_VISIBLE;
            else
                s->contents[i]->flags &= ~MU_NODE_VISIBLE;
        }
    }
}

static void tabs_pick(void *user) {
    struct {
        MuContext *ctx;
        MuNode *tabs;
        int index;
    } *u = user;
    if (!u || !u->ctx || !u->tabs) return;
    mu_tabs_set_selection(u->ctx, u->tabs, u->index);
}

static void tabs_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void tabs_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    (void)ctx;
    const MuLayoutStyle *ls = &node->layout;
    float w = avail.x > 0.f ? avail.x : 320.f;
    float h = ls->min_height > 0.f ? ls->min_height : 280.f;
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

static void tabs_layout_children(MuContext *ctx, MuNode *node) {
    MuTabsState *s = tabs_state(node);
    if (!s || !s->bar || !s->body) return;

    MuRect inner = tabs_inner(node);
    float bar_h = MU_TABS_BAR_HEIGHT;
    if (bar_h > inner.h) bar_h = inner.h;

    s->bar->bounds = (MuRect){inner.x, inner.y, inner.w, bar_h};
    s->body->bounds = (MuRect){inner.x, inner.y + bar_h, inner.w, inner.h - bar_h};
    if (s->body->bounds.h < 0.f) s->body->bounds.h = 0.f;

    mu_layout_node(ctx, s->bar);

    MuRect body_inner = s->body->bounds;
    const MuLayoutStyle *bl = &s->body->layout;
    body_inner.x += bl->padding_left;
    body_inner.y += bl->padding_top;
    body_inner.w -= bl->padding_left + bl->padding_right;
    body_inner.h -= bl->padding_top + bl->padding_bottom;
    if (body_inner.w < 0.f) body_inner.w = 0.f;
    if (body_inner.h < 0.f) body_inner.h = 0.f;

    for (int i = 0; i < s->n; i++) {
        MuNode *pane = s->contents[i];
        if (!pane || !(pane->flags & MU_NODE_VISIBLE)) continue;
        pane->bounds = body_inner;
        mu_layout_node(ctx, pane);
    }
}

static void tabs_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    float rad = (st.radius_tl + st.radius_tr + st.radius_br + st.radius_bl) * 0.25f;
    mu_draw_rect(rc, node->bounds, st.background, st.border, st.border_width, rad);

    MuTabsState *s = tabs_state(node);
    if (!s || !s->bar || s->sel < 0 || s->sel >= s->n) return;

    MuColor line = ctx->style ? ctx->style->panel_border : st.border;
    MuRect bar = s->bar->bounds;
    MuRect sep = {bar.x, bar.y + bar.h - 1.f, bar.w, 1.f};
    mu_draw_rect(rc, sep, line, (MuColor){0, 0, 0, 0}, 0.f, 0.f);
}

static void tabs_paint_overlay(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuTabsState *s = tabs_state(node);
    if (!s || s->sel < 0 || s->sel >= s->n || !s->bar_items[s->sel]) return;

    MuColor accent = ctx->style ? ctx->style->button_bg : (MuColor){59, 130, 246, 255};
    MuRect tab = s->bar_items[s->sel]->bounds;
    MuRect indicator = {tab.x + 8.f, tab.y + tab.h - 2.f, tab.w - 16.f, 2.f};
    if (indicator.w < 4.f) indicator.w = 4.f;
    mu_draw_rect(rc, indicator, accent, accent, 0.f, 1.f);
}

static bool tabs_hit(MuContext *ctx, MuNode *node, MuVec2 pt) {
    (void)ctx;
    return pt_in_rect(pt, node->bounds);
}

static bool tabs_ptr(MuContext *ctx, MuNode *node, const void *evp) {
    const MuPointerEvent *ev = (const MuPointerEvent *)evp;
    if (ev->pressed && pt_in_rect(ev->position, node->bounds)) mu_focus_set(ctx, node->id);
    return true;
}

static bool tabs_key(MuContext *ctx, MuNode *node, const void *evp) {
    const MuKeyEvent *ev = (const MuKeyEvent *)evp;
    MuTabsState *s = tabs_state(node);
    if (!s || !ev || !ev->pressed || s->n <= 1) return false;

    if (ev->key == MU_KEY_RIGHT) {
        mu_tabs_set_selection(ctx, node, (s->sel + 1) % s->n);
        return true;
    }
    if (ev->key == MU_KEY_LEFT) {
        int next = s->sel - 1;
        if (next < 0) next = s->n - 1;
        mu_tabs_set_selection(ctx, node, next);
        return true;
    }
    return false;
}

static const MuNodeOps tabs_node_ops = {
    .destroy_state = tabs_destroy,
    .measure = tabs_measure,
    .layout_children = tabs_layout_children,
    .paint = tabs_paint,
    .paint_overlay = tabs_paint_overlay,
    .hit_test = tabs_hit,
    .on_pointer = tabs_ptr,
    .on_key = tabs_key,
    .on_char = NULL,
};

void mu_tabs_register(MuContext *ctx) {
    (void)ctx;
    if (!tabs_kind_id) tabs_kind_id = mu_register_node_kind(ctx, "tabs", &tabs_node_ops);
}

uint32_t mu_kind_tabs(const MuContext *ctx) {
    (void)ctx;
    return tabs_kind_id;
}

MuNode *mu_make_tabs(MuContext *ctx) {
    if (!ctx || !tabs_kind_id) return NULL;

    MuTabsState *s = (MuTabsState *)calloc(1, sizeof(MuTabsState));
    if (!s) return NULL;
    s->sel = -1;
    s->ctx = ctx;

    MuNode *tabs = mu_node_create(ctx, tabs_kind_id, s);
    if (!tabs) {
        free(s);
        return NULL;
    }

    MuNode *bar = mu_make_panel(ctx, false);
    MuNode *body = mu_make_panel(ctx, true);
    if (!bar || !body) {
        mu_node_destroy_recursive(ctx, tabs);
        return NULL;
    }

    bar->role = "group";
    body->role = "group";
    mu_layout_set_flex_direction(bar, MU_FLEX_ROW);
    mu_layout_set_gap(bar, 0.f);
    mu_layout_set_padding_all(bar, 0.f);
    mu_layout_set_align_items(bar, MU_ALIGN_STRETCH);
    mu_layout_set_padding(body, 12.f, 12.f, 12.f, 12.f);
    mu_layout_set_gap(body, 8.f);

    s->bar = bar;
    s->body = body;
    mu_node_add_child(ctx, tabs, bar);
    mu_node_add_child(ctx, tabs, body);

    tabs->role = "tabs";
    tabs->flags |= MU_NODE_CLIP_CHILDREN;
    tabs->flags |= MU_NODE_FOCUSABLE;
    mu_layout_set_padding_all(tabs, 0.f);
    return tabs;
}

MuNode *mu_tabs_add(MuContext *ctx, MuNode *tabs, const char *label) {
    MuTabsState *s = tabs_state(tabs);
    if (!ctx || !s || s->n >= MU_TABS_MAX) return NULL;

    int i = s->n;
    const char *src = label ? label : "";
    strncpy(s->labels[i], src, MU_TABS_LABEL_LEN - 1);
    s->labels[i][MU_TABS_LABEL_LEN - 1] = '\0';

    s->picks[i].ctx = ctx;
    s->picks[i].tabs = tabs;
    s->picks[i].index = i;

    MuNode *tab = mu_make_panel(ctx, false);
    tab->role = "tab";
    mu_layout_set_padding(tab, 8.f, 16.f, 8.f, 16.f);
    mu_layout_set_min_size(tab, 0.f, MU_TABS_BAR_HEIGHT);
    mu_layout_set_align_self(tab, MU_ALIGN_STRETCH);
    mu_panel_set_on_click(tab, tabs_pick, &s->picks[i]);

    MuNode *lbl = mu_make_label(ctx, s->labels[i]);
    mu_node_set_hit_transparent(lbl, true);
    mu_node_add_child(ctx, tab, lbl);

    MuNode *pane = mu_make_panel(ctx, true);
    pane->role = "group";
    mu_layout_set_padding_all(pane, 0.f);
    mu_layout_set_gap(pane, 8.f);
    mu_layout_set_align_items(pane, MU_ALIGN_STRETCH);
    pane->flags &= ~MU_NODE_VISIBLE;

    s->bar_items[i] = tab;
    s->contents[i] = pane;
    mu_node_add_child(ctx, s->bar, tab);
    mu_node_add_child(ctx, s->body, pane);

    s->n++;
    s->ctx = ctx;
    if (s->sel < 0) mu_tabs_set_selection(ctx, tabs, 0);
    else tabs_update_styles(tabs);
    mu_layout_mark_dirty(tabs);
    return pane;
}

MuNode *mu_tabs_content(MuNode *tabs, int index) {
    MuTabsState *s = tabs_state(tabs);
    if (!s || index < 0 || index >= s->n) return NULL;
    return s->contents[index];
}

int mu_tabs_count(const MuNode *tabs) {
    const MuTabsState *s = tabs ? (const MuTabsState *)tabs->state : NULL;
    return s ? s->n : 0;
}

int mu_tabs_get_selection(const MuNode *tabs) {
    const MuTabsState *s = tabs ? (const MuTabsState *)tabs->state : NULL;
    return s ? s->sel : -1;
}

void mu_tabs_set_selection(MuContext *ctx, MuNode *tabs, int index) {
    MuTabsState *s = tabs_state(tabs);
    if (!ctx || !s || index < 0 || index >= s->n) return;
    if (s->sel == index) return;
    s->sel = index;
    tabs_update_styles(tabs);
    mu_layout_mark_dirty(tabs);
    if (s->on_change) s->on_change(s->user, s->sel);
}

const char *mu_tabs_label(const MuNode *tabs, int index) {
    const MuTabsState *s = tabs ? (const MuTabsState *)tabs->state : NULL;
    if (!s || index < 0 || index >= s->n) return "";
    return s->labels[index];
}

void mu_tabs_set_on_change(MuNode *tabs, void (*cb)(void *user, int index), void *user) {
    MuTabsState *s = tabs_state(tabs);
    if (!s) return;
    s->on_change = cb;
    s->user = user;
}
