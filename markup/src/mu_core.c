#include "../include/markup/mu_core.h"
#include "../include/markup/mu_text.h"
#include "../include/markup/mu_layout_flex.h"
#include "mu_input_internal.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef struct MuNodeSlot {
    MuNode *node;
    uint32_t generation, next;
} MuNodeSlot;

static bool register_node(MuContext *ctx, MuNode *node) {
    MuNodeSlot *slots = (MuNodeSlot *)ctx->node_slots;
    uint32_t index;
    if (ctx->node_free_slot) {
        index = ctx->node_free_slot - 1u;
        ctx->node_free_slot = slots[index].next;
    } else {
        if (ctx->node_slot_count == ctx->node_slot_cap) {
            uint32_t cap = ctx->node_slot_cap ? ctx->node_slot_cap * 2u : 64u;
            if (cap < ctx->node_slot_cap || (size_t)cap > SIZE_MAX / sizeof(*slots)) return false;
            MuNodeSlot *next = (MuNodeSlot *)realloc(slots, (size_t)cap * sizeof(*slots));
            if (!next) return false;
            memset(next + ctx->node_slot_cap, 0, (cap - ctx->node_slot_cap) * sizeof(*next));
            slots = next;
            ctx->node_slots = slots;
            ctx->node_slot_cap = cap;
        }
        index = ctx->node_slot_count++;
        slots[index].generation = 1;
    }
    slots[index].node = node;
    node->handle = (MuNodeHandle){ctx, index + 1u, slots[index].generation};
    return true;
}

MuNodeHandle mu_node_handle(const MuNode *node) {
    return node ? node->handle : (MuNodeHandle){0};
}

MuNode *mu_context_resolve_handle(MuContext *ctx, MuNodeHandle h) {
    if (!ctx || h.context != ctx || !h.slot || h.slot > ctx->node_slot_count) return NULL;
    MuNodeSlot *slot = &((MuNodeSlot *)ctx->node_slots)[h.slot - 1u];
    return slot->generation == h.generation ? slot->node : NULL;
}

static void unregister_node(MuContext *ctx, MuNode *node) {
    MuNodeSlot *slot = &((MuNodeSlot *)ctx->node_slots)[node->handle.slot - 1u];
    slot->node = NULL;
    /* Retire a slot on generation exhaustion, rather than reviving ancient handles. */
    if (slot->generation != UINT32_MAX) {
        slot->generation++;
        slot->next = ctx->node_free_slot;
        ctx->node_free_slot = node->handle.slot;
    }
    if (ctx->focused_id == node->id) ctx->focused_id = 0;
    if (ctx->hovered_id == node->id) ctx->hovered_id = 0;
    if (ctx->captured_pointer_id == node->id) ctx->captured_pointer_id = 0;
}

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
    unregister_node(ctx, node);
    if (!node->arena_owned) { free(node->children); free(node); }
}

void mu_context_init(MuContext *ctx, size_t arena_capacity) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->arena = mu_arena_create(arena_capacity ? arena_capacity : 65536);
    ctx->scratch = mu_arena_create(32768);
    ctx->kind_cap = 64;
    ctx->kind_ops = (const MuNodeOps **)calloc((size_t)ctx->kind_cap, sizeof(*ctx->kind_ops));
    ctx->kind_names = (const char **)calloc((size_t)ctx->kind_cap, sizeof(*ctx->kind_names));
    ctx->next_node_id = 1;
    ctx->layout_epoch = 1;
    ctx->last_error = MU_OK;
    mu_input_context_init(ctx);
    if (!ctx->arena || !ctx->scratch || !ctx->kind_ops || !ctx->kind_names || !ctx->input_seat)
        mu_error_set(ctx, MU_ERR_NOMEM, "context allocation");
}

