#include "../include/markup/mu_widgets_basic.h"
#include "../include/markup/mu_layout_flex.h"
#include "../include/markup/mu_style.h"
#include "../include/markup/mu_render.h"
#include "../include/markup/mu_input.h"
#include <stdlib.h>
#include <string.h>

#define MU_TEXTINPUT_CAP 256
#define MU_TEXTINPUT_PLACEHOLDER_LEN 64

typedef struct MuTextInState {
    char buf[MU_TEXTINPUT_CAP];
    int len;
    int cursor;
    float scroll_x;
    char placeholder[MU_TEXTINPUT_PLACEHOLDER_LEN];
} MuTextInState;

const MuNodeOps mu_textinput_node_ops;

static MuTextInState *ti_state(MuNode *node) {
    return node ? (MuTextInState *)node->state : NULL;
}

static void ti_clamp_cursor(MuTextInState *s) {
    if (!s) return;
    if (s->cursor < 0) s->cursor = 0;
    if (s->cursor > s->len) s->cursor = s->len;
}

static void ti_sync_scroll(MuContext *ctx, MuNode *node, MuRenderContext *rc, const MuTextStyle *text) {
    MuTextInState *s = ti_state(node);
    if (!s) return;

    char prefix[MU_TEXTINPUT_CAP];
    if (s->cursor > 0) {
        memcpy(prefix, s->buf, (size_t)s->cursor);
        prefix[s->cursor] = '\0';
    } else {
        prefix[0] = '\0';
    }

    MuTextMetrics caret_m = {0.f, 0.f};
    mu_text_measure(rc, prefix, text, &caret_m);
    float pad = 10.f;
    float inner_w = node->bounds.w - pad * 2.f;
    if (inner_w < 1.f) inner_w = 1.f;

    float caret_x = caret_m.width;
    if (caret_x - s->scroll_x > inner_w - 4.f) s->scroll_x = caret_x - inner_w + 4.f;
    if (caret_x - s->scroll_x < 0.f) s->scroll_x = caret_x;
    if (s->scroll_x < 0.f) s->scroll_x = 0.f;

    MuTextMetrics full = {0.f, 0.f};
    mu_text_measure(rc, s->buf, text, &full);
    float max_scroll = full.width - inner_w;
    if (max_scroll < 0.f) max_scroll = 0.f;
    if (s->scroll_x > max_scroll) s->scroll_x = max_scroll;
    (void)ctx;
}

static int ti_cursor_from_x(MuNode *node, MuRenderContext *rc, const MuTextStyle *text, float x) {
    MuTextInState *s = ti_state(node);
    if (!s) return 0;

    float pad = 10.f;
    float rel = x - (node->bounds.x + pad - s->scroll_x);
    if (rel <= 0.f) return 0;

    for (int i = 0; i <= s->len; i++) {
        char tmp[MU_TEXTINPUT_CAP];
        int n = i < s->len ? i + 1 : i;
        if (n > 0) {
            memcpy(tmp, s->buf, (size_t)n);
            tmp[n] = '\0';
        } else {
            tmp[0] = '\0';
        }
        MuTextMetrics tm = {0.f, 0.f};
        mu_text_measure(rc, tmp, text, &tm);
        if (i < s->len) {
            char tmp2[MU_TEXTINPUT_CAP];
            memcpy(tmp2, s->buf, (size_t)(i + 1));
            tmp2[i + 1] = '\0';
            MuTextMetrics tm2 = {0.f, 0.f};
            mu_text_measure(rc, tmp2, text, &tm2);
            float mid = (tm.width + tm2.width) * 0.5f;
            if (rel < mid) return i;
        } else if (rel <= tm.width) {
            return i;
        }
    }
    return s->len;
}

static void ti_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void ti_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    (void)ctx;
    (void)node;
    out->x = avail.x > 0.f ? avail.x : 240.f;
    out->y = 34.f;
}

static bool ti_ptr(MuContext *ctx, MuNode *node, const void *evp) {
    const MuPointerEvent *ev = (const MuPointerEvent *)evp;
    if (!ev->pressed) return true;
    mu_focus_set(ctx, node->id);
    MuTextInState *s = ti_state(node);
    if (!s) return true;
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    s->cursor = ti_cursor_from_x(node, NULL, &st.text, ev->position.x);
    ti_clamp_cursor(s);
    ti_sync_scroll(ctx, node, NULL, &st.text);
    return true;
}

