#include "../include/markup/mu_widgets_basic.h"
#include "../include/markup/mu_layout_flex.h"
#include "../include/markup/mu_style.h"
#include "../include/markup/mu_raylib.h"
#include "../include/markup/mu_input.h"
#include <raylib.h>
#include <stdlib.h>
#include <string.h>

static char *dup_cstr(const char *s) {
    const char *src = s ? s : "";
    size_t n = strlen(src) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, src, n);
    return p;
}

typedef struct MuWidgetsBasicKindIds {
    uint32_t panel, label, button, slider, check, dropdown, text, modal, toast;
} MuWidgetsBasicKindIds;

static const MuWidgetsBasicKindIds *wb_kinds(const MuContext *ctx) {
    if (!ctx) return NULL;
    return (const MuWidgetsBasicKindIds *)ctx->widgets_basic_kinds;
}

typedef struct MuPanelState {
    MuFlexLayoutState flex;
} MuPanelState;

typedef struct MuLabelState {
    char *text;
} MuLabelState;

typedef struct MuButtonState {
    char *text;
    void (*on_click)(void *);
    void *user;
} MuButtonState;

typedef struct MuSliderState {
    float min_v, max_v, value;
} MuSliderState;

typedef struct MuCheckboxState {
    bool checked;
    void (*on_toggle)(void *, bool);
    void *user;
} MuCheckboxState;

typedef struct MuDropdownState {
    char items[16][64];
    int n;
    int sel;
} MuDropdownState;

typedef struct MuTextInState {
    char buf[256];
    int len;
} MuTextInState;

typedef struct MuModalState {
    bool visible;
} MuModalState;

typedef struct MuToastState {
    char msg[128];
} MuToastState;

/* --- Panel --- */
static void panel_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void panel_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    MuPanelState *ps = (MuPanelState *)node->state;
    MuFlexLayoutState *fl = &ps->flex;
    float mw = 20, mh = 20;
    for (int i = 0; i < node->child_count; i++) {
        MuNode *ch = node->children[i];
        if (!(ch->flags & MU_NODE_VISIBLE)) continue;
        MuVec2 d = {40, 24};
        const MuNodeOps *ops = mu_get_node_ops(ctx, ch->kind);
        if (ops && ops->measure) ops->measure(ctx, ch, avail, &d);
        if (fl->direction == MU_FLEX_ROW) {
            mw += d.x + fl->gap;
            if (d.y > mh) mh = d.y;
        } else {
            mh += d.y + fl->gap;
            if (d.x > mw) mw = d.x;
        }
    }
    mw += fl->pad_left + fl->pad_right;
    mh += fl->pad_top + fl->pad_bottom;
    out->x = mw;
    out->y = mh;
}

static void panel_layout(MuContext *ctx, MuNode *node) {
    mu_layout_flex_run(ctx, node);
}

static void panel_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    float rad = (st.radius_tl + st.radius_tr + st.radius_br + st.radius_bl) * 0.25f;
    mu_draw_rect(rc, node->bounds, st.background, st.border, st.border_width, rad);
}

static const MuNodeOps panel_ops = {.destroy_state = panel_destroy,
                                    .measure = panel_measure,
                                    .layout_children = panel_layout,
                                    .paint = panel_paint,
                                    .hit_test = NULL,
                                    .on_pointer = NULL,
                                    .on_key = NULL,
                                    .on_char = NULL};

/* --- Label --- */
static void label_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    MuLabelState *s = (MuLabelState *)node->state;
    if (s) {
        free(s->text);
        free(s);
    }
    node->state = NULL;
}

static void label_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    (void)avail;
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuLabelState *s = (MuLabelState *)node->state;
    const char *t = s && s->text ? s->text : "";
    Font f = mu_raylib_ui_font();
    Vector2 sz = MeasureTextEx(f, t, st.font_size, 1.0f);
    out->x = sz.x + 4;
    out->y = sz.y + 4;
}

static void label_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuLabelState *s = (MuLabelState *)node->state;
    const char *t = s && s->text ? s->text : "";
    mu_draw_text(rc, t, node->bounds.x + 2, node->bounds.y + 2, st.font_size, st.foreground);
}

