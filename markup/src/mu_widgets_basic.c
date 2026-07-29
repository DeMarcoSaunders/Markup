#include "../include/markup/mu_widgets_basic.h"
#include "../include/markup/mu_popup.h"
#include "../include/markup/mu_image.h"
#include "../include/markup/mu_layout_flex.h"
#include "../include/markup/mu_style.h"
#include "../include/markup/mu_render.h"
#include "../include/markup/mu_input.h"
#include "../include/markup/mu_popup.h"
#include "../include/markup/mu_compose.h"
#include <stdlib.h>
#include <string.h>

static char *dup_cstr(const char *s) {
    const char *src = s ? s : "";
    size_t n = strlen(src) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, src, n);
    return p;
}

static bool widget_pt_in(MuNode *node, MuVec2 pt) {
    return pt.x >= node->bounds.x && pt.x < node->bounds.x + node->bounds.w && pt.y >= node->bounds.y &&
           pt.y < node->bounds.y + node->bounds.h;
}

typedef struct MuWidgetsBasicKindIds {
    uint32_t panel, label, button, slider, check, dropdown, text, modal, toast, image, scroll;
} MuWidgetsBasicKindIds;

extern const MuNodeOps mu_scroll_node_ops;

static const MuWidgetsBasicKindIds *wb_kinds(const MuContext *ctx) {
    if (!ctx) return NULL;
    return (const MuWidgetsBasicKindIds *)ctx->widgets_basic_kinds;
}

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
    MuNode *popup;
    struct {
        MuContext *ctx;
        MuNode *dropdown;
        int index;
    } picks[16];
} MuDropdownState;

typedef struct MuModalState {
    bool visible;
} MuModalState;

typedef struct MuToastState {
    char msg[128];
} MuToastState;

typedef struct MuImageState {
    uint32_t image_id;
    MuImageFit fit;
    MuColor tint;
    float radius;
    MuRect src;
    float intrinsic_w;
    float intrinsic_h;
} MuImageState;

typedef struct MuPanelState {
    void (*on_click)(void *);
    void *user;
} MuPanelState;

/* --- Panel --- */
static void panel_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void panel_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    if (node->flags & MU_NODE_FLEX_CONTAINER)
        mu_layout_measure_container(ctx, node, avail, out);
    else {
        out->x = 20.f;
        out->y = 20.f;
    }
}

static void panel_layout(MuContext *ctx, MuNode *node) {
    mu_layout_flex_run(ctx, node);
}

static void panel_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    float rad = (st.radius_tl + st.radius_tr + st.radius_br + st.radius_bl) * 0.25f;
    if (st.background.a > 0)
        mu_draw_rect(rc, node->bounds, st.background, (MuColor){0, 0, 0, 0}, 0.f, rad);
    if (st.border.a > 0 && st.border_width > 0.f)
        mu_draw_rect(rc, node->bounds, (MuColor){0, 0, 0, 0}, st.border, st.border_width, rad);
}

static bool panel_hit(MuContext *ctx, MuNode *node, MuVec2 pt) {
    MuPanelState *s = (MuPanelState *)node->state;
    if (!s || !s->on_click) return false;
    (void)ctx;
    return widget_pt_in(node, pt);
}

static bool panel_ptr(MuContext *ctx, MuNode *node, const void *evp) {
    const MuPointerEvent *ev = (const MuPointerEvent *)evp;
    MuPanelState *s = (MuPanelState *)node->state;
    if (!s || !s->on_click) return false;
    if (ev->released && widget_pt_in(node, ev->position)) s->on_click(s->user);
    return true;
}

static const MuNodeOps panel_ops = {.destroy_state = panel_destroy,
                                    .measure = panel_measure,
                                    .layout_children = panel_layout,
                                    .paint = panel_paint,
                                    .hit_test = panel_hit,
                                    .on_pointer = panel_ptr,
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
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuLabelState *s = (MuLabelState *)node->state;
    const char *t = s && s->text ? s->text : "";
    MuTextMetrics tm;
    float wrap_w = avail.x > 0.f ? avail.x - 4.f : 0.f;
    if (node->layout.max_width > 0.f && (wrap_w <= 0.f || node->layout.max_width < wrap_w))
        wrap_w = node->layout.max_width - 4.f;
    if (wrap_w > 8.f)
        mu_text_measure_wrapped(NULL, t, &st.text, wrap_w, &tm);
    else
        mu_text_measure(NULL, t, &st.text, &tm);
    out->x = tm.width + 4.f;
    out->y = tm.height + 4.f;
    if (avail.x > 0.f && out->x > avail.x) out->x = avail.x;
}

