#ifndef MU_ANIM_H
#define MU_ANIM_H

#include "mu_core.h"
#include "mu_spring.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MuSpring and the mu_spring_* functions live in mu_spring.h (markup_core). */

/* --- Springing node bounds ------------------------------------------------- */

/*
 * The node animator holds a spring per edge of a node's rect and writes the result into
 * node->bounds, after which the ordinary machinery takes over: damage tracking discovers
 * the moved bounds by comparing against last frame, so an animating node repaints without
 * anything being marked, and a finished animation lets the frames go quiet on their own.
 *
 * Entries are keyed by node id rather than by pointer, so a node destroyed mid-animation
 * simply stops resolving and its entry is dropped. There is no ordering requirement and
 * nothing to remember — unlike the physics binding, which does hold raw pointers.
 *
 * Bounds written here are overwritten by any layout pass that owns the node. Animate
 * nodes whose position is yours to set: children of a non-flex parent, or the popup and
 * modal layers, which mu_layout_run never walks.
 */

typedef struct MuAnimator MuAnimator;

MuAnimator *mu_anim_create(void);
void mu_anim_destroy(MuAnimator *a);

/**
 * Spring `node`'s bounds toward `target`.
 *
 * Calling this again while running re-aims the existing springs, keeping their velocity,
 * so a target that moves every frame is followed smoothly rather than restarted. The
 * springs hold the authoritative position once started; seeding happens only on the first
 * call, from the node's current bounds.
 */
bool mu_anim_to(MuAnimator *a, MuNode *node, MuRect target, float response, float damping);

/** Move there immediately and cancel any animation in progress. */
void mu_anim_set(MuAnimator *a, MuNode *node, MuRect bounds);

/** Stop animating `node`, leaving its bounds wherever they had reached. */
void mu_anim_cancel(MuAnimator *a, const MuNode *node);

bool mu_anim_active(const MuAnimator *a, const MuNode *node);

/** How many animations are live. Finished ones are dropped, so this returns to 0. */
int mu_anim_count(const MuAnimator *a);

/**
 * Advance every animation and write the results into node bounds. Returns the number
 * still moving, so a caller can tell whether it needs to keep asking for frames.
 *
 * Call after layout and before mu_damage_collect: layout must not overwrite what this
 * wrote, and the new bounds must be in place before damage compares them.
 */
int mu_anim_advance(MuContext *ctx, MuAnimator *a, float dt);

#ifdef __cplusplus
}
#endif

#endif
