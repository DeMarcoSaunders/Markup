#include "../include/markup/mu_widgets_basic.h"
#include "../include/markup/mu_layout_flex.h"
#include "../include/markup/mu_style.h"
#include "../include/markup/mu_render.h"
#include "../include/markup/mu_input.h"
#include <stdlib.h>
#include <string.h>

/*
 * Single-line text input.
 *
 * State is a fixed MU_TEXTINPUT_CAP buffer holding UTF-8. The cursor is a BYTE offset,
 * but it is always kept on a character boundary: delete, arrow movement, click
 * positioning and insertion all step whole sequences via the utf8_* helpers below.
 * textinput_clamp_cursor snaps any offset that arrives mid-sequence, so an out-of-band
 * write to MuTextInState.cursor degrades to the nearest boundary instead of corrupting
 * the buffer.
 *
 * Both input paths agree on that: the SDL backend inserts pre-encoded text via
 * mu_textinput_insert_utf8, while backends that deliver codepoints (raylib) go through
 * textinput_char, which encodes to UTF-8 rather than dropping non-ASCII.
 *
 * Not handled: grapheme clusters. A combining mark or emoji ZWJ sequence is several
 * codepoints, so backspace deletes one codepoint of it at a time rather than the whole
 * cluster. Fixing that needs Unicode tables and belongs with real text shaping.
 *
 * scroll_x keeps the caret visible when text overflows the field: textinput_sync_scroll
 * is the single place that adjusts it, and every edit path must end there or the caret
 * drifts out of view.
 *
 * Hit-testing (textinput_cursor_from_x) and painting must agree on the text origin, or
 * clicks land on the wrong character. That shared origin is MU_TEXTINPUT_PAD_X — change
 * it in one place only.
 */

#define MU_TEXTINPUT_CAP 256
#define MU_TEXTINPUT_PLACEHOLDER_LEN 64
/* Horizontal inset of text from the field edge. Shared by hit-testing and painting. */
#define MU_TEXTINPUT_PAD_X 10.f

typedef struct MuTextInState {
    char buf[MU_TEXTINPUT_CAP];
    int len;
    int cursor;
    float scroll_x;
    char placeholder[MU_TEXTINPUT_PLACEHOLDER_LEN];
} MuTextInState;

const MuNodeOps mu_textinput_node_ops;

static MuTextInState *textinput_state(MuNode *node) {
    return node ? (MuTextInState *)node->state : NULL;
}

/* --- UTF-8 boundary helpers ------------------------------------------------------
 * The buffer holds UTF-8 while the cursor is a byte offset, so anything that moves or
 * deletes has to land on a character boundary. Continuation bytes are 10xxxxxx; every
 * other byte starts a character. buf[len] is the NUL terminator, which is a boundary. */

static bool utf8_is_continuation(char c) {
    return ((unsigned char)c & 0xC0u) == 0x80u;
}

/** Boundary at or before `i` — snaps a mid-sequence offset backwards. */
static int utf8_snap(const char *buf, int i) {
    if (i <= 0) return 0;
    while (i > 0 && utf8_is_continuation(buf[i])) i--;
    return i;
}

/** Start of the character before `i`. */
static int utf8_prev(const char *buf, int i) {
    if (i <= 0) return 0;
    i--;
    while (i > 0 && utf8_is_continuation(buf[i])) i--;
    return i;
}

/** Start of the character after the one at `i`, clamped to `len`. */
static int utf8_next(const char *buf, int len, int i) {
    if (i >= len) return len;
    i++;
    while (i < len && utf8_is_continuation(buf[i])) i++;
    return i;
}

static void textinput_clamp_cursor(MuTextInState *s) {
    if (!s) return;
    if (s->cursor < 0) s->cursor = 0;
    if (s->cursor > s->len) s->cursor = s->len;
    /* A click or an externally-set offset can land mid-sequence. */
    s->cursor = utf8_snap(s->buf, s->cursor);
}