static void label_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuLabelState *s = (MuLabelState *)node->state;
    const char *t = s && s->text ? s->text : "";
    mu_push_scissor(rc, node->bounds);
    mu_draw_text_wrapped(rc, t, node->bounds, &st.text, st.foreground);
    mu_pop_scissor(rc);
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
    MuTextMetrics tm;
    mu_text_measure(NULL, t, &st.text, &tm);
    out->x = tm.width + 24;
    out->y = tm.height + 16;
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
    MuTextMetrics tm;
    mu_text_measure(rc, t, &st.text, &tm);
    float tx = node->bounds.x + (node->bounds.w - tm.width) * 0.5f;
    float ty = node->bounds.y + (node->bounds.h - tm.height) * 0.5f;
    mu_draw_text(rc, t, tx, ty, &st.text, st.foreground);
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
    out->x = avail.x > 0.f ? avail.x : 280.f;
    out->y = 28.f;
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

/* --- Dropdown (popup menu) --- */
static void dropdown_pick(void *user) {
    struct {
        MuContext *ctx;
        MuNode *dropdown;
        int index;
    } *u = user;
    if (!u || !u->ctx || !u->dropdown) return;
    MuDropdownState *s = (MuDropdownState *)u->dropdown->state;
    if (!s) return;
    if (u->index >= 0 && u->index < s->n) {
        s->sel = u->index;
        mu_layout_mark_dirty(u->dropdown);
    }
    if (s->popup) mu_popup_close(u->ctx, s->popup);
}

static void dropdown_clear_menu(MuContext *ctx, MuDropdownState *s) {
    if (!ctx || !s || !s->popup) return;
    MuNode *content = mu_popup_content(s->popup);
    if (!content) return;
    while (content->child_count > 0) {
        MuNode *ch = content->children[0];
        mu_node_remove_child(ctx, content, ch);
        mu_node_destroy_recursive(ctx, ch);
    }
}

static void dropdown_rebuild_menu(MuContext *ctx, MuNode *dd) {
    MuDropdownState *s = (MuDropdownState *)dd->state;
    if (!ctx || !s || !s->popup) return;
    MuNode *content = mu_popup_content(s->popup);
    if (!content) return;

    dropdown_clear_menu(ctx, s);
    for (int i = 0; i < s->n; i++) {
        s->picks[i].ctx = ctx;
        s->picks[i].dropdown = dd;
        s->picks[i].index = i;

        MuNode *row = mu_make_panel(ctx, false);
        row->role = "group";
        mu_layout_set_padding(row, 8.f, 12.f, 8.f, 12.f);
        mu_layout_set_gap(row, 0.f);
        mu_panel_set_on_click(row, dropdown_pick, &s->picks[i]);

        MuNode *lbl = mu_make_label(ctx, s->items[i]);
        if (i == s->sel) mu_node_set_text_weight(lbl, 600);
        mu_node_set_hit_transparent(lbl, true);
        mu_node_add_child(ctx, row, lbl);
        mu_node_add_child(ctx, content, row);
    }
    mu_layout_mark_dirty(s->popup);
}

static void dd_destroy(MuContext *ctx, MuNode *node) {
    MuDropdownState *s = (MuDropdownState *)node->state;
    if (s && s->popup) mu_popup_close(ctx, s->popup);
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void dd_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuDropdownState *s = (MuDropdownState *)node->state;
    float min_w = 120.f;
    if (s) {
        for (int i = 0; i < s->n; i++) {
            MuTextMetrics tm;
            mu_text_measure(NULL, s->items[i], &st.text, &tm);
            if (tm.width + 56.f > min_w) min_w = tm.width + 56.f;
        }
    }
    out->x = avail.x > min_w ? avail.x : min_w;
    out->y = 38.f;
}