static const MuNodeOps label_ops = {.destroy_state = label_destroy,
                                    .measure = label_measure,
                                    .layout_children = NULL,
                                    .paint = label_paint,
                                    .hit_test = NULL,
                                    .on_pointer = NULL,
                                    .on_key = NULL,
                                    .on_char = NULL};

/* --- Button --- */
static void btn_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    MuButtonState *s = (MuButtonState *)node->state;
    if (s) {
        free(s->text);
        free(s);
    }
    node->state = NULL;
}

static void btn_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    (void)avail;
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuButtonState *s = (MuButtonState *)node->state;
    const char *t = s && s->text ? s->text : "Btn";
    Font f = mu_raylib_ui_font();
    Vector2 sz = MeasureTextEx(f, t, st.font_size, 1.0f);
    out->x = sz.x + 24;
    out->y = sz.y + 16;
}

static bool btn_pt_in(MuContext *ctx, MuNode *node, MuVec2 pt) {
    (void)ctx;
    return pt.x >= node->bounds.x && pt.x < node->bounds.x + node->bounds.w && pt.y >= node->bounds.y &&
           pt.y < node->bounds.y + node->bounds.h;
}

static bool btn_ptr(MuContext *ctx, MuNode *node, const void *evp) {
    const MuPointerEvent *ev = (const MuPointerEvent *)evp;
    MuButtonState *s = (MuButtonState *)node->state;
    if (!s) return false;
    if (ev->released && btn_pt_in(ctx, node, ev->position) && s->on_click) s->on_click(s->user);
    (void)ev->pressed;
    (void)ev->drag;
    return true;
}

static void btn_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    float rad = (st.radius_tl + st.radius_tr + st.radius_br + st.radius_bl) * 0.25f;
    mu_draw_rect(rc, node->bounds, st.background, st.border, st.border_width, rad);
    MuButtonState *s = (MuButtonState *)node->state;
    const char *t = s && s->text ? s->text : "";
    Font f = mu_raylib_ui_font();
    Vector2 sz = MeasureTextEx(f, t, st.font_size, 1.0f);
    float tx = node->bounds.x + (node->bounds.w - sz.x) * 0.5f;
    float ty = node->bounds.y + (node->bounds.h - sz.y) * 0.5f;
    mu_draw_text(rc, t, tx, ty, st.font_size, st.foreground);
}

static const MuNodeOps button_ops = {.destroy_state = btn_destroy,
                                     .measure = btn_measure,
                                     .layout_children = NULL,
                                     .paint = btn_paint,
                                     .hit_test = btn_pt_in,
                                     .on_pointer = btn_ptr,
                                     .on_key = NULL,
                                     .on_char = NULL};

/* --- Slider --- */
static void slider_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void slider_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    (void)ctx;
    (void)node;
    (void)avail;
    out->x = 200;
    out->y = 24;
}

static bool slider_ptr(MuContext *ctx, MuNode *node, const void *evp) {
    const MuPointerEvent *ev = (const MuPointerEvent *)evp;
    MuSliderState *s = (MuSliderState *)node->state;
    if (!s) return false;
    if (ev->pressed) {
        ctx->captured_pointer_id = node->id;
        /* jump to pos */
    }
    if (ev->drag || ev->pressed) {
        float t = (ev->position.x - node->bounds.x) / node->bounds.w;
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        s->value = s->min_v + t * (s->max_v - s->min_v);
    }
    if (ev->released) ctx->captured_pointer_id = 0;
    return true;
}

static void slider_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuSliderState *s = (MuSliderState *)node->state;
    mu_draw_rect(rc, node->bounds, st.background, st.border, 1, 4);
    if (s) {
        float t = (s->value - s->min_v) / (s->max_v - s->min_v + 1e-6f);
        MuRect thumb = {node->bounds.x + t * (node->bounds.w - 14), node->bounds.y + 2, 12, node->bounds.h - 4};
        mu_draw_rect(rc, thumb, st.foreground, st.foreground, 0, 6);
    }
}

