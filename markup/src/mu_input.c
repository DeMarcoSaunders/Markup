#include "../include/markup/mu_input.h"
#include <stdlib.h>
#include <string.h>

extern void mu_widgets_dispatch_wheel(MuContext *ctx, MuVec2 position, float delta_x, float delta_y);

typedef struct MuInputSeat {
    MuSeatWheelFn wheel;
    void *wheel_user;
    MuSeatKeyFn key;
    void *key_user;
    MuSeatTouchFn touch;
    void *touch_user;
    MuPresentedFn on_presented;
    void *on_presented_user;
    MuClipboardBridge clipboard;
    MuImeCallbacks ime;
    MuRect page_ime_caret;
    bool page_ime_caret_valid;
    MuMarkupImeCaretFn markup_caret;
    void *markup_caret_user;
    char clipboard_buf[4096];
} MuInputSeat;

static MuInputSeat *seat_of(MuContext *ctx) {
    return ctx ? (MuInputSeat *)ctx->input_seat : NULL;
}

void mu_input_context_init(MuContext *ctx) {
    if (!ctx || ctx->input_seat) return;
    MuInputSeat *s = (MuInputSeat *)calloc(1, sizeof(MuInputSeat));
    ctx->input_seat = s;
}

void mu_input_context_shutdown(MuContext *ctx) {
    if (!ctx) return;
    free(ctx->input_seat);
    ctx->input_seat = NULL;
}

static uint32_t key_modifiers(const MuKeyEvent *ev, MuContext *ctx) {
    if (ev && ev->modifiers) return ev->modifiers;
    return ctx ? ctx->seat_modifiers : 0;
}

static MuCursor cursor_for_node(const MuNode *n) {
    if (!n) return MU_CURSOR_DEFAULT;
    if (n->cursor != MU_CURSOR_DEFAULT) return (MuCursor)n->cursor;
    if (n->role && strcmp(n->role, "input") == 0) return MU_CURSOR_TEXT;
    if (n->role && (strcmp(n->role, "button") == 0 || strcmp(n->role, "tab") == 0)) return MU_CURSOR_POINTER;
    return MU_CURSOR_DEFAULT;
}

static MuCursor cursor_at(const MuContext *ctx, uint32_t hovered_id) {
    if (!ctx || !hovered_id) return MU_CURSOR_DEFAULT;
    for (MuNode *n = mu_context_find_id((MuContext *)ctx, NULL, hovered_id); n; n = n->parent) {
        MuCursor c = cursor_for_node(n);
        if (c != MU_CURSOR_DEFAULT) return c;
    }
    return MU_CURSOR_DEFAULT;
}

void mu_modal_push(MuContext *ctx, uint32_t modal_root_id) {
    if (!ctx) return;
    if (ctx->modal_count >= ctx->modal_cap) {
        int nc = ctx->modal_cap ? ctx->modal_cap * 2 : 4;
        uint32_t *ns = (uint32_t *)realloc(ctx->modal_stack, sizeof(uint32_t) * (size_t)nc);
        if (!ns) return;
        ctx->modal_stack = ns;
        ctx->modal_cap = nc;
    }
    ctx->modal_stack[ctx->modal_count++] = modal_root_id;
}

void mu_modal_pop(MuContext *ctx) {
    if (!ctx || ctx->modal_count <= 0) return;
    ctx->modal_count--;
}

uint32_t mu_modal_top(const MuContext *ctx) {
    if (!ctx || ctx->modal_count <= 0) return 0;
    return ctx->modal_stack[ctx->modal_count - 1];
}

