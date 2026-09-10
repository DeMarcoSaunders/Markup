#ifndef MU_PHYSICS_NODE_H
#define MU_PHYSICS_NODE_H

#include "mu_core.h"
#include "mu_physics.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Physics bound to the node tree.
 *
 * Deliberately separate from mu_physics.h, which knows nothing about nodes and can be
 * simulated, tested and debugged with no context and no renderer. This is the only file
 * that joins the two.
 *
 * The model: a `physics_world` node owns a MuPhysWorld, and each body may drive one node.
 * Advancing the world writes every attached body's axis-aligned bounds into its node, and
 * from there the ordinary machinery takes over — damage tracking discovers the moved
 * bounds by comparing against last frame, so a falling body repaints correctly without
 * anything being marked by hand.
 *
 * Nodes paint themselves. Physics only moves them, so a body can drive any node kind:
 * a panel, an image, a custom kind of your own.
 *
 * Bodies are positioned in coordinates local to the world node's rect, so laying the
 * container out somewhere else moves the whole simulation with it.
 *
 * Rotation is not applied. MuNode has axis-aligned bounds and no angle, so a body's node
 * covers its AABB — which is exact for circles and for boxes that stay upright, and
 * visibly wrong for a box at 45 degrees. Use circles, or a custom paint op that reads the
 * angle from mu_phys_get_transform and draws the real shape.
 */

/** Register the `physics_world` kind. Safe to call more than once. */
void mu_physics_register(MuContext *ctx);
uint32_t mu_kind_physics_world(const MuContext *ctx);

/**
 * A node owning a physics world. Pass NULL for `cfg` to take mu_phys_config_init defaults.
 *
 * The node clips its children: a body that escapes the container is not painted outside
 * it. Its bounds define the simulation's origin, not its extent — nothing stops a body
 * leaving, so give the world static bodies for walls.
 */
MuNode *mu_make_physics_world(MuContext *ctx, const MuPhysConfig *cfg);

/** The underlying world, for adding bodies and everything else in mu_physics.h. */
MuPhysWorld *mu_physics_world(MuNode *world);

/**
 * Bind `child` to `body` so the body's bounds drive the node's, adding `child` to the
 * world node if it is not already there.
 *
 * The binding holds a raw MuNode pointer. Destroying a node that still has a body
 * attached leaves the world writing into freed memory — call mu_physics_remove_body
 * first, or destroy the whole world node, which is always safe.
 */
bool mu_physics_attach(MuContext *ctx, MuNode *world, MuNode *child, MuBodyId body);

/** Remove `body` from the world. Its node, if any, stays in the tree, simply unbound. */
void mu_physics_remove_body(MuNode *world, MuBodyId body);

/**
 * Advance by `elapsed` real seconds and write the resulting bounds into attached nodes.
 *
 * Stepping happens at a fixed internal rate regardless of how ragged `elapsed` is. A
 * variable step changes the effective stiffness of the solver frame to frame, which makes
 * stacks visibly breathe and makes results irreproducible — so the leftover is carried to
 * the next call rather than stretching a step to fit.
 *
 * A very long `elapsed` (a breakpoint, a stalled frame) is clamped rather than simulated
 * in full: catching up on a lost second would cost more than the frame that lost it, and
 * running further behind each time is how a simulation spirals.
 */
void mu_physics_advance(MuNode *world, float elapsed);

/** Fixed step length in seconds, and the ceiling applied to one advance. */
void mu_physics_set_timing(MuNode *world, float fixed_dt, float max_advance);

/** Steps actually taken by the last mu_physics_advance. 0 means it only accumulated. */
int mu_physics_last_steps(const MuNode *world);

/** A point in the same space as node bounds, converted to world-local coordinates. */
MuVec2 mu_physics_to_local(const MuNode *world, MuVec2 point);

/**
 * Called on pointer release inside the container, with the position already converted to
 * world-local coordinates — which is what you need to place a body there.
 *
 * The node does not accept hits until a callback is set, so a decorative world stays out
 * of the way of whatever is behind it.
 */
void mu_physics_set_on_click(MuNode *world, void (*cb)(void *user, MuVec2 local), void *user);

#ifdef __cplusplus
}
#endif

#endif
