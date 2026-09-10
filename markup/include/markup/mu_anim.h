#ifndef MU_ANIM_H
#define MU_ANIM_H

#include "mu_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Spring animation.
 *
 * Almost all motion in a UI is one value converging on a target: a sheet sliding in, a
 * scroll rubber-banding at its limit, a list closing the gap after a row is removed. A
 * spring expresses that directly, and unlike a timed easing curve it can be re-aimed
 * mid-flight without a visible discontinuity — the value keeps its velocity and bends
 * toward the new target. That is what makes an interruptible interface feel attached to
 * the pointer rather than to a clock.
 *
 * Springs are solved analytically rather than integrated. A stepped integrator bleeds
 * energy at small steps and diverges at large ones, so it needs substepping and a clamp
 * on dt, and it still behaves differently at 60 Hz than at 144 Hz. The closed form has
 * neither problem: it is exact at any dt, so a frame that took a whole second lands
 * exactly where a second of motion should, and the result is identical on every display.
 *
 * Animations end. Once a spring is within a hair of its target it snaps there, zeroes its
 * velocity and reports itself finished — which is what lets damage tracking take the
 * frames back to idle instead of repainting a value still twitching in its fifteenth
 * decimal place.
 */

/* --- The spring itself. Usable on its own, for any float. ------------------- */

typedef struct MuSpring {
    float value;
    float target;
    float velocity;
    float omega; /* natural frequency in rad/s, derived from `response` */
    float zeta;  /* damping ratio */
    float eps_value;
    float eps_velocity;
} MuSpring;

/** Damping ratios. Below 1 overshoots and settles back; 1 is the fastest approach that
 *  does not overshoot at all; above 1 crawls in. */
#define MU_SPRING_STIFF 1.0f
#define MU_SPRING_SNAPPY 0.85f
#define MU_SPRING_BOUNCY 0.55f

/** A reasonable default for UI motion: quick, with the faintest overshoot. */
#define MU_SPRING_RESPONSE 0.32f

/**
 * `response` is roughly how long the motion takes, in seconds — the period the spring
 * would oscillate at if undamped. Larger is slower and looser.
 *
 * Rest thresholds default to values suited to pixel-space quantities. Animating something
 * on a different scale (an opacity in 0..1, an angle in radians) wants
 * mu_spring_set_rest_threshold, or it will appear to finish before it has moved.
 */
void mu_spring_init(MuSpring *s, float value, float response, float damping);

/** Re-aim without disturbing the motion: velocity is kept, so this is safe mid-flight. */
void mu_spring_set_target(MuSpring *s, float target);

/** Jump to `value` and stop dead. */
void mu_spring_snap(MuSpring *s, float value);

/** Change the shape of the motion, keeping the current value and velocity. */
void mu_spring_reshape(MuSpring *s, float response, float damping);

void mu_spring_set_rest_threshold(MuSpring *s, float value_eps, float velocity_eps);

/** Advance by `dt` seconds. Returns true while still moving, false once at rest. */
bool mu_spring_step(MuSpring *s, float dt);

bool mu_spring_at_rest(const MuSpring *s);

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