static bool pt_in(MuVec2 p, MuRect r) {
    return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

static MuNode *hit_dfs(MuContext *ctx, MuNode *n, MuVec2 pt) {
    if (!n || !(n->flags & MU_NODE_VISIBLE) || (n->flags & MU_NODE_DISABLED)) return NULL;

    for (int i = n->child_count - 1; i >= 0; i--) {
        MuNode *h = hit_dfs(ctx, n->children[i], pt);
        if (h) return h;
    }

    if (n->flags & MU_NODE_HIT_TRANSPARENT) return NULL;

    const MuNodeOps *ops = mu_get_node_ops(ctx, n->kind);
    if (ops && ops->hit_test) {
        if (!ops->hit_test(ctx, n, pt)) return NULL;
    } else {
        if (!pt_in(pt, n->bounds)) return NULL;
    }
    return n;
}

static MuNode *hit_tree(MuContext *ctx, MuVec2 pt) {
    if (ctx->modal_layer && (ctx->modal_layer->flags & MU_NODE_VISIBLE))
        return hit_dfs(ctx, ctx->modal_layer, pt);
    uint32_t mid = mu_modal_top(ctx);
    if (mid) {
        MuNode *modal = mu_context_find_id(ctx, NULL, mid);
        if (modal) return hit_dfs(ctx, modal, pt);
    }
    if (ctx->popup_layer && (ctx->popup_layer->flags & MU_NODE_VISIBLE) && ctx->active_popup_id) {
        MuNode *h = hit_dfs(ctx, ctx->popup_layer, pt);
        if (h) return h;
    }
    if (!ctx->root) return NULL;
    return hit_dfs(ctx, ctx->root, pt);
}

static void update_focus_flags(MuContext *ctx, MuNode *n) {
    if (!n) return;
    if (n->id == ctx->focused_id && (n->flags & MU_NODE_FOCUSABLE)) n->flags |= MU_NODE_FOCUSED;
    else n->flags &= ~MU_NODE_FOCUSED;
    for (int i = 0; i < n->child_count; i++) update_focus_flags(ctx, n->children[i]);
}

void mu_input_clear_hovers(MuContext *ctx, MuNode *subtree) {
    MuNode *n = subtree ? subtree : ctx->root;
    if (!n) return;
    n->flags &= ~MU_NODE_HOVERED;
    for (int i = 0; i < n->child_count; i++) mu_input_clear_hovers(ctx, n->children[i]);
}

MuNode *mu_input_node_at(MuContext *ctx, MuVec2 position) {
    if (!ctx) return NULL;
    return hit_tree(ctx, position);
}

void mu_input_set_modifiers(MuContext *ctx, uint32_t mods) {
    if (ctx) ctx->seat_modifiers = mods;
}

uint32_t mu_input_modifiers(const MuContext *ctx) {
    return ctx ? ctx->seat_modifiers : 0;
}

MuCursor mu_input_cursor(const MuContext *ctx) {
    if (!ctx) return MU_CURSOR_DEFAULT;
    return (MuCursor)ctx->seat_cursor;
}

void mu_node_set_cursor(MuNode *node, MuCursor cursor) {
    if (!node) return;
    node->cursor = (uint8_t)cursor;
}

void mu_input_set_wheel_handler(MuContext *ctx, MuSeatWheelFn fn, void *user) {
    MuInputSeat *s = seat_of(ctx);
    if (!s) return;
    s->wheel = fn;
    s->wheel_user = user;
}

void mu_input_set_key_handler(MuContext *ctx, MuSeatKeyFn fn, void *user) {
    MuInputSeat *s = seat_of(ctx);
    if (!s) return;
    s->key = fn;
    s->key_user = user;
}

void mu_input_set_touch_handler(MuContext *ctx, MuSeatTouchFn fn, void *user) {
    MuInputSeat *s = seat_of(ctx);
    if (!s) return;
    s->touch = fn;
    s->touch_user = user;
}

void mu_input_set_clipboard(MuContext *ctx, const MuClipboardBridge *bridge) {
    MuInputSeat *s = seat_of(ctx);
    if (!ctx || !s) return;
    if (bridge)
        s->clipboard = *bridge;
    else
        memset(&s->clipboard, 0, sizeof(s->clipboard));
}

bool mu_clipboard_set_text(MuContext *ctx, const char *text) {
    MuInputSeat *s = seat_of(ctx);
    if (!ctx || !s) return false;
    if (s->clipboard.set_text) return s->clipboard.set_text(s->clipboard.user, text ? text : "");
    const char *src = text ? text : "";
    strncpy(s->clipboard_buf, src, sizeof(s->clipboard_buf) - 1);
    s->clipboard_buf[sizeof(s->clipboard_buf) - 1] = '\0';
    return true;
}

const char *mu_clipboard_get_text(MuContext *ctx) {
    MuInputSeat *s = seat_of(ctx);
    if (!ctx || !s) return "";
    if (s->clipboard.get_text) return s->clipboard.get_text(s->clipboard.user);
    return s->clipboard_buf;
}

void mu_input_set_ime(MuContext *ctx, const MuImeCallbacks *ime) {
    MuInputSeat *s = seat_of(ctx);
    if (!ctx || !s) return;
    if (ime)
        s->ime = *ime;
    else
        memset(&s->ime, 0, sizeof(s->ime));
}

void mu_input_set_ime_caret_rect(MuContext *ctx, MuRect rect) {
    MuInputSeat *s = seat_of(ctx);
    if (!ctx || !s) return;
    s->page_ime_caret = rect;
    s->page_ime_caret_valid = true;
}

void mu_input_reset_page_ime_caret(MuContext *ctx) {
    MuInputSeat *s = seat_of(ctx);
    if (!s) return;
    s->page_ime_caret_valid = false;
}

void mu_input_set_markup_ime_caret(MuContext *ctx, MuMarkupImeCaretFn fn, void *user) {
    MuInputSeat *s = seat_of(ctx);
    if (!ctx || !s) return;
    s->markup_caret = fn;
    s->markup_caret_user = user;
}

bool mu_input_get_ime_caret_rect(const MuContext *ctx, MuRect *out) {
    if (!out) return false;
    *out = (MuRect){0.f, 0.f, 0.f, 0.f};
    if (!ctx) return false;
    MuInputSeat *s = seat_of((MuContext *)ctx);
    if (s && s->page_ime_caret_valid) {
        *out = s->page_ime_caret;
        return out->w > 0.f && out->h > 0.f;
    }
    if (s && s->markup_caret) return s->markup_caret((MuContext *)ctx, out, s->markup_caret_user);
    return false;
}

void mu_input_update_hover(MuContext *ctx, MuVec2 mouse) {
    if (!ctx) return;
    mu_input_clear_hovers(ctx, ctx->root);
    MuNode *under = hit_tree(ctx, mouse);
    if (under) under->flags |= MU_NODE_HOVERED;
    ctx->hovered_id = under ? under->id : 0;
    ctx->seat_cursor = (uint8_t)cursor_at(ctx, ctx->hovered_id);
    update_focus_flags(ctx, ctx->root);
}

void mu_input_dispatch_pointer(MuContext *ctx, const MuPointerEvent *ev) {
    if (!ctx || !ev) return;
    static uint32_t pressed_target_id = 0;

    MuPointerEvent pe = *ev;
    if (!pe.modifiers) pe.modifiers = ctx->seat_modifiers;

    if (ctx->captured_pointer_id) {
        MuNode *cap = mu_context_find_id(ctx, NULL, ctx->captured_pointer_id);
        if (cap) {
            const MuNodeOps *ops = mu_get_node_ops(ctx, cap->kind);
            if (ops && ops->on_pointer) ops->on_pointer(ctx, cap, &pe);
        }
        if (pe.released) ctx->captured_pointer_id = 0;
        return;
    }

    MuNode *under = hit_tree(ctx, pe.position);
    if (pe.drag) return;

    if (pe.pressed) {
        pressed_target_id = under ? under->id : 0;
        if (under) {
            under->flags |= MU_NODE_PRESSED;
            if (under->flags & MU_NODE_FOCUSABLE) mu_focus_set(ctx, under->id);
            const MuNodeOps *ops = mu_get_node_ops(ctx, under->kind);
            if (ops && ops->on_pointer) ops->on_pointer(ctx, under, &pe);
        }
    } else if (pe.released) {
        MuNode *target = under;
        if (!target && pressed_target_id) target = mu_context_find_id(ctx, NULL, pressed_target_id);
        if (target) {
            target->flags &= ~MU_NODE_PRESSED;
            const MuNodeOps *ops = mu_get_node_ops(ctx, target->kind);
            if (ops && ops->on_pointer) ops->on_pointer(ctx, target, &pe);
        }
        pressed_target_id = 0;
    }
}

void mu_focus_set(MuContext *ctx, uint32_t node_id) {
    if (!ctx) return;
    ctx->focused_id = node_id;
    update_focus_flags(ctx, ctx->root);
}

typedef struct MuTabFocusCollect {
    MuNode **buf;
    int cap;
    int count;
} MuTabFocusCollect;

static void mu_tab_focus_collect(MuTabFocusCollect *ac, MuNode *x) {
    if (!x) return;
    if ((x->flags & MU_NODE_FOCUSABLE) && (x->flags & MU_NODE_VISIBLE) && !(x->flags & MU_NODE_DISABLED)) {
        if (ac->count >= ac->cap) {
            ac->cap = ac->cap ? ac->cap * 2 : 32;
            ac->buf = (MuNode **)realloc(ac->buf, sizeof(MuNode *) * (size_t)ac->cap);
        }
        ac->buf[ac->count++] = x;
    }
    for (int i = 0; i < x->child_count; i++) mu_tab_focus_collect(ac, x->children[i]);
}

void mu_focus_advance_tab(MuContext *ctx) {
    if (!ctx || !ctx->root) return;
    MuTabFocusCollect ac = {NULL, 0, 0};
    mu_tab_focus_collect(&ac, ctx->root);
    if (ac.count == 0) {
        free(ac.buf);
        return;
    }
    int cur = 0;
    for (int i = 0; i < ac.count; i++) {
        if (ac.buf[i]->id == ctx->focused_id) {
            cur = i;
            break;
        }
    }
    ctx->focused_id = ac.buf[(cur + 1) % ac.count]->id;
    free(ac.buf);
    update_focus_flags(ctx, ctx->root);
}

void mu_input_dispatch_key(MuContext *ctx, const MuKeyEvent *ev) {
    if (!ctx || !ev) return;
    if (!ev->pressed && !ev->released) return;

    MuKeyEvent ke = *ev;
    if (!ke.modifiers) ke.modifiers = key_modifiers(ev, ctx);

    MuInputSeat *s = seat_of(ctx);
    if (s && s->key && s->key(ctx, &ke, s->key_user)) return;

    MuNode *f = mu_context_find_id(ctx, NULL, ctx->focused_id);
    if (f) {
        const MuNodeOps *ops = mu_get_node_ops(ctx, f->kind);
        if (ops && ops->on_key) (void)ops->on_key(ctx, f, &ke);
    }
}

void mu_input_dispatch_char(MuContext *ctx, unsigned int codepoint) {
    if (!ctx || !codepoint) return;
    MuNode *f = mu_context_find_id(ctx, NULL, ctx->focused_id);
    if (!f) return;
    const MuNodeOps *ops = mu_get_node_ops(ctx, f->kind);
    if (ops && ops->on_char) ops->on_char(ctx, f, codepoint);
}

void mu_input_dispatch_wheel(MuContext *ctx, const MuWheelEvent *ev) {
    if (!ctx || !ev) return;
    if (ev->delta_x == 0.f && ev->delta_y == 0.f) return;

    MuWheelEvent we = *ev;
    if (!we.modifiers) we.modifiers = ctx->seat_modifiers;

    MuInputSeat *s = seat_of(ctx);
    if (s && s->wheel && s->wheel(ctx, &we, s->wheel_user)) return;

    mu_widgets_dispatch_wheel(ctx, we.position, we.delta_x, we.delta_y);
}

void mu_input_dispatch_touch(MuContext *ctx, const MuTouchEvent *ev) {
    if (!ctx || !ev) return;
    MuTouchEvent te = *ev;
    if (!te.modifiers) te.modifiers = ctx->seat_modifiers;
    MuInputSeat *s = seat_of(ctx);
    if (s && s->touch) s->touch(ctx, &te, s->touch_user);
}

void mu_context_set_on_presented(MuContext *ctx, MuPresentedFn fn, void *user) {
    MuInputSeat *s = seat_of(ctx);
    if (!s) return;
    s->on_presented = fn;
    s->on_presented_user = user;
}

void mu_frame_presented(MuContext *ctx) {
    if (!ctx) return;
    MuInputSeat *s = seat_of(ctx);
    if (s && s->on_presented) s->on_presented(ctx, s->on_presented_user, ctx->frame_index, ctx->frame_dt);
}