static bool dd_ptr(MuContext *ctx, MuNode *node, const void *evp) {
    const MuPointerEvent *ev = (const MuPointerEvent *)evp;
    MuDropdownState *s = (MuDropdownState *)node->state;
    if (!s || s->n <= 0 || !widget_pt_in(node, ev->position)) return false;
    if (ev->pressed) {
        if (!s->popup && ctx->popup_layer) {
            s->popup = mu_make_popup(ctx);
            if (s->popup) {
                mu_popup_set_anchor(s->popup, node);
                mu_popup_set_placement(s->popup, MU_POPUP_AUTO);
            }
        }
        if (s->popup) {
            dropdown_rebuild_menu(ctx, node);
            mu_popup_set_anchor(s->popup, node);
            mu_popup_toggle(ctx, s->popup);
        }
    }
    return true;
}

static void dd_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuDropdownState *s = (MuDropdownState *)node->state;
    MuColor border = st.border;
    float border_w = 1.f;
    if (node->flags & MU_NODE_HOVERED) {
        border = st.foreground;
        border_w = 2.f;
    }
    mu_draw_rect(rc, node->bounds, st.background, border, border_w, 6.f);
    const char *t = (s && s->n > 0) ? s->items[s->sel] : "Select…";
    MuTextMetrics tm;
    mu_text_measure(rc, t, &st.text, &tm);
    float ty = node->bounds.y + (node->bounds.h - tm.height) * 0.5f;
    mu_draw_text(rc, t, node->bounds.x + 10.f, ty, &st.text, st.foreground);
    MuColor chevron = ctx->style ? ctx->style->muted_fg : (MuColor){140, 150, 170, 255};
    mu_draw_text(rc, "v", node->bounds.x + node->bounds.w - 18.f, ty, &st.text, chevron);
}

static const MuNodeOps dropdown_ops = {.destroy_state = dd_destroy,
                                        .measure = dd_measure,
                                        .layout_children = NULL,
                                        .paint = dd_paint,
                                        .hit_test = NULL,
                                        .on_pointer = dd_ptr,
                                        .on_key = NULL,
                                        .on_char = NULL};

extern const MuNodeOps mu_textinput_node_ops;

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
    for (int i = 0; i < node->child_count; i++) mu_layout_node(ctx, node->children[i]);
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
    MuTextMetrics tm;
    mu_text_measure(NULL, t, &st.text, &tm);
    (void)avail;
    out->x = tm.width + 20;
    out->y = tm.height + 16;
}

static void toast_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuToastState *s = (MuToastState *)node->state;
    mu_draw_rect(rc, node->bounds, st.background, st.background, 0, st.radius_tl);
    mu_draw_text(rc, s ? s->msg : "", node->bounds.x + 10, node->bounds.y + 8, &st.text, st.foreground);
}

static const MuNodeOps toast_ops = {.destroy_state = toast_destroy,
                                     .measure = toast_measure,
                                     .layout_children = NULL,
                                     .paint = toast_paint,
                                     .hit_test = NULL,
                                     .on_pointer = NULL,
                                     .on_key = NULL,
                                     .on_char = NULL};

/* --- Image --- */
static void image_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void image_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    (void)ctx;
    MuImageState *s = (MuImageState *)node->state;
    float w = 64.f, h = 64.f;
    if (s) {
        if (s->intrinsic_w > 0.f && s->intrinsic_h > 0.f) {
            w = s->intrinsic_w;
            h = s->intrinsic_h;
        } else if (s->image_id != MU_IMAGE_INVALID) {
            float iw = 0.f, ih = 0.f;
            if (mu_image_get_size(NULL, s->image_id, &iw, &ih) && iw > 0.f && ih > 0.f) {
                MuRect resolved;
                if (mu_image_resolve_src(&s->src, iw, ih, &resolved)) {
                    w = resolved.w;
                    h = resolved.h;
                } else {
                    w = iw;
                    h = ih;
                }
            }
        }
    }
    const MuLayoutStyle *fl = &node->layout;
    if (fl->min_width > 0.f) w = fl->min_width;
    if (fl->min_height > 0.f) h = fl->min_height;
    if (fl->max_width > 0.f && w > fl->max_width) w = fl->max_width;
    if (fl->max_height > 0.f && h > fl->max_height) h = fl->max_height;
    if (avail.x > 0.f && w > avail.x) w = avail.x;
    if (avail.y > 0.f && h > avail.y) h = avail.y;
    out->x = w;
    out->y = h;
}