static bool ti_delete_before(MuTextInState *s) {
    if (!s || s->cursor <= 0 || s->len <= 0) return false;
    memmove(&s->buf[s->cursor - 1], &s->buf[s->cursor], (size_t)(s->len - s->cursor + 1));
    s->len--;
    s->cursor--;
    return true;
}

static bool ti_delete_after(MuTextInState *s) {
    if (!s || s->cursor >= s->len) return false;
    memmove(&s->buf[s->cursor], &s->buf[s->cursor + 1], (size_t)(s->len - s->cursor));
    s->len--;
    return true;
}

static bool ti_key(MuContext *ctx, MuNode *node, const void *evp) {
    const MuKeyEvent *ev = (const MuKeyEvent *)evp;
    MuTextInState *s = ti_state(node);
    if (!s || !ev->pressed) return false;

    bool changed = false;
    switch (ev->key) {
    case MU_KEY_BACKSPACE:
        changed = ti_delete_before(s);
        break;
    case MU_KEY_DELETE:
        changed = ti_delete_after(s);
        break;
    case MU_KEY_LEFT:
        if (s->cursor > 0) {
            s->cursor--;
            changed = true;
        }
        break;
    case MU_KEY_RIGHT:
        if (s->cursor < s->len) {
            s->cursor++;
            changed = true;
        }
        break;
    case MU_KEY_HOME:
        if (s->cursor != 0) {
            s->cursor = 0;
            changed = true;
        }
        break;
    case MU_KEY_END:
        if (s->cursor != s->len) {
            s->cursor = s->len;
            changed = true;
        }
        break;
    default:
        return false;
    }

    if (changed) {
        MuStyleSnapshot st;
        mu_style_resolve(ctx, node, &st);
        ti_sync_scroll(ctx, node, NULL, &st.text);
        mu_layout_mark_dirty(node);
    }
    return changed;
}

static bool ti_char(MuContext *ctx, MuNode *node, unsigned int cp) {
    MuTextInState *s = ti_state(node);
    if (!s || cp < 32u || s->len >= MU_TEXTINPUT_CAP - 1) return false;
    if (cp >= 127u) return false;
    if (s->cursor < 0) s->cursor = 0;
    if (s->cursor > s->len) s->cursor = s->len;
    memmove(&s->buf[s->cursor + 1], &s->buf[s->cursor], (size_t)(s->len - s->cursor + 1));
    s->buf[s->cursor++] = (char)cp;
    s->len++;
    s->buf[s->len] = '\0';
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    ti_sync_scroll(ctx, node, NULL, &st.text);
    mu_layout_mark_dirty(node);
    return true;
}

static void ti_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuTextInState *s = ti_state(node);
    const char *text = s ? s->buf : "";
    bool focused = (node->flags & MU_NODE_FOCUSED) != 0;
    MuColor border = focused ? st.foreground : st.border;
    float border_w = focused ? 2.f : 1.f;
    mu_draw_rect(rc, node->bounds, st.background, border, border_w, 6.f);

    float pad = 10.f;
    MuRect clip = {node->bounds.x + pad, node->bounds.y, node->bounds.w - pad * 2.f, node->bounds.h};
    if (clip.w < 0.f) clip.w = 0.f;
    mu_push_scissor(rc, clip);

    float tx = node->bounds.x + pad - (s ? s->scroll_x : 0.f);
    float ty = node->bounds.y + (node->bounds.h - 16.f) * 0.5f;

    if (s && s->len > 0) {
        MuTextMetrics tm = {0.f, 0.f};
        mu_text_measure(rc, text, &st.text, &tm);
        ty = node->bounds.y + (node->bounds.h - tm.height) * 0.5f;
        mu_draw_text(rc, text, tx, ty, &st.text, st.foreground);
    } else {
        MuTextMetrics tm = {0.f, 0.f};
        const char *ph = (s && s->placeholder[0]) ? s->placeholder : "Type here…";
        mu_text_measure(rc, ph, &st.text, &tm);
        ty = node->bounds.y + (node->bounds.h - tm.height) * 0.5f;
        MuColor ph_c = ctx->style ? ctx->style->muted_fg : (MuColor){140, 150, 170, 255};
        mu_draw_text(rc, ph, tx, ty, &st.text, ph_c);
    }

    if (focused && s) {
        char prefix[MU_TEXTINPUT_CAP];
        if (s->cursor > 0) {
            memcpy(prefix, s->buf, (size_t)s->cursor);
            prefix[s->cursor] = '\0';
        } else {
            prefix[0] = '\0';
        }
        MuTextMetrics cm = {0.f, 0.f};
        mu_text_measure(rc, prefix, &st.text, &cm);
        float line_h = cm.height > 0.f ? cm.height : 16.f;
        if ((ctx->frame_index / 30) % 2 == 0) {
            MuRect caret = {tx + cm.width, ty, 2.f, line_h};
            mu_draw_rect(rc, caret, st.foreground, (MuColor){0, 0, 0, 0}, 0.f, 0.f);
        }
        ti_sync_scroll(ctx, node, rc, &st.text);
    }

    mu_pop_scissor(rc);
}

