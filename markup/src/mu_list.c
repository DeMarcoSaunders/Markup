#include "../include/markup/mu_list.h"
#include "../include/markup/mu_widgets_basic.h"
#include "../include/markup/mu_layout_flex.h"
#include "../include/markup/mu_style.h"
#include "../include/markup/mu_render.h"
#include "../include/markup/mu_input.h"
#include <stdlib.h>
#include <string.h>

#define MU_LIST_MAX_ITEMS 64
#define MU_LIST_ITEM_LEN 128

typedef struct MuListState {
    MuContext *ctx;
    MuNode *scroll;
    int n;
    int sel;
    char items[MU_LIST_MAX_ITEMS][MU_LIST_ITEM_LEN];
    void (*on_change)(void *user, int index);
    void *user;
    struct {
        MuContext *ctx;
        MuNode *list;
        int index;
    } picks[MU_LIST_MAX_ITEMS];
} MuListState;

static uint32_t list_kind_id = 0;

static MuListState *list_state(MuNode *node) {
    return node ? (MuListState *)node->state : NULL;
}

static bool pt_in_rect(MuVec2 p, MuRect r) {
    return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

static void list_apply_height(MuNode *list, float min_height) {
    float h = min_height > 0.f ? min_height : 160.f;
    mu_layout_set_min_size(list, 0.f, h);
    mu_layout_set_max_size(list, 0.f, h);
    mu_layout_set_flex(list, 0.f, 0.f, h);
}

static void list_update_styles(MuNode *list);

static void list_ensure_visible(MuContext *ctx, MuNode *list, int index) {
    MuListState *s = list_state(list);
    if (!ctx || !s || !s->scroll || index < 0) return;
    MuNode *content = mu_scroll_content(s->scroll);
    if (!content || index >= content->child_count) return;

    MuNode *row = content->children[index];
    MuRect inner = s->scroll->bounds;
    float scroll_x = 0.f, scroll_y = 0.f;
    mu_scroll_get_offset(s->scroll, &scroll_x, &scroll_y);

    float row_top = row->bounds.y;
    float row_bot = row->bounds.y + row->bounds.h;
    float view_top = inner.y;
    float view_bot = inner.y + inner.h;

    if (row_top < view_top)
        mu_scroll_set_offset(ctx, s->scroll, scroll_x, scroll_y - (view_top - row_top));
    else if (row_bot > view_bot)
        mu_scroll_set_offset(ctx, s->scroll, scroll_x, scroll_y + (row_bot - view_bot));
}

static void list_pick(void *user) {
    struct {
        MuContext *ctx;
        MuNode *list;
        int index;
    } *u = user;
    if (!u || !u->ctx || !u->list) return;
    MuListState *s = list_state(u->list);
    if (!s || u->index < 0 || u->index >= s->n) return;
    if (s->sel == u->index) return;
    s->sel = u->index;
    list_update_styles(u->list);
    mu_layout_mark_dirty(u->list);
    list_ensure_visible(u->ctx, u->list, s->sel);
    if (s->on_change) s->on_change(s->user, s->sel);
}

static void list_clear_rows(MuContext *ctx, MuListState *s) {
    if (!ctx || !s || !s->scroll) return;
    MuNode *content = mu_scroll_content(s->scroll);
    if (!content) return;
    while (content->child_count > 0) {
        MuNode *ch = content->children[0];
        mu_node_remove_child(ctx, content, ch);
        mu_node_destroy_recursive(ctx, ch);
    }
}

static void list_update_styles(MuNode *list) {
    MuListState *s = list_state(list);
    if (!s || !s->scroll) return;
    MuNode *content = mu_scroll_content(s->scroll);
    if (!content) return;
    for (int i = 0; i < content->child_count; i++) {
        MuNode *row = content->children[i];
        row->role = (i == s->sel) ? "listitem-selected" : "listitem";
    }
}

static void list_rebuild_rows(MuContext *ctx, MuNode *list) {
    MuListState *s = list_state(list);
    if (!ctx || !s || !s->scroll) return;
    MuNode *content = mu_scroll_content(s->scroll);
    if (!content) return;

    list_clear_rows(ctx, s);
    for (int i = 0; i < s->n; i++) {
        s->picks[i].ctx = ctx;
        s->picks[i].list = list;
        s->picks[i].index = i;

        MuNode *row = mu_make_panel(ctx, false);
        row->role = "listitem";
        mu_layout_set_padding(row, 8.f, 12.f, 8.f, 12.f);
        mu_layout_set_min_size(row, 0.f, 32.f);
        mu_panel_set_on_click(row, list_pick, &s->picks[i]);

        MuNode *lbl = mu_make_label(ctx, s->items[i]);
        mu_node_set_hit_transparent(lbl, true);
        mu_node_add_child(ctx, row, lbl);
        mu_node_add_child(ctx, content, row);
    }
    list_update_styles(list);
    mu_layout_mark_dirty(list);
}

static void list_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void list_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
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

static void list_layout_children(MuContext *ctx, MuNode *node) {
    MuListState *s = list_state(node);
    if (!s || !s->scroll) return;
    s->scroll->bounds = node->bounds;
    mu_layout_node(ctx, s->scroll);
}

static void list_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuColor border = ctx->style ? ctx->style->panel_border : st.border;
    mu_draw_rect(rc, node->bounds, (MuColor){0, 0, 0, 0}, border, 1.f, 6.f);
}

