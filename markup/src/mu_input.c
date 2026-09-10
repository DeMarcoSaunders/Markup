#include "../include/markup/mu_input.h"
#include <stdlib.h>
#include <string.h>

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

void mu_input_update_hover(MuContext *ctx, MuVec2 mouse) {
    if (!ctx) return;
    mu_input_clear_hovers(ctx, ctx->root);
    MuNode *under = hit_tree(ctx, mouse);
    if (under) under->flags |= MU_NODE_HOVERED;
    ctx->hovered_id = under ? under->id : 0;
    update_focus_flags(ctx, ctx->root);
}

void mu_input_dispatch_pointer(MuContext *ctx, const MuPointerEvent *ev) {
    if (!ctx || !ev) return;
    static uint32_t pressed_target_id = 0;

    if (ctx->captured_pointer_id) {
        MuNode *cap = mu_context_find_id(ctx, NULL, ctx->captured_pointer_id);
        if (cap) {
            const MuNodeOps *ops = mu_get_node_ops(ctx, cap->kind);
            if (ops && ops->on_pointer) ops->on_pointer(ctx, cap, ev);
        }
        if (ev->released) ctx->captured_pointer_id = 0;
        return;
    }

    MuNode *under = hit_tree(ctx, ev->position);
    if (ev->drag) return;

    if (ev->pressed) {
        pressed_target_id = under ? under->id : 0;
        if (under) {
            under->flags |= MU_NODE_PRESSED;
            if (under->flags & MU_NODE_FOCUSABLE) mu_focus_set(ctx, under->id);
            const MuNodeOps *ops = mu_get_node_ops(ctx, under->kind);
            if (ops && ops->on_pointer) ops->on_pointer(ctx, under, ev);
        }
    } else if (ev->released) {
        MuNode *target = under;
        if (!target && pressed_target_id) target = mu_context_find_id(ctx, NULL, pressed_target_id);
        if (target) {
            target->flags &= ~MU_NODE_PRESSED;
            const MuNodeOps *ops = mu_get_node_ops(ctx, target->kind);
            if (ops && ops->on_pointer) ops->on_pointer(ctx, target, ev);
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
    for (int i = 0; i < x->child_count; i++)
        mu_tab_focus_collect(ac, x->children[i]);
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
    if (!ctx || !ev || !ev->pressed) return;
    MuNode *f = mu_context_find_id(ctx, NULL, ctx->focused_id);
    if (f) {
        const MuNodeOps *ops = mu_get_node_ops(ctx, f->kind);
        if (ops && ops->on_key && ops->on_key(ctx, f, ev)) return;
    }
}

void mu_input_dispatch_char(MuContext *ctx, unsigned int codepoint) {
    if (!ctx || !codepoint) return;
    MuNode *f = mu_context_find_id(ctx, NULL, ctx->focused_id);
    if (!f) return;
    const MuNodeOps *ops = mu_get_node_ops(ctx, f->kind);
    if (ops && ops->on_char) ops->on_char(ctx, f, codepoint);
}