static const MuNodeOps slider_ops = {.destroy_state = slider_destroy,
                                       .measure = slider_measure,
                                       .layout_children = NULL,
                                       .paint = slider_paint,
                                       .hit_test = NULL,
                                       .on_pointer = slider_ptr,
                                       .on_key = NULL,
                                       .on_char = NULL};

/* --- Checkbox --- */
static void cb_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void cb_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    (void)ctx;
    (void)node;
    (void)avail;
    out->x = 22;
    out->y = 22;
}

static bool cb_ptr(MuContext *ctx, MuNode *node, const void *evp) {
    const MuPointerEvent *ev = (const MuPointerEvent *)evp;
    MuCheckboxState *s = (MuCheckboxState *)node->state;
    if (!s) return false;
    if (ev->released) {
        float mx = ev->position.x, my = ev->position.y;
        if (mx >= node->bounds.x && mx < node->bounds.x + node->bounds.w && my >= node->bounds.y &&
            my < node->bounds.y + node->bounds.h) {
            s->checked = !s->checked;
            if (s->on_toggle) s->on_toggle(s->user, s->checked);
        }
    }
    return true;
}

static void cb_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuCheckboxState *s = (MuCheckboxState *)node->state;
    mu_draw_rect(rc, node->bounds, st.background, st.border, 2, 4);
    if (s && s->checked) {
        MuRect in = {node->bounds.x + 5, node->bounds.y + 5, node->bounds.w - 10, node->bounds.h - 10};
        mu_draw_rect(rc, in, st.foreground, st.foreground, 0, 2);
    }
}

static const MuNodeOps checkbox_ops = {.destroy_state = cb_destroy,
                                        .measure = cb_measure,
                                        .layout_children = NULL,
                                        .paint = cb_paint,
                                        .hit_test = NULL,
                                        .on_pointer = cb_ptr,
                                        .on_key = NULL,
                                        .on_char = NULL};

/* --- Dropdown (cycle on click) --- */
static void dd_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void dd_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    (void)avail;
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuDropdownState *s = (MuDropdownState *)node->state;
    Font f = mu_raylib_ui_font();
    float maxw = 80;
    if (s) {
        for (int i = 0; i < s->n; i++) {
            Vector2 sz = MeasureTextEx(f, s->items[i], st.font_size, 1.0f);
            if (sz.x + 40 > maxw) maxw = sz.x + 40;
        }
    }
    out->x = maxw;
    out->y = 36;
}

static bool dd_ptr(MuContext *ctx, MuNode *node, const void *evp) {
    const MuPointerEvent *ev = (const MuPointerEvent *)evp;
    MuDropdownState *s = (MuDropdownState *)node->state;
    if (!s || s->n <= 0) return false;
    if (ev->released) {
        float mx = ev->position.x, my = ev->position.y;
        if (mx >= node->bounds.x && mx < node->bounds.x + node->bounds.w && my >= node->bounds.y &&
            my < node->bounds.y + node->bounds.h)
            s->sel = (s->sel + 1) % s->n;
    }
    (void)ctx;
    return true;
}

static void dd_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuDropdownState *s = (MuDropdownState *)node->state;
    mu_draw_rect(rc, node->bounds, st.background, st.border, 1, st.radius_tl);
    const char *t = (s && s->n > 0) ? s->items[s->sel] : "";
    mu_draw_text(rc, t, node->bounds.x + 8, node->bounds.y + 8, st.font_size, st.foreground);
}

static const MuNodeOps dropdown_ops = {.destroy_state = dd_destroy,
                                        .measure = dd_measure,
                                        .layout_children = NULL,
                                        .paint = dd_paint,
                                        .hit_test = NULL,
                                        .on_pointer = dd_ptr,
                                        .on_key = NULL,
                                        .on_char = NULL};

/* --- Text input --- */
static void ti_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void ti_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    (void)ctx;
    (void)node;
    (void)avail;
    out->x = 180;
    out->y = 32;
}

static bool ti_ptr(MuContext *ctx, MuNode *node, const void *evp) {
    const MuPointerEvent *ev = (const MuPointerEvent *)evp;
    if (ev->pressed) mu_focus_set(ctx, node->id);
    return true;
}

