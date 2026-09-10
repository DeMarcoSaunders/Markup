#include "../include/markup/mu_physics_node.h"

#include "../include/markup/mu_input.h" /* MuPointerEvent only; no link dependency */

#include <stdlib.h>

/*
 * The join between the physics world and the node tree.
 *
 * Everything here is bookkeeping: mu_physics.c does the simulation and knows nothing
 * about nodes, and the node kinds paint themselves and know nothing about physics. This
 * file's whole job is to advance one and write the result into the other.
 */

typedef struct MuPhysicsNodeState {
    MuPhysWorld *world;
    float accum;      /* leftover real time not yet consumed by a fixed step */
    float fixed_dt;
    float max_advance;
    int last_steps;
    void (*on_click)(void *user, MuVec2 local);
    void *user;
} MuPhysicsNodeState;

static uint32_t physics_kind_id;

static MuPhysicsNodeState *state_of(MuNode *node) {
    if (!node || !physics_kind_id || node->kind != physics_kind_id) return NULL;
    return (MuPhysicsNodeState *)node->state;
}

static void physics_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    MuPhysicsNodeState *s = (MuPhysicsNodeState *)node->state;
    if (!s) return;
    mu_phys_destroy(s->world);
    free(s);
    node->state = NULL;
}

/* Only claims hits once a click callback is set, so a decorative world does not swallow
 * pointer events meant for whatever sits behind it. */
static bool physics_hit(MuContext *ctx, MuNode *node, MuVec2 pt) {
    (void)ctx;
    MuPhysicsNodeState *s = state_of(node);
    if (!s || !s->on_click) return false;
    return pt.x >= node->bounds.x && pt.x < node->bounds.x + node->bounds.w &&
           pt.y >= node->bounds.y && pt.y < node->bounds.y + node->bounds.h;
}

static bool physics_pointer(MuContext *ctx, MuNode *node, const void *evp) {
    (void)ctx;
    MuPhysicsNodeState *s = state_of(node);
    const MuPointerEvent *ev = (const MuPointerEvent *)evp;
    if (!s || !s->on_click || !ev) return false;
    if (ev->released) s->on_click(s->user, mu_physics_to_local(node, ev->position));
    return true;
}

/*
 * No layout_children op and no MU_NODE_FLEX_CONTAINER, which is what stops the layout
 * pass from touching the children — their bounds belong to the simulation. The world node
 * itself is still laid out normally by whatever contains it.
 */
static const MuNodeOps physics_ops = {
    .destroy_state = physics_destroy,
    .hit_test = physics_hit,
    .on_pointer = physics_pointer,
};

void mu_physics_register(MuContext *ctx) {
    if (!physics_kind_id) physics_kind_id = mu_register_node_kind(ctx, "physics_world", &physics_ops);
}

uint32_t mu_kind_physics_world(const MuContext *ctx) {
    (void)ctx;
    return physics_kind_id;
}

MuNode *mu_make_physics_world(MuContext *ctx, const MuPhysConfig *cfg) {
    if (!ctx || !physics_kind_id) return NULL;

    MuPhysConfig defaults;
    if (!cfg) {
        mu_phys_config_init(&defaults);
        cfg = &defaults;
    }

    MuPhysicsNodeState *s = (MuPhysicsNodeState *)calloc(1, sizeof(MuPhysicsNodeState));
    if (!s) return NULL;
    s->world = mu_phys_create(cfg);
    if (!s->world) {
        free(s);
        return NULL;
    }
    /* 120 Hz: stiff enough that ordinary stacks settle, cheap enough that a 60 Hz frame
     * costs two steps. The ceiling is a quarter second, so a stall costs 30 steps and not
     * however many the wall clock lost. */
    s->fixed_dt = 1.f / 120.f;
    s->max_advance = 0.25f;

    MuNode *node = mu_node_create(ctx, physics_kind_id, s);
    if (!node) {
        mu_phys_destroy(s->world);
        free(s);
        return NULL;
    }
    node->flags |= MU_NODE_CLIP_CHILDREN;
    return node;
}

MuPhysWorld *mu_physics_world(MuNode *world) {
    MuPhysicsNodeState *s = state_of(world);
    return s ? s->world : NULL;
}

bool mu_physics_attach(MuContext *ctx, MuNode *world, MuNode *child, MuBodyId body) {
    MuPhysicsNodeState *s = state_of(world);
    if (!ctx || !s || !child) return false;
    if (!mu_phys_valid(s->world, body)) return false;
    if (child->parent != world && !mu_node_add_child(ctx, world, child)) return false;
    mu_phys_set_user(s->world, body, child);
    return true;
}

void mu_physics_remove_body(MuNode *world, MuBodyId body) {
    MuPhysicsNodeState *s = state_of(world);
    if (!s) return;
    /* Clear the binding first: after mu_phys_remove the id is dead, and leaving a stale
     * node pointer in a slot that may be recycled is exactly the bug this avoids. */
    if (mu_phys_valid(s->world, body)) mu_phys_set_user(s->world, body, NULL);
    mu_phys_remove(s->world, body);
}

void mu_physics_set_timing(MuNode *world, float fixed_dt, float max_advance) {
    MuPhysicsNodeState *s = state_of(world);
    if (!s) return;
    if (fixed_dt > 0.f) s->fixed_dt = fixed_dt;
    if (max_advance > 0.f) s->max_advance = max_advance;
}

int mu_physics_last_steps(const MuNode *world) {
    MuPhysicsNodeState *s = state_of((MuNode *)world);
    return s ? s->last_steps : 0;
}

MuVec2 mu_physics_to_local(const MuNode *world, MuVec2 point) {
    if (!world) return point;
    return (MuVec2){point.x - world->bounds.x, point.y - world->bounds.y};
}

void mu_physics_set_on_click(MuNode *world, void (*cb)(void *user, MuVec2 local), void *user) {
    MuPhysicsNodeState *s = state_of(world);
    if (!s) return;
    s->on_click = cb;
    s->user = user;
}

/**
 * Write each attached body's bounds into its node.
 *
 * Assigning node->bounds directly rather than through mu_node_set_bounds is deliberate:
 * that would mark the subtree layout-dirty every frame, and there is no layout to run.
 * Damage tracking reads node->bounds against last frame's snapshot, so a moved body is
 * discovered without anything being marked.
 */
static void sync_bounds(MuNode *world, MuPhysicsNodeState *s) {
    MuBodyId id = MU_BODY_NONE;
    while ((id = mu_phys_next(s->world, id)) != MU_BODY_NONE) {
        MuNode *n = (MuNode *)mu_phys_get_user(s->world, id);
        if (!n) continue;
        MuRect a;
        if (!mu_phys_get_aabb(s->world, id, &a)) continue;
        n->bounds = (MuRect){world->bounds.x + a.x, world->bounds.y + a.y, a.w, a.h};
    }
}

void mu_physics_advance(MuNode *world, float elapsed) {
    MuPhysicsNodeState *s = state_of(world);
    if (!s) return;

    if (elapsed < 0.f) elapsed = 0.f;
    if (elapsed > s->max_advance) elapsed = s->max_advance;

    s->accum += elapsed;
    int steps = 0;
    while (s->accum >= s->fixed_dt) {
        mu_phys_step(s->world, s->fixed_dt);
        s->accum -= s->fixed_dt;
        steps++;
    }
    s->last_steps = steps;

    /* Sync even on a step-free call: a body may have been added or moved directly since
     * the last advance, and its node should not stay a frame behind. */
    sync_bounds(world, s);
}