void mu_context_shutdown(MuContext *ctx) {
    mu_input_context_shutdown(ctx);
    /* style module freed by mu_style_shutdown if linked */
    (void)ctx->style;
    mu_context_clear_nodes(ctx);
    free(ctx->node_slots);
    ctx->node_slots = NULL;
    ctx->node_slot_count = ctx->node_slot_cap = 0;
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
    if (!ctx) return;
    ctx->last_error = code;
    if (msg) {
        strncpy(ctx->last_message, msg, sizeof(ctx->last_message) - 1);
        ctx->last_message[sizeof(ctx->last_message) - 1] = '\0';
    }
}

uint32_t mu_register_node_kind(MuContext *ctx, const char *name, const MuNodeOps *ops) {
    if (!ctx || !ops) return 0;
    if (!ctx->kind_ops || !ctx->kind_names) {
        mu_error_set(ctx, MU_ERR_NOMEM, "kind registry unavailable"); return 0;
    }
    if (ctx->kind_count >= ctx->kind_cap) {
        if (ctx->kind_cap > INT_MAX / 2) { mu_error_set(ctx, MU_ERR_OVERFLOW, "kind registry capacity"); return 0; }
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
    if (!ctx || !kind || !mu_get_node_ops(ctx, kind) || !ctx->next_node_id) {
        mu_error_set(ctx, MU_ERR_INVALID, "invalid node kind or exhausted IDs");
        return NULL;
    }
    size_t mark = mu_arena_used(ctx->arena);
    MuNode *n = ctx->arena_nodes ? (MuNode *)mu_arena_alloc(ctx->arena, sizeof(MuNode), _Alignof(MuNode)) :
                                  (MuNode *)calloc(1, sizeof(MuNode));
    if (!n) {
        mu_error_set(ctx, MU_ERR_NOMEM, "node");
        return NULL;
    }
    if (!register_node(ctx, n)) {
        if (ctx->arena_nodes) mu_arena_rewind(ctx->arena, mark); else free(n);
        mu_error_set(ctx, MU_ERR_NOMEM, "node handle registry");
        return NULL;
    }
    n->context = ctx;
    n->arena_owned = ctx->arena_nodes;
    n->layout_revision = 1;
    n->id = ctx->next_node_id++;
    n->kind = kind;
    n->state = state;
    n->flags = MU_NODE_VISIBLE | MU_NODE_LAYOUT_DIRTY;
    mu_layout_init(&n->layout);
    mu_text_style_init(&n->text);
    return n;
}

static void damage_subtree(MuContext *ctx, MuNode *node) {
    if (!ctx || !node) return;
    /* Descendants can cast shadows beyond a non-clipping container. */
    if ((node->flags | node->prev_flags) & MU_NODE_VISIBLE) {
        mu_damage_add(ctx, node->bounds);
        mu_damage_add(ctx, node->prev_paint_bounds);
    }
    for (int i = 0; i < node->child_count; i++) damage_subtree(ctx, node->children[i]);
}

void mu_node_destroy_recursive(MuContext *ctx, MuNode *node) {
    if (!node) return;
    if (!ctx || node->context != ctx) return;
    if (node->parent) mu_node_remove_child(ctx, node->parent, node);
    if (ctx->root == node) ctx->root = NULL;
    if (ctx->popup_layer == node) ctx->popup_layer = NULL;
    if (ctx->modal_layer == node) ctx->modal_layer = NULL;
    damage_subtree(ctx, node);
    destroy_subtree_states(ctx, node);
    free_subtree(ctx, node);
}

bool mu_node_add_child(MuContext *ctx, MuNode *parent, MuNode *child) {
    if (!ctx || !parent || !child || parent->context != ctx || child->context != ctx || child->parent) {
        mu_error_set(ctx, MU_ERR_INVALID, "child already attached or foreign context");
        return false;
    }
    for (MuNode *n = parent; n; n = n->parent) if (n == child) {
        mu_error_set(ctx, MU_ERR_INVALID, "node cycle");
        return false;
    }
    if (parent->child_count >= parent->child_cap) {
        if (parent->child_cap > INT_MAX / 2) {
            mu_error_set(ctx, MU_ERR_OVERFLOW, "child capacity"); return false;
        }
        int nc = parent->child_cap ? parent->child_cap * 2 : 4;
        if ((size_t)nc > SIZE_MAX / sizeof(MuNode *)) {
            mu_error_set(ctx, MU_ERR_OVERFLOW, "child storage size"); return false;
        }
        MuNode **ch;
        if (parent->arena_owned) {
            ch = (MuNode **)mu_arena_alloc(ctx->arena, sizeof(*ch) * (size_t)nc, _Alignof(MuNode *));
            if (ch && parent->child_count) memcpy(ch, parent->children, parent->child_count * sizeof(*ch));
        } else ch = (MuNode **)realloc(parent->children, sizeof(*ch) * (size_t)nc);
        if (!ch) { mu_error_set(ctx, MU_ERR_NOMEM, "child storage"); return false; }
        parent->children = ch;
        parent->child_cap = nc;
    }
    child->parent = parent;
    parent->children[parent->child_count++] = child;
    mu_layout_mark_dirty(parent);
    return true;
}

void mu_node_remove_child(MuContext *ctx, MuNode *parent, MuNode *child) {
    if (!ctx || !parent || !child || parent->context != ctx || child->context != ctx || child->parent != parent) {
        mu_error_set(ctx, MU_ERR_INVALID, "node is not a child of this parent"); return;
    }
    damage_subtree(ctx, child);
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            memmove(&parent->children[i], &parent->children[i + 1],
                    (size_t)(parent->child_count - i - 1) * sizeof(*parent->children));
            parent->child_count--;
            child->parent = NULL;
            mu_layout_mark_dirty(parent);
            return;
        }
    }
}