static bool ti_key(MuContext *ctx, MuNode *node, const void *evp) {
    const MuKeyEvent *ev = (const MuKeyEvent *)evp;
    MuTextInState *s = (MuTextInState *)node->state;
    if (!s) return false;
    if (ev->key == KEY_BACKSPACE && s->len > 0) {
        s->buf[--s->len] = '\0';
        return true;
    }
    (void)ctx;
    return false;
}

static bool ti_char(MuContext *ctx, MuNode *n, unsigned int cp) {
    MuTextInState *s = (MuTextInState *)n->state;
    if (!s) return false;
    if (cp >= 32u && cp < 127u && s->len < 255) {
        s->buf[s->len++] = (char)cp;
        s->buf[s->len] = '\0';
    }
    (void)ctx;
    return true;
}

static void ti_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    mu_draw_rect(rc, node->bounds, st.background, st.border, 1, 4);
    MuTextInState *s = (MuTextInState *)node->state;
    mu_draw_text(rc, s ? s->buf : "", node->bounds.x + 6, node->bounds.y + 7, st.font_size, st.foreground);
}

static const MuNodeOps textinput_ops = {.destroy_state = ti_destroy,
                                         .measure = ti_measure,
                                         .layout_children = NULL,
                                         .paint = ti_paint,
                                         .hit_test = NULL,
                                         .on_pointer = ti_ptr,
                                         .on_key = ti_key,
                                         .on_char = ti_char};

/* --- Modal --- */
static void modal_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void modal_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    (void)ctx;
    (void)node;
    out->x = avail.x;
    out->y = avail.y;
}

static void modal_layout(MuContext *ctx, MuNode *node) {
    MuModalState *m = (MuModalState *)node->state;
    if (!m || !m->visible) return;
    for (int i = 0; i < node->child_count; i++) {
        MuNode *ch = node->children[i];
        const MuNodeOps *ops = mu_get_node_ops(ctx, ch->kind);
        if (ops && ops->layout_children) ops->layout_children(ctx, ch);
        else if (ch->flags & MU_NODE_FLEX_CONTAINER) mu_layout_flex_run(ctx, ch);
    }
}

static void modal_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuModalState *m = (MuModalState *)node->state;
    if (!m || !m->visible) return;
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuRect full = node->bounds;
    MuColor dim = ctx->style ? ctx->style->modal_overlay : (MuColor){0, 0, 0, 160};
    mu_draw_rect(rc, full, dim, (MuColor){0, 0, 0, 0}, 0, 0);
    for (int i = 0; i < node->child_count; i++) mu_paint_tree(ctx, rc, node->children[i]);
    (void)st;
}

static const MuNodeOps modal_ops = {.destroy_state = modal_destroy,
                                      .measure = modal_measure,
                                      .layout_children = modal_layout,
                                      .paint = modal_paint,
                                      .hit_test = NULL,
                                      .on_pointer = NULL,
                                      .on_key = NULL,
                                      .on_char = NULL};

/* --- Toast --- */
static void toast_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void toast_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    MuToastState *s = (MuToastState *)node->state;
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    const char *t = s && s->msg[0] ? s->msg : "";
    Font f = mu_raylib_ui_font();
    Vector2 sz = MeasureTextEx(f, t, st.font_size, 1.0f);
    (void)avail;
    out->x = sz.x + 20;
    out->y = sz.y + 16;
}

static void toast_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuToastState *s = (MuToastState *)node->state;
    mu_draw_rect(rc, node->bounds, st.background, st.background, 0, st.radius_tl);
    mu_draw_text(rc, s ? s->msg : "", node->bounds.x + 10, node->bounds.y + 8, st.font_size, st.foreground);
}

static const MuNodeOps toast_ops = {.destroy_state = toast_destroy,
                                     .measure = toast_measure,
                                     .layout_children = NULL,
                                     .paint = toast_paint,
                                     .hit_test = NULL,
                                     .on_pointer = NULL,
                                     .on_key = NULL,
                                     .on_char = NULL};

