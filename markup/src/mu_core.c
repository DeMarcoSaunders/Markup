#include "../include/markup/mu_core.h"
#include <stdlib.h>
#include <string.h>

static void destroy_subtree_states(MuContext *ctx, MuNode *node) {
    if (!node) return;
    const MuNodeOps *ops = mu_get_node_ops(ctx, node->kind);
    for (int i = 0; i < node->child_count; i++)
        destroy_subtree_states(ctx, node->children[i]);
    if (ops && ops->destroy_state) ops->destroy_state(ctx, node);
    else if (node->state) {
        free(node->state);
        node->state = NULL;
    }
}

static void free_subtree(MuContext *ctx, MuNode *node) {
    if (!node) return;
    for (int i = 0; i < node->child_count; i++) free_subtree(ctx, node->children[i]);
    free(node->children);
    free(node);
}

void mu_context_init(MuContext *ctx, size_t arena_capacity) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->arena = mu_arena_create(arena_capacity ? arena_capacity : 65536);
    ctx->scratch = mu_arena_create(32768);
    ctx->kind_cap = 64;
    ctx->kind_ops = (const MuNodeOps **)calloc((size_t)ctx->kind_cap, sizeof(*ctx->kind_ops));
    ctx->kind_names = (const char **)calloc((size_t)ctx->kind_cap, sizeof(*ctx->kind_names));
    ctx->next_node_id = 1;
    ctx->last_error = MU_OK;
}

void mu_context_shutdown(MuContext *ctx) {
    /* style module freed by mu_style_shutdown if linked */
    (void)ctx->style;
    if (ctx->modal_layer) {
        destroy_subtree_states(ctx, ctx->modal_layer);
        free_subtree(ctx, ctx->modal_layer);
        ctx->modal_layer = NULL;
    }
    if (ctx->root) {
        destroy_subtree_states(ctx, ctx->root);
        free_subtree(ctx, ctx->root);
        ctx->root = NULL;
    }
    free(ctx->modal_stack);
    ctx->modal_stack = NULL;
    free(ctx->widgets_basic_kinds);
    ctx->widgets_basic_kinds = NULL;
    mu_arena_destroy(ctx->arena);
    mu_arena_destroy(ctx->scratch);
    ctx->arena = ctx->scratch = NULL;
    free((void *)ctx->kind_ops);
    free((void *)ctx->kind_names);
    ctx->kind_ops = NULL;
    ctx->kind_names = NULL;
}

void mu_error_set(MuContext *ctx, MuError code, const char *msg) {
    ctx->last_error = code;
    if (msg) {
        strncpy(ctx->last_message, msg, sizeof(ctx->last_message) - 1);
        ctx->last_message[sizeof(ctx->last_message) - 1] = '\0';
    }
}

uint32_t mu_register_node_kind(MuContext *ctx, const char *name, const MuNodeOps *ops) {
    if (!ctx || !ops) return 0;
    if (ctx->kind_count >= ctx->kind_cap) {
        int nc = ctx->kind_cap * 2;
        const MuNodeOps **nop = (const MuNodeOps **)calloc((size_t)nc, sizeof(*nop));
        const char **nn = (const char **)calloc((size_t)nc, sizeof(*nn));
        if (!nop || !nn) {
            free((void *)nop);
            free((void *)nn);
            mu_error_set(ctx, MU_ERR_NOMEM, "kind registry grow");
            return 0;
        }
        memcpy((void *)nop, (const void *)ctx->kind_ops, (size_t)ctx->kind_cap * sizeof(*nop));
        memcpy((void *)nn, (const void *)ctx->kind_names, (size_t)ctx->kind_cap * sizeof(*nn));
        free((void *)ctx->kind_ops);
        free((void *)ctx->kind_names);
        ctx->kind_ops = nop;
        ctx->kind_names = nn;
        ctx->kind_cap = nc;
    }
    uint32_t k = (uint32_t)ctx->kind_count + 1u;
    ctx->kind_ops[ctx->kind_count] = ops;
    ctx->kind_names[ctx->kind_count] = name;
    ctx->kind_count++;
    return k;
}

const MuNodeOps *mu_get_node_ops(MuContext *ctx, uint32_t kind) {
    if (!ctx || kind == 0 || kind > (uint32_t)ctx->kind_count) return NULL;
    return ctx->kind_ops[kind - 1u];
}

MuNode *mu_node_create(MuContext *ctx, uint32_t kind, void *state) {
    MuNode *n = (MuNode *)calloc(1, sizeof(MuNode));
    if (!n) {
        mu_error_set(ctx, MU_ERR_NOMEM, "node");
        return NULL;
    }
    n->id = ctx->next_node_id++;
    n->kind = kind;
    n->state = state;
    n->flags = MU_NODE_VISIBLE;
    n->flex_grow = 0;
    n->flex_basis = -1.0f;
    return n;
}

void mu_node_destroy_recursive(MuContext *ctx, MuNode *node) {
    if (!node) return;
    destroy_subtree_states(ctx, node);
    free_subtree(ctx, node);
}

bool mu_node_add_child(MuContext *ctx, MuNode *parent, MuNode *child) {
    (void)ctx;
    if (!parent || !child) return false;
    if (parent->child_count >= parent->child_cap) {
        int nc = parent->child_cap ? parent->child_cap * 2 : 4;
        MuNode **ch = (MuNode **)realloc(parent->children, sizeof(*ch) * (size_t)nc);
        if (!ch) return false;
        parent->children = ch;
        parent->child_cap = nc;
    }
    child->parent = parent;
    parent->children[parent->child_count++] = child;
    return true;
}

void mu_node_remove_child(MuContext *ctx, MuNode *parent, MuNode *child) {
    (void)ctx;
    if (!parent || !child) return;
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            memmove(&parent->children[i], &parent->children[i + 1],
                    (size_t)(parent->child_count - i - 1) * sizeof(*parent->children));
            parent->child_count--;
            child->parent = NULL;
            return;
        }
    }
}

void mu_context_set_root(MuContext *ctx, MuNode *root) {
    ctx->root = root;
}

static MuNode *find_id(MuNode *node, uint32_t id) {
    if (!node) return NULL;
    if (node->id == id) return node;
    for (int i = 0; i < node->child_count; i++) {
        MuNode *f = find_id(node->children[i], id);
        if (f) return f;
    }
    return NULL;
}

MuNode *mu_context_find_id(MuContext *ctx, MuNode *subtree, uint32_t id) {
    MuNode *start = subtree ? subtree : ctx->root;
    return find_id(start, id);
}

void mu_frame_begin(MuContext *ctx) {
    if (ctx->scratch) mu_arena_reset(ctx->scratch);
    ctx->frame_index++;
}

void mu_frame_end(MuContext *ctx) { (void)ctx; }