const MuNodeOps mu_textinput_node_ops = {
    .destroy_state = ti_destroy,
    .measure = ti_measure,
    .layout_children = NULL,
    .paint = ti_paint,
    .hit_test = NULL,
    .on_pointer = ti_ptr,
    .on_key = ti_key,
    .on_char = ti_char,
};

void mu_textinput_insert_utf8(MuContext *ctx, MuNode *input, const char *utf8) {
    MuTextInState *s = ti_state(input);
    if (!s || !utf8 || !utf8[0]) return;
    int slen = (int)strlen(utf8);
    if (slen <= 0 || s->len + slen >= MU_TEXTINPUT_CAP - 1) return;
    ti_clamp_cursor(s);
    memmove(&s->buf[s->cursor + slen], &s->buf[s->cursor], (size_t)(s->len - s->cursor + 1));
    memcpy(&s->buf[s->cursor], utf8, (size_t)slen);
    s->len += slen;
    s->cursor += slen;
    s->buf[s->len] = '\0';
    if (ctx && input) {
        MuStyleSnapshot st;
        mu_style_resolve(ctx, input, &st);
        ti_sync_scroll(ctx, input, NULL, &st.text);
    }
    mu_layout_mark_dirty(input);
}

const char *mu_textinput_get_text(const MuNode *input) {
    const MuTextInState *s = input ? (const MuTextInState *)input->state : NULL;
    return s ? s->buf : "";
}

void mu_textinput_set_text(MuNode *input, const char *text) {
    MuTextInState *s = ti_state(input);
    if (!s) return;
    const char *src = text ? text : "";
    strncpy(s->buf, src, MU_TEXTINPUT_CAP - 1);
    s->buf[MU_TEXTINPUT_CAP - 1] = '\0';
    s->len = (int)strlen(s->buf);
    s->cursor = s->len;
    s->scroll_x = 0.f;
    mu_layout_mark_dirty(input);
}

void mu_textinput_set_placeholder(MuNode *input, const char *placeholder) {
    MuTextInState *s = ti_state(input);
    if (!s) return;
    const char *src = placeholder ? placeholder : "";
    strncpy(s->placeholder, src, MU_TEXTINPUT_PLACEHOLDER_LEN - 1);
    s->placeholder[MU_TEXTINPUT_PLACEHOLDER_LEN - 1] = '\0';
    mu_layout_mark_dirty(input);
}

MuNode *mu_make_textinput(MuContext *ctx, const char *initial) {
    if (!ctx || !ctx->widgets_basic_kinds) return NULL;
    uint32_t kind = mu_kind_textinput(ctx);
    if (!kind) return NULL;

    MuTextInState *s = (MuTextInState *)calloc(1, sizeof(MuTextInState));
    if (!s) return NULL;
    strncpy(s->placeholder, "Type here…", sizeof(s->placeholder) - 1);
    if (initial) {
        strncpy(s->buf, initial, MU_TEXTINPUT_CAP - 1);
        s->len = (int)strlen(s->buf);
        s->cursor = s->len;
    }

    MuNode *n = mu_node_create(ctx, kind, s);
    if (!n) {
        free(s);
        return NULL;
    }
    n->role = "input";
    n->flags |= MU_NODE_FOCUSABLE;
    n->layout.flex_shrink = 0.f;
    return n;
}
