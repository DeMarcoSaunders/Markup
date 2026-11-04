#include "../include/markup/mu_spring.h"

#include <math.h>
#include <string.h>

/*
 * Springs, solved in closed form.
 *
 * A damped harmonic oscillator has an exact solution, and using it rather than an
 * integrator removes a whole class of problems: there is no step size at which this
 * diverges, no energy lost to small steps, and no difference between the motion at 60 Hz
 * and at 144 Hz. A frame that stalled for a second lands exactly where a second of motion
 * belongs instead of overshooting into nonsense.
 *
 * Three regimes, by damping ratio. The critically damped case is not just the boundary
 * between them, it is a genuinely different formula — the two roots coincide, so the
 * general solution collapses and picks up a factor of t.
 */

#define MU_SPRING_TAU 6.28318530718f
#define MU_SPRING_MIN_RESPONSE 0.001f
/* Wide enough that the near-critical formulas are not evaluated with a divisor close to
 * zero, narrow enough to be visually indistinguishable from true critical damping. */
#define MU_SPRING_CRIT_BAND 1e-4f

void mu_spring_init(MuSpring *s, float value, float response, float damping) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->value = value;
    s->target = value;
    s->velocity = 0.f;
    /* Defaults sized for pixels: a hundredth of one is far below anything visible. */
    s->eps_value = 0.01f;
    s->eps_velocity = 0.05f;
    mu_spring_reshape(s, response, damping);
}

void mu_spring_reshape(MuSpring *s, float response, float damping) {
    if (!s) return;
    if (!(response > MU_SPRING_MIN_RESPONSE)) response = MU_SPRING_MIN_RESPONSE;
    s->omega = MU_SPRING_TAU / response;
    s->zeta = damping > 0.f ? damping : MU_SPRING_STIFF;
}

void mu_spring_set_target(MuSpring *s, float target) {
    if (s) s->target = target;
}

void mu_spring_snap(MuSpring *s, float value) {
    if (!s) return;
    s->value = value;
    s->target = value;
    s->velocity = 0.f;
}

void mu_spring_set_rest_threshold(MuSpring *s, float value_eps, float velocity_eps) {
    if (!s) return;
    if (value_eps > 0.f) s->eps_value = value_eps;
    if (velocity_eps > 0.f) s->eps_velocity = velocity_eps;
}

bool mu_spring_at_rest(const MuSpring *s) {
    if (!s) return true;
    float d = s->value - s->target;
    if (d < 0.f) d = -d;
    float v = s->velocity < 0.f ? -s->velocity : s->velocity;
    /* Both conditions matter: a spring passing through its target at speed is not at
     * rest, and one creeping in from far away is not either. */
    return d <= s->eps_value && v <= s->eps_velocity;
}

bool mu_spring_step(MuSpring *s, float dt) {
    if (!s) return false;
    if (mu_spring_at_rest(s)) {
        s->value = s->target;
        s->velocity = 0.f;
        return false;
    }
    if (!(dt > 0.f)) return true;

    const float w = s->omega;
    const float z = s->zeta;
    const float d0 = s->value - s->target; /* displacement, which is what decays */
    const float v0 = s->velocity;
    float d, v;

    if (z < 1.f - MU_SPRING_CRIT_BAND) {
        /* Underdamped: decaying oscillation about the target. */
        const float wd = w * sqrtf(1.f - z * z);
        const float e = expf(-z * w * dt);
        const float cs = cosf(wd * dt), sn = sinf(wd * dt);
        const float A = d0;
        const float B = (v0 + z * w * d0) / wd;
        const float p = A * cs + B * sn;
        d = e * p;
        v = e * (wd * (B * cs - A * sn) - z * w * p);
    } else if (z > 1.f + MU_SPRING_CRIT_BAND) {
        /* Overdamped: two real roots, no oscillation, the slower one dominating. */
        const float r = w * sqrtf(z * z - 1.f);
        const float r1 = -z * w + r;
        const float r2 = -z * w - r;
        const float c2 = (v0 - r1 * d0) / (r2 - r1);
        const float c1 = d0 - c2;
        const float e1 = expf(r1 * dt), e2 = expf(r2 * dt);
        d = c1 * e1 + c2 * e2;
        v = c1 * r1 * e1 + c2 * r2 * e2;
    } else {
        /* Critically damped: the fastest approach that never crosses the target. */
        const float e = expf(-w * dt);
        const float c = v0 + w * d0;
        d = (d0 + c * dt) * e;
        v = (c - w * (d0 + c * dt)) * e;
    }

    s->value = s->target + d;
    s->velocity = v;

    if (mu_spring_at_rest(s)) {
        s->value = s->target;
        s->velocity = 0.f;
        return false;
    }
    return true;
}

bool mu_spring_follow(MuSpring *s, float target, float dt) {
    if (!s) return false;
    s->target = target;
    if (!(dt > 0.f)) {
        mu_spring_snap(s, target);
        return false;
    }
    return mu_spring_step(s, dt);
}