void mu_context_set_root(MuContext *ctx, MuNode *root) {
    if (!ctx || (root && (root->context != ctx || root->parent))) {
        mu_error_set(ctx, MU_ERR_INVALID, "root must be detached and belong to this context"); return;
    }
    if (ctx->root != root) mu_damage_all(ctx);
    ctx->root = root;
    if (root) mu_layout_mark_dirty(root);
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
    if (subtree) return find_id(subtree, id);
    MuNode *f = find_id(ctx ? ctx->root : NULL, id);
    if (f) return f;
    if (ctx && ctx->popup_layer && (f = find_id(ctx->popup_layer, id))) return f;
    if (ctx && ctx->modal_layer && (f = find_id(ctx->modal_layer, id))) return f;
    return NULL;
}

void mu_frame_begin(MuContext *ctx) {
    if (ctx->scratch) mu_arena_reset(ctx->scratch);
    ctx->frame_index++;
    mu_input_reset_page_ime_caret(ctx);
}

void mu_frame_end(MuContext *ctx) { (void)ctx; }

void mu_frame_set_dt(MuContext *ctx, float seconds) {
    if (ctx) ctx->frame_dt = seconds > 0.f ? seconds : 0.f;
}

int mu_animating(const MuContext *ctx) {
    return ctx ? ctx->animating : 0;
}

bool mu_context_enable_node_arena(MuContext *ctx) {
    if (!ctx || !ctx->arena || ctx->node_slot_count) {
        mu_error_set(ctx, MU_ERR_INVALID, "enable node arena before creating nodes");
        return false;
    }
    ctx->arena_nodes = true;
    return true;
}

void mu_context_clear_nodes(MuContext *ctx) {
    if (!ctx) return;
    /* Slots also track detached nodes, so a failed build cannot leak them. */
    for (uint32_t i = 0; i < ctx->node_slot_count; i++) {
        MuNode *n = ((MuNodeSlot *)ctx->node_slots)[i].node;
        if (n && !n->parent) mu_node_destroy_recursive(ctx, n);
    }
    ctx->root = ctx->popup_layer = ctx->modal_layer = NULL;
    ctx->focused_id = ctx->hovered_id = ctx->captured_pointer_id = ctx->active_popup_id = 0;
    ctx->modal_count = 0;
    if (ctx->input_seat) ((MuInputSeat *)ctx->input_seat)->pressed_target_id = 0;
    mu_input_reset_page_ime_caret(ctx);
    if (ctx->arena_nodes) mu_arena_reset(ctx->arena);
    mu_damage_all(ctx);
}