static void textinput_sync_scroll(MuContext *ctx, MuNode *node, MuRenderContext *rc, const MuTextStyle *text) {
    MuTextInState *s = textinput_state(node);
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
    float pad = MU_TEXTINPUT_PAD_X;
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

static int textinput_cursor_from_x(MuNode *node, MuRenderContext *rc, const MuTextStyle *text, float x) {
    MuTextInState *s = textinput_state(node);
    if (!s) return 0;

    float pad = MU_TEXTINPUT_PAD_X;
    float rel = x - (node->bounds.x + pad - s->scroll_x);
    if (rel <= 0.f || s->len <= 0) return 0;

    /* Step character boundaries, measuring the prefix ending at each one, and return the
     * boundary nearest the click. Measuring prefixes (rather than summing per-character
     * widths) keeps this correct for proportional and kerned fonts. */
    char prefix[MU_TEXTINPUT_CAP];
    int prev = 0;
    float prev_w = 0.f;
    for (int i = utf8_next(s->buf, s->len, 0);; i = utf8_next(s->buf, s->len, i)) {
        memcpy(prefix, s->buf, (size_t)i);
        prefix[i] = '\0';
        MuTextMetrics tm = {0.f, 0.f};
        mu_text_measure(rc, prefix, text, &tm);

        if (rel < (prev_w + tm.width) * 0.5f) return prev;
        prev = i;
        prev_w = tm.width;
        if (i >= s->len) break;
    }
    return s->len;
}

static void textinput_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void textinput_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    (void)ctx;
    (void)node;
    out->x = avail.x > 0.f ? avail.x : 240.f;
    out->y = 34.f;
}

static bool textinput_ptr(MuContext *ctx, MuNode *node, const void *evp) {
    const MuPointerEvent *ev = (const MuPointerEvent *)evp;
    if (!ev->pressed) return true;
    mu_focus_set(ctx, node->id);
    MuTextInState *s = textinput_state(node);
    if (!s) return true;
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    s->cursor = textinput_cursor_from_x(node, NULL, &st.text, ev->position.x);
    textinput_clamp_cursor(s);
    textinput_sync_scroll(ctx, node, NULL, &st.text);
    return true;
}

static bool textinput_delete_before(MuTextInState *s) {
    if (!s || s->cursor <= 0 || s->len <= 0) return false;
    int start = utf8_prev(s->buf, s->cursor);
    int span = s->cursor - start;
    memmove(&s->buf[start], &s->buf[s->cursor], (size_t)(s->len - s->cursor + 1));
    s->len -= span;
    s->cursor = start;
    return true;
}

static bool textinput_delete_after(MuTextInState *s) {
    if (!s || s->cursor >= s->len) return false;
    int end = utf8_next(s->buf, s->len, s->cursor);
    int span = end - s->cursor;
    memmove(&s->buf[s->cursor], &s->buf[end], (size_t)(s->len - end + 1));
    s->len -= span;
    return true;
}

static bool textinput_key(MuContext *ctx, MuNode *node, const void *evp) {
    const MuKeyEvent *ev = (const MuKeyEvent *)evp;
    MuTextInState *s = textinput_state(node);
    if (!s || !ev->pressed) return false;

    bool changed = false;
    switch (ev->key) {
    case MU_KEY_BACKSPACE:
        changed = textinput_delete_before(s);
        break;
    case MU_KEY_DELETE:
        changed = textinput_delete_after(s);
        break;
    case MU_KEY_LEFT:
        if (s->cursor > 0) {
            s->cursor = utf8_prev(s->buf, s->cursor);
            changed = true;
        }
        break;
    case MU_KEY_RIGHT:
        if (s->cursor < s->len) {
            s->cursor = utf8_next(s->buf, s->len, s->cursor);
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
        textinput_sync_scroll(ctx, node, NULL, &st.text);
        mu_layout_mark_dirty(node);
    }
    return changed;
}

/** Encode `cp` into `out` (max 4 bytes). Returns the length, or 0 if not encodable. */
static int utf8_encode(unsigned int cp, char out[4]) {
    if (cp > 0x10FFFFu) return 0;
    if (cp >= 0xD800u && cp <= 0xDFFFu) return 0; /* lone surrogate */
    if (cp < 0x80u) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800u) {
        out[0] = (char)(0xC0u | (cp >> 6));
        out[1] = (char)(0x80u | (cp & 0x3Fu));
        return 2;
    }
    if (cp < 0x10000u) {
        out[0] = (char)(0xE0u | (cp >> 12));
        out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (cp & 0x3Fu));
        return 3;
    }
    out[0] = (char)(0xF0u | (cp >> 18));
    out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
    out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
    out[3] = (char)(0x80u | (cp & 0x3Fu));
    return 4;
}