static void image_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuImageState *s = (MuImageState *)node->state;
    if (!s || s->image_id == MU_IMAGE_INVALID) {
        MuStyleSnapshot st;
        mu_style_resolve(ctx, node, &st);
        mu_draw_rect(rc, node->bounds, (MuColor){st.foreground.r, st.foreground.g, st.foreground.b, 40},
                     st.border, 1.f, s ? s->radius : 4.f);
        return;
    }

    MuDrawImageOpts opts;
    mu_draw_image_opts_init(&opts);
    opts.fit = s->fit;
    opts.tint = s->tint;
    opts.radius = s->radius;
    opts.src = s->src;
    mu_draw_image(rc, s->image_id, node->bounds, &opts);
}

static const MuNodeOps image_ops = {.destroy_state = image_destroy,
                                    .measure = image_measure,
                                    .layout_children = NULL,
                                    .paint = image_paint,
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
    k->text = mu_register_node_kind(ctx, "textinput", &mu_textinput_node_ops);
    k->modal = mu_register_node_kind(ctx, "modal", &modal_ops);
    k->toast = mu_register_node_kind(ctx, "toast", &toast_ops);
    k->image = mu_register_node_kind(ctx, "image", &image_ops);
    k->scroll = mu_register_node_kind(ctx, "scroll", &mu_scroll_node_ops);
    ctx->widgets_basic_kinds = k;
    mu_popup_register(ctx);
    mu_list_register(ctx);
    mu_tabs_register(ctx);
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
uint32_t mu_kind_image(const MuContext *ctx) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    return k ? k->image : 0;
}
uint32_t mu_kind_scroll(const MuContext *ctx) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    return k ? k->scroll : 0;
}

/* --- Convenience constructors (optional for apps) --- */
MuNode *mu_make_panel(MuContext *ctx, bool column) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    if (!k)
        return NULL;
    MuPanelState *ps = (MuPanelState *)calloc(1, sizeof(MuPanelState));
    if (!ps) return NULL;
    MuNode *n = mu_node_create(ctx, k->panel, ps);
    if (!n) {
        free(ps);
        return NULL;
    }
    mu_layout_set_flex_direction(n, column ? MU_FLEX_COLUMN : MU_FLEX_ROW);
    mu_layout_set_justify(n, MU_JUSTIFY_START);
    mu_layout_set_align_items(n, MU_ALIGN_STRETCH);
    mu_layout_set_gap(n, 8.f);
    mu_layout_set_padding_all(n, 12.f);
    n->role = "panel";
    n->flags |= MU_NODE_FLEX_CONTAINER | MU_NODE_CLIP_CHILDREN;
    return n;
}

void mu_panel_set_on_click(MuNode *panel, void (*on_click)(void *), void *user) {
    if (!panel) return;
    MuPanelState *s = (MuPanelState *)panel->state;
    if (!s) return;
    s->on_click = on_click;
    s->user = user;
    if (on_click) {
        panel->flags |= MU_NODE_FOCUSABLE;
        if (!panel->role || panel->role[0] == '\0' || strcmp(panel->role, "panel") == 0 ||
            strcmp(panel->role, "group") == 0)
            panel->role = "tile";
    }
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
    n->layout.flex_shrink = 0.f;
    if (ctx->popup_layer) {
        s->popup = mu_make_popup(ctx);
        if (s->popup) {
            mu_popup_set_anchor(s->popup, n);
            mu_popup_set_placement(s->popup, MU_POPUP_AUTO);
        }
    }
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
    mu_layout_mark_dirty(modal);
    if (show) {
        modal->flags |= MU_NODE_VISIBLE;
        mu_modal_push(ctx, modal->id);
    } else {
        modal->flags &= ~MU_NODE_VISIBLE;
        if (mu_modal_top(ctx) == modal->id) mu_modal_pop(ctx);
    }
}