void mu_widgets_basic_register(MuContext *ctx) {
    if (!ctx || ctx->widgets_basic_kinds)
        return;
    MuWidgetsBasicKindIds *k = (MuWidgetsBasicKindIds *)calloc(1, sizeof(*k));
    if (!k)
        return;
    k->panel = mu_register_node_kind(ctx, "panel", &panel_ops);
    k->label = mu_register_node_kind(ctx, "label", &label_ops);
    k->button = mu_register_node_kind(ctx, "button", &button_ops);
    k->slider = mu_register_node_kind(ctx, "slider", &slider_ops);
    k->check = mu_register_node_kind(ctx, "checkbox", &checkbox_ops);
    k->dropdown = mu_register_node_kind(ctx, "dropdown", &dropdown_ops);
    k->text = mu_register_node_kind(ctx, "textinput", &textinput_ops);
    k->modal = mu_register_node_kind(ctx, "modal", &modal_ops);
    k->toast = mu_register_node_kind(ctx, "toast", &toast_ops);
    ctx->widgets_basic_kinds = k;
}

uint32_t mu_kind_panel(const MuContext *ctx) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    return k ? k->panel : 0;
}
uint32_t mu_kind_label(const MuContext *ctx) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    return k ? k->label : 0;
}
uint32_t mu_kind_button(const MuContext *ctx) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    return k ? k->button : 0;
}
uint32_t mu_kind_slider(const MuContext *ctx) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    return k ? k->slider : 0;
}
uint32_t mu_kind_checkbox(const MuContext *ctx) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    return k ? k->check : 0;
}
uint32_t mu_kind_dropdown(const MuContext *ctx) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    return k ? k->dropdown : 0;
}
uint32_t mu_kind_textinput(const MuContext *ctx) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    return k ? k->text : 0;
}
uint32_t mu_kind_modal(const MuContext *ctx) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    return k ? k->modal : 0;
}
uint32_t mu_kind_toast(const MuContext *ctx) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    return k ? k->toast : 0;
}

/* --- Convenience constructors (optional for apps) --- */
MuNode *mu_make_panel(MuContext *ctx, bool column) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    if (!k)
        return NULL;
    MuPanelState *ps = (MuPanelState *)calloc(1, sizeof(MuPanelState));
    if (!ps) return NULL;
    ps->flex.direction = column ? MU_FLEX_COLUMN : MU_FLEX_ROW;
    ps->flex.justify = MU_JUSTIFY_START;
    ps->flex.align_items = MU_ALIGN_STRETCH;
    ps->flex.gap = 8;
    ps->flex.pad_left = ps->flex.pad_right = ps->flex.pad_top = ps->flex.pad_bottom = 12;
    MuNode *n = mu_node_create(ctx, k->panel, ps);
    if (!n) {
        free(ps);
        return NULL;
    }
    n->role = "panel";
    n->flags |= MU_NODE_FLEX_CONTAINER | MU_NODE_CLIP_CHILDREN;
    return n;
}

MuNode *mu_make_label(MuContext *ctx, const char *text) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    if (!k)
        return NULL;
    MuLabelState *s = (MuLabelState *)calloc(1, sizeof(MuLabelState));
    if (!s) return NULL;
    s->text = dup_cstr(text);
    MuNode *n = mu_node_create(ctx, k->label, s);
    if (!n) {
        free(s->text);
        free(s);
        return NULL;
    }
    n->role = "label";
    return n;
}

MuNode *mu_make_button(MuContext *ctx, const char *text, void (*cb)(void *), void *user) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    if (!k)
        return NULL;
    MuButtonState *s = (MuButtonState *)calloc(1, sizeof(MuButtonState));
    if (!s) return NULL;
    s->text = dup_cstr(text ? text : "OK");
    s->on_click = cb;
    s->user = user;
    MuNode *n = mu_node_create(ctx, k->button, s);
    if (!n) {
        free(s->text);
        free(s);
        return NULL;
    }
    n->role = "button";
    n->flags |= MU_NODE_FOCUSABLE;
    return n;
}