static bool list_hit(MuContext *ctx, MuNode *node, MuVec2 pt) {
    (void)ctx;
    return pt_in_rect(pt, node->bounds);
}

static bool list_ptr(MuContext *ctx, MuNode *node, const void *evp) {
    const MuPointerEvent *ev = (const MuPointerEvent *)evp;
    if (ev->pressed && pt_in_rect(ev->position, node->bounds)) mu_focus_set(ctx, node->id);
    return true;
}

static const MuNodeOps list_node_ops = {
    .destroy_state = list_destroy,
    .measure = list_measure,
    .layout_children = list_layout_children,
    .paint = list_paint,
    .hit_test = list_hit,
    .on_pointer = list_ptr,
    .on_key = NULL,
    .on_char = NULL,
};

void mu_list_register(MuContext *ctx) {
    (void)ctx;
    if (!list_kind_id) list_kind_id = mu_register_node_kind(ctx, "list", &list_node_ops);
}

uint32_t mu_kind_list(const MuContext *ctx) {
    (void)ctx;
    return list_kind_id;
}

MuNode *mu_make_list(MuContext *ctx, float min_height) {
    if (!ctx || !list_kind_id) return NULL;

    MuListState *s = (MuListState *)calloc(1, sizeof(MuListState));
    if (!s) return NULL;
    s->sel = -1;

    MuNode *list = mu_node_create(ctx, list_kind_id, s);
    if (!list) {
        free(s);
        return NULL;
    }

    MuNode *scroll = mu_make_scroll(ctx, MU_SCROLL_VERTICAL);
    if (!scroll) {
        mu_node_destroy_recursive(ctx, list);
        return NULL;
    }
    s->scroll = scroll;
    s->ctx = ctx;
    scroll->role = "scroll";
    mu_node_add_child(ctx, list, scroll);
    mu_layout_set_padding_all(scroll, 0.f);

    list->role = "list";
    list->flags |= MU_NODE_CLIP_CHILDREN;
    list->flags |= MU_NODE_FOCUSABLE;
    list_apply_height(list, min_height);
    return list;
}

void mu_list_add_item(MuContext *ctx, MuNode *list, const char *text) {
    if (!list) return;
    MuListState *s = list_state(list);
    if (!s || s->n >= MU_LIST_MAX_ITEMS) return;
    if (ctx) s->ctx = ctx;
    const char *src = text ? text : "";
    strncpy(s->items[s->n], src, MU_LIST_ITEM_LEN - 1);
    s->items[s->n][MU_LIST_ITEM_LEN - 1] = '\0';
    s->n++;
    if (s->ctx) list_rebuild_rows(s->ctx, list);
}

void mu_list_clear(MuContext *ctx, MuNode *list) {
    MuListState *s = list_state(list);
    if (!ctx || !s) return;
    s->n = 0;
    s->sel = -1;
    list_clear_rows(ctx, s);
    mu_layout_mark_dirty(list);
}

int mu_list_count(const MuNode *list) {
    const MuListState *s = list ? (const MuListState *)list->state : NULL;
    return s ? s->n : 0;
}

int mu_list_get_selection(const MuNode *list) {
    const MuListState *s = list ? (const MuListState *)list->state : NULL;
    return s ? s->sel : -1;
}

void mu_list_set_selection(MuContext *ctx, MuNode *list, int index) {
    MuListState *s = list_state(list);
    if (!ctx || !s || index < -1 || index >= s->n) return;
    if (s->sel == index) return;
    s->sel = index;
    list_update_styles(list);
    mu_layout_mark_dirty(list);
    if (index >= 0) list_ensure_visible(ctx, list, index);
    if (s->on_change) s->on_change(s->user, s->sel);
}

const char *mu_list_item_text(const MuNode *list, int index) {
    const MuListState *s = list ? (const MuListState *)list->state : NULL;
    if (!s || index < 0 || index >= s->n) return "";
    return s->items[index];
}

void mu_list_set_on_change(MuNode *list, void (*cb)(void *user, int index), void *user) {
    MuListState *s = list_state(list);
    if (!s) return;
    s->on_change = cb;
    s->user = user;
}
