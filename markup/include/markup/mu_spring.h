#ifndef MU_SPRING_H
#define MU_SPRING_H

#include <stdbool.h>

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
 *
 * This lives in markup_core, not markup_anim: it is pure arithmetic with no dependency
 * beyond math.h, so built-in widgets can spring their own state without every consumer
 * of markup_widgets_basic having to link the node animator.
 */

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

/**
 * Aim at `target` and advance by `dt` in one call; returns true while still moving.
 *
 * With no clock (`dt <= 0`) this snaps straight to the target instead of waiting. A
 * spring that is aimed but never stepped would otherwise report itself moving forever,
 * and a widget that animates by default must not keep an app with no frame clock
 * repainting endlessly — it should just show the end state, which is also what "reduce
 * motion" wants.
 */
bool mu_spring_follow(MuSpring *s, float target, float dt);

#ifdef __cplusplus
}
#endif

#endif
