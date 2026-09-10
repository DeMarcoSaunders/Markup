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
    MuLayoutStyle layout;
    MuTextStyle text;
    /* Backdrop blur radius in pixels; 0 disables. Lives on the node rather than in
     * widget state because damage collection has to know about it — a blurred node's
     * appearance depends on pixels it does not own. */
    float backdrop_blur;

    /* Damage tracking: last painted geometry and appearance-affecting flags. Compared
     * once per frame so moves, resizes and hover/press/focus changes are detected
     * automatically instead of relying on every mutation site remembering to mark. */
    MuRect prev_bounds;
    uint32_t prev_flags;
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
/** Appearance changed without moving — widget state a snapshot cannot see. */
#define MU_NODE_PAINT_DIRTY (1u << 12)
/**
 * Something has damaged the region this node's backdrop blur reads, so a cached blur is
 * no longer valid.
 *
 * Sticky on purpose. Unlike every other damage signal it is not cleared at the end of the
 * frame that raised it, but when the blur is actually recomputed — which may be many
 * frames later if the node is hidden or falls outside the damage rect in between. A
 * per-frame flag would go stale exactly there: a panel hidden while the content behind it
 * moves would come back believing its cached blur still matched.
 */
#define MU_NODE_BACKDROP_DIRTY (1u << 13)

/** Flags that change how a node paints. Any change here damages the node's bounds. */
#define MU_NODE_STYLE_FLAGS                                                                        \
    (MU_NODE_VISIBLE | MU_NODE_DISABLED | MU_NODE_FOCUSED | MU_NODE_HOVERED | MU_NODE_PRESSED)

/**
 * Blur nodes tracked for one frame so a cached blur can be reused.
 *
 * A backdrop blur only has to be recomputed when the pixels it reads change. Damage
 * raised by the node itself or by its own children paints *over* the blur, never under
 * it, so it cannot alter the input — which is what makes the common case (a button
 * hovering inside a glass panel) cacheable.
 */
#define MU_BACKDROP_TRACK_MAX 16

typedef struct MuBackdropTrack {
    MuNode *node;
    /* Damage from nodes outside this one's subtree: the only damage that can change what
     * the blur reads. Conservative — it also counts siblings painted on top, which are
     * harmless but cost a recompute. Per-frame scratch; the lasting verdict is recorded
     * on the node as MU_NODE_BACKDROP_DIRTY. */
    MuRect outside;
    bool outside_any;
} MuBackdropTrack;

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

    /* Union of everything needing repaint this frame. Valid after mu_damage_collect. */
    MuRect damage;
    bool damage_any;
    /* Set to force a full repaint every frame; the A/B reference for checking that no
     * invalidation has been missed. */
    bool damage_force_all;

    /* Backdrop-blur bookkeeping for this frame, filled by mu_damage_collect. More blurred
     * nodes than the array holds sets backdrop_overflow, which disables caching for the
     * frame rather than guessing at which ones to track. */
    MuBackdropTrack backdrop[MU_BACKDROP_TRACK_MAX];
    int backdrop_count;
    bool backdrop_overflow;

    uint64_t frame_index;
    MuError last_error;
    char last_message[128];

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

/*
 * Damage tracking.
 *
 * mu_paint_all repaints the whole tree clipped to a single union damage rect, rather
 * than tracking per-node regions. Repainting everything that intersects the rect, in
 * normal order, is what keeps transparency and overlap correct — a node cannot be
 * redrawn without whatever shows through beneath it being redrawn too.
 *
 * Most damage is discovered by mu_damage_collect comparing each node's bounds and
 * style flags against the previous frame, so moves, resizes, visibility changes and
 * hover/press/focus never need marking at the call site. Only widget-internal state
 * that changes appearance without changing either (a slider value, a checkbox toggle)
 * needs mu_node_mark_paint_dirty.
 */

/** Union `r` into this frame's damage. */
void mu_damage_add(MuContext *ctx, MuRect r);

/** Force a full repaint this frame (resize, theme change, first frame). */
void mu_damage_all(MuContext *ctx);

/** Mark a node as needing repaint in place. */
void mu_node_mark_paint_dirty(MuNode *node);

/**
 * Blur whatever is painted behind this node, within its own bounds.
 *
 * The node still paints its background on top, so a translucent background over the blur
 * is what produces frosted glass. Pass 0 to disable.
 *
 * Damage handling accounts for this: because the result depends on content the node does
 * not own, and because the blur kernel reaches outside the node, mu_damage_collect grows
 * the damage rect to cover the node plus its blur radius whenever it is touched at all.
 */
void mu_node_set_backdrop_blur(MuNode *node, float radius);

/** Walk the tree, accumulate damage, and re-snapshot. Called by mu_paint_all. */
void mu_damage_collect(MuContext *ctx);

/**
 * Whether `node`'s blurred backdrop still matches what its cached blur was built from.
 *
 * False once anything outside the node's own subtree has damaged the region its blur
 * kernel reads, and it stays false until the blur is recomputed. A node's own children
 * are excluded because they paint over the blur rather than into it, so hovering a button
 * inside a glass panel leaves the blur input untouched even though it damages pixels
 * inside the panel — which is the case the cache exists to serve.
 *
 * Call after mu_damage_collect. Backends use it to decide whether a cached blur may be
 * reused; it is deliberately conservative, and returns false whenever damage_force_all is
 * set so the full-repaint reference path never consults a cache.
 */
bool mu_node_backdrop_unchanged(const MuContext *ctx, const MuNode *node);

/** Clear MU_NODE_BACKDROP_DIRTY. For paint ops to call once the blur has been redrawn. */
void mu_node_backdrop_mark_clean(MuNode *node);

bool mu_damage_empty(const MuContext *ctx);
MuRect mu_damage_rect(const MuContext *ctx);

/** Drop accumulated damage. Call after presenting the frame. */
void mu_damage_reset(MuContext *ctx);

void mu_error_set(MuContext *ctx, MuError code, const char *msg);

#ifdef __cplusplus
}
#endif

#endif
