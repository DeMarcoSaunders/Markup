#ifndef MU_CORE_H
#define MU_CORE_H

#include "mu_layout.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mu_text.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MuContext MuContext;
typedef struct MuNode MuNode;

typedef struct MuVec2 {
    float x, y;
} MuVec2;

typedef struct MuRect {
    float x, y, w, h;
} MuRect;

typedef enum MuError {
    MU_OK = 0,
    MU_ERR_NOMEM,
    MU_ERR_INVALID,
    MU_ERR_OVERFLOW,
} MuError;

/* Opaque bump arena: reset per frame or on demand */
typedef struct MuArena MuArena;

MuArena *mu_arena_create(size_t capacity);
void mu_arena_destroy(MuArena *a);
void mu_arena_reset(MuArena *a);
void *mu_arena_alloc(MuArena *a, size_t size, size_t align);

typedef struct MuRenderContext MuRenderContext;
typedef struct MuStyleModule MuStyleModule;

typedef struct MuNodeOps {
    void (*destroy_state)(MuContext *ctx, MuNode *node);
    /* desired size for leaf widgets; container may ignore */
    void (*measure)(MuContext *ctx, MuNode *node, MuVec2 available, MuVec2 *out_desired);
    /* optional: custom layout; if NULL, layout module handles container nodes */
    void (*layout_children)(MuContext *ctx, MuNode *node);
    void (*paint)(MuContext *ctx, MuNode *node, MuRenderContext *render);
    /** Optional pass after children (e.g. scrollbars above clipped content). */
    void (*paint_overlay)(MuContext *ctx, MuNode *node, MuRenderContext *render);
    /* return true if node consumes hit */
    bool (*hit_test)(MuContext *ctx, MuNode *node, MuVec2 pt);
    bool (*on_pointer)(MuContext *ctx, MuNode *node, const void *event);
    bool (*on_key)(MuContext *ctx, MuNode *node, const void *event);
    bool (*on_char)(MuContext *ctx, MuNode *node, unsigned int codepoint);
} MuNodeOps;

struct MuNode {
    uint32_t id;
    uint32_t kind;
    MuNode *parent;
    MuNode **children;
    int child_count;
    int child_cap;
    void *state;
    MuRect bounds;
    uint32_t flags;
    const char *role;
    const char *class_name;
    MuLayoutStyle layout;
    MuTextStyle text;
    int z_index;
};

#define MU_NODE_VISIBLE (1u << 0)
#define MU_NODE_DISABLED (1u << 1)
#define MU_NODE_FLEX_CONTAINER (1u << 2)
#define MU_NODE_CLIP_CHILDREN (1u << 3)
#define MU_NODE_FOCUSABLE (1u << 4)
#define MU_NODE_FOCUSED (1u << 5)
#define MU_NODE_HOVERED (1u << 8)
#define MU_NODE_PRESSED (1u << 9)
#define MU_NODE_LAYOUT_DIRTY (1u << 10)
/** Pass pointer hits through to ancestors (for labels/icons inside clickable tiles). */
#define MU_NODE_HIT_TRANSPARENT (1u << 11)

struct MuContext {
    MuNode *root;
    uint32_t next_node_id;
    MuArena *arena;
    MuArena *scratch; /* per-frame bump */

    const MuNodeOps **kind_ops;
    const char **kind_names;
    int kind_count;
    int kind_cap;

    uint32_t focused_id;
    uint32_t hovered_id;
    uint32_t captured_pointer_id;
    uint32_t *modal_stack;
    int modal_count;
    int modal_cap;
    /* Optional full-screen overlay tree (not a flex child of root); paint after root */
    MuNode *modal_layer;
    /* Floating popups (menus); paint after root, before modal */
    MuNode *popup_layer;
    uint32_t active_popup_id;

    uint64_t frame_index;
    MuError last_error;
    char last_message[128];

    bool wants_keyboard_focus_scan;
    void *user_ptr;
    MuStyleModule *style;
    /* Owned by markup_widgets_basic: calloc'd in mu_widgets_basic_register, freed here */
    void *widgets_basic_kinds;
};

void mu_context_init(MuContext *ctx, size_t arena_capacity);
void mu_context_shutdown(MuContext *ctx);

uint32_t mu_register_node_kind(MuContext *ctx, const char *name, const MuNodeOps *ops);
const MuNodeOps *mu_get_node_ops(MuContext *ctx, uint32_t kind);

MuNode *mu_node_create(MuContext *ctx, uint32_t kind, void *state);
void mu_node_destroy_recursive(MuContext *ctx, MuNode *node);
bool mu_node_add_child(MuContext *ctx, MuNode *parent, MuNode *child);
void mu_node_remove_child(MuContext *ctx, MuNode *parent, MuNode *child);

void mu_context_set_root(MuContext *ctx, MuNode *root);
MuNode *mu_context_find_id(MuContext *ctx, MuNode *subtree, uint32_t id);

void mu_frame_begin(MuContext *ctx);
void mu_frame_end(MuContext *ctx);

void mu_error_set(MuContext *ctx, MuError code, const char *msg);

#ifdef __cplusplus
}
#endif

#endif