void mu_label_set_text(MuNode *label, const char *text) {
    if (!label) return;
    MuLabelState *s = (MuLabelState *)label->state;
    if (!s) return;
    char *next = dup_cstr(text);
    if (!next) return;
    free(s->text);
    s->text = next;
    mu_layout_mark_dirty(label);
}

float mu_slider_get_value(const MuNode *slider) {
    if (!slider) return 0.f;
    const MuSliderState *s = (const MuSliderState *)slider->state;
    return s ? s->value : 0.f;
}

const char *mu_dropdown_selected_text(const MuNode *dropdown) {
    if (!dropdown) return "";
    const MuDropdownState *s = (const MuDropdownState *)dropdown->state;
    if (!s || s->n <= 0 || s->sel < 0 || s->sel >= s->n) return "";
    return s->items[s->sel];
}

int mu_dropdown_get_selection(const MuNode *dropdown) {
    if (!dropdown) return -1;
    const MuDropdownState *s = (const MuDropdownState *)dropdown->state;
    if (!s || s->n <= 0) return -1;
    return s->sel;
}

void mu_toast_set_message(MuNode *toast, const char *msg) {
    if (!toast) return;
    MuToastState *s = (MuToastState *)toast->state;
    if (!s) return;
    strncpy(s->msg, msg ? msg : "", sizeof(s->msg) - 1);
    s->msg[sizeof(s->msg) - 1] = '\0';
    mu_layout_mark_dirty(toast);
}

MuNode *mu_make_image(MuContext *ctx, uint32_t image_id) {
    const MuWidgetsBasicKindIds *k = wb_kinds(ctx);
    if (!k) return NULL;
    MuImageState *s = (MuImageState *)calloc(1, sizeof(MuImageState));
    if (!s) return NULL;
    s->image_id = image_id;
    s->fit = MU_IMAGE_FIT_CONTAIN;
    s->tint = (MuColor){255, 255, 255, 255};
    s->radius = 8.f;
    MuNode *n = mu_node_create(ctx, k->image, s);
    if (!n) {
        free(s);
        return NULL;
    }
    n->role = "image";
    n->layout.flex_shrink = 0.f;
    return n;
}

void mu_image_set_id(MuNode *image, uint32_t image_id) {
    if (!image) return;
    MuImageState *s = (MuImageState *)image->state;
    if (!s) return;
    s->image_id = image_id;
    s->intrinsic_w = 0.f;
    s->intrinsic_h = 0.f;
    mu_layout_mark_dirty(image);
}

uint32_t mu_image_get_id(const MuNode *image) {
    if (!image) return MU_IMAGE_INVALID;
    const MuImageState *s = (const MuImageState *)image->state;
    return s ? s->image_id : MU_IMAGE_INVALID;
}

bool mu_image_set_source(MuNode *image, MuRenderContext *rc, const char *path) {
    if (!image || !rc || !path) return false;
    uint32_t id = mu_image_load_file(rc, path);
    if (id == MU_IMAGE_INVALID) return false;
    mu_image_set_id(image, id);
    float w = 0.f, h = 0.f;
    if (mu_image_get_size(rc, id, &w, &h)) {
        MuImageState *s = (MuImageState *)image->state;
        if (s) {
            s->intrinsic_w = w;
            s->intrinsic_h = h;
        }
    }
    return true;
}

void mu_image_set_fit(MuNode *image, MuImageFit fit) {
    if (!image) return;
    MuImageState *s = (MuImageState *)image->state;
    if (!s) return;
    s->fit = fit;
}

void mu_image_set_tint(MuNode *image, MuColor tint) {
    if (!image) return;
    MuImageState *s = (MuImageState *)image->state;
    if (!s) return;
    s->tint = tint;
}

void mu_image_set_radius(MuNode *image, float radius) {
    if (!image) return;
    MuImageState *s = (MuImageState *)image->state;
    if (!s) return;
    s->radius = radius;
}

void mu_image_set_src_rect(MuNode *image, MuRect src) {
    if (!image) return;
    MuImageState *s = (MuImageState *)image->state;
    if (!s) return;
    s->src = src;
    s->intrinsic_w = 0.f;
    s->intrinsic_h = 0.f;
    mu_layout_mark_dirty(image);
}