MuNode *mu_make_slider(MuContext *ctx, float mn, float mx, float val) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    if (!k)
        return NULL;
    MuSliderState *s = (MuSliderState *)calloc(1, sizeof(MuSliderState));
    if (!s) return NULL;
    s->min_v = mn;
    s->max_v = mx;
    s->value = val;
    MuNode *n = mu_node_create(ctx, k->slider, s);
    if (!n) {
        free(s);
        return NULL;
    }
    n->role = "slider";
    return n;
}

MuNode *mu_make_checkbox(MuContext *ctx, bool on, void (*on_toggle)(void *, bool), void *user) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    if (!k)
        return NULL;
    MuCheckboxState *s = (MuCheckboxState *)calloc(1, sizeof(MuCheckboxState));
    if (!s) return NULL;
    s->checked = on;
    s->on_toggle = on_toggle;
    s->user = user;
    MuNode *n = mu_node_create(ctx, k->check, s);
    if (!n) {
        free(s);
        return NULL;
    }
    n->role = "checkbox";
    return n;
}

MuNode *mu_make_dropdown(MuContext *ctx) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    if (!k)
        return NULL;
    MuDropdownState *s = (MuDropdownState *)calloc(1, sizeof(MuDropdownState));
    if (!s) return NULL;
    MuNode *n = mu_node_create(ctx, k->dropdown, s);
    if (!n) {
        free(s);
        return NULL;
    }
    n->role = "dropdown";
    return n;
}

static void dropdown_add_internal(MuDropdownState *d, const char *item) {
    if (!d || !item || d->n >= 16) return;
    strncpy(d->items[d->n], item, sizeof(d->items[0]) - 1);
    d->items[d->n][sizeof(d->items[0]) - 1] = '\0';
    d->n++;
}

void mu_dropdown_add_option(MuNode *dd, const char *item) {
    if (!dd || !dd->state) return;
    dropdown_add_internal((MuDropdownState *)dd->state, item);
}

MuNode *mu_make_textinput(MuContext *ctx, const char *initial) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    if (!k)
        return NULL;
    MuTextInState *s = (MuTextInState *)calloc(1, sizeof(MuTextInState));
    if (!s) return NULL;
    if (initial) {
        strncpy(s->buf, initial, sizeof(s->buf) - 1);
        s->len = (int)strlen(s->buf);
    }
    MuNode *n = mu_node_create(ctx, k->text, s);
    if (!n) {
        free(s);
        return NULL;
    }
    n->role = "input";
    n->flags |= MU_NODE_FOCUSABLE;
    return n;
}

MuNode *mu_make_modal(MuContext *ctx) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    if (!k)
        return NULL;
    MuModalState *m = (MuModalState *)calloc(1, sizeof(MuModalState));
    if (!m) return NULL;
    MuNode *n = mu_node_create(ctx, k->modal, m);
    if (!n) {
        free(m);
        return NULL;
    }
    n->role = "modal";
    n->flags &= ~MU_NODE_VISIBLE;
    return n;
}

MuNode *mu_make_toast(MuContext *ctx, const char *msg) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    if (!k)
        return NULL;
    MuToastState *s = (MuToastState *)calloc(1, sizeof(MuToastState));
    if (!s) return NULL;
    if (msg) strncpy(s->msg, msg, sizeof(s->msg) - 1);
    MuNode *n = mu_node_create(ctx, k->toast, s);
    if (!n) {
        free(s);
        return NULL;
    }
    n->role = "toast";
    return n;
}

void mu_modal_bind_layer(MuContext *ctx, MuNode *modal_root) {
    if (ctx) ctx->modal_layer = modal_root;
}

void mu_modal_set_visible(MuContext *ctx, MuNode *modal, bool show) {
    if (!ctx || !modal) return;
    MuModalState *m = (MuModalState *)modal->state;
    if (!m) return;
    m->visible = show;
    if (show) {
        modal->flags |= MU_NODE_VISIBLE;
        mu_modal_push(ctx, modal->id);
    } else {
        modal->flags &= ~MU_NODE_VISIBLE;
        if (mu_modal_top(ctx) == modal->id) mu_modal_pop(ctx);
    }
}