static bool textinput_char(MuContext *ctx, MuNode *node, unsigned int cp) {
    MuTextInState *s = textinput_state(node);
    if (!s || cp < 32u || cp == 127u) return false;

    /* Encode rather than reject non-ASCII: the buffer is UTF-8, and the SDL backend
     * already inserts multi-byte text through mu_textinput_insert_utf8. Dropping it
     * here would make behaviour depend on which backend delivered the keystroke. */
    char enc[4];
    int n = utf8_encode(cp, enc);
    if (n == 0 || s->len + n >= MU_TEXTINPUT_CAP) return false;

    textinput_clamp_cursor(s);
    memmove(&s->buf[s->cursor + n], &s->buf[s->cursor], (size_t)(s->len - s->cursor + 1));
    memcpy(&s->buf[s->cursor], enc, (size_t)n);
    s->cursor += n;
    s->len += n;
    s->buf[s->len] = '\0';
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    textinput_sync_scroll(ctx, node, NULL, &st.text);
    mu_layout_mark_dirty(node);
    return true;
}

static void textinput_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    MuTextInState *s = textinput_state(node);
    const char *text = s ? s->buf : "";
    bool focused = (node->flags & MU_NODE_FOCUSED) != 0;
    MuColor border = focused ? st.foreground : st.border;
    float border_w = focused ? 2.f : 1.f;
    mu_draw_rect(rc, node->bounds, st.background, border, border_w, 6.f);

    float pad = MU_TEXTINPUT_PAD_X;
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
        textinput_sync_scroll(ctx, node, rc, &st.text);
    }

    mu_pop_scissor(rc);
}

const MuNodeOps mu_textinput_node_ops = {
    .destroy_state = textinput_destroy,
    .measure = textinput_measure,
    .layout_children = NULL,
    .paint = textinput_paint,
    .hit_test = NULL,
    .on_pointer = textinput_ptr,
    .on_key = textinput_key,
    .on_char = textinput_char,
};

void mu_textinput_insert_utf8(MuContext *ctx, MuNode *input, const char *utf8) {
    MuTextInState *s = textinput_state(input);
    if (!s || !utf8 || !utf8[0]) return;
    int slen = (int)strlen(utf8);
    if (slen <= 0 || s->len + slen >= MU_TEXTINPUT_CAP - 1) return;
    textinput_clamp_cursor(s);
    memmove(&s->buf[s->cursor + slen], &s->buf[s->cursor], (size_t)(s->len - s->cursor + 1));
    memcpy(&s->buf[s->cursor], utf8, (size_t)slen);
    s->len += slen;
    s->cursor += slen;
    s->buf[s->len] = '\0';
    if (ctx && input) {
        MuStyleSnapshot st;
        mu_style_resolve(ctx, input, &st);
        textinput_sync_scroll(ctx, input, NULL, &st.text);
    }
    mu_layout_mark_dirty(input);
}

const char *mu_textinput_get_text(const MuNode *input) {
    const MuTextInState *s = input ? (const MuTextInState *)input->state : NULL;
    return s ? s->buf : "";
}

void mu_textinput_set_text(MuNode *input, const char *text) {
    MuTextInState *s = textinput_state(input);
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
    MuTextInState *s = textinput_state(input);
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
