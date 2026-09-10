/*
 * Rigid body physics tests.
 *
 * These assert physical behaviour, not just absence of crashes: free-fall matching the
 * analytic solution, a box coming to rest without sinking, stacks staying stacked,
 * momentum conserved in an elastic collision, friction holding a body on a slope.
 * Those are the properties that actually break when a solver is subtly wrong.
 */

#include "markup/mu_physics.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_failures;
static const char *g_case = "";

#define CASE(name) (g_case = (name))

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            printf("FAIL [%s] %s:%d: %s\n", g_case, __FILE__, __LINE__, #cond);                    \
            g_failures++;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_NEAR(got, want, tol)                                                                 \
    do {                                                                                           \
        double g_ = (double)(got), w_ = (double)(want), t_ = (double)(tol);                        \
        if (fabs(g_ - w_) > t_) {                                                                  \
            printf("FAIL [%s] %s:%d: %s = %.4f, expected %.4f +/- %.4f\n", g_case, __FILE__,       \
                   __LINE__, #got, g_, w_, t_);                                                    \
            g_failures++;                                                                          \
        }                                                                                          \
    } while (0)

#define DT (1.f / 60.f)

static MuPhysWorld *make_world(float gravity_y) {
    MuPhysConfig cfg;
    mu_phys_config_init(&cfg);
    cfg.gravity = (MuVec2){0.f, gravity_y};
    return mu_phys_create(&cfg);
}

static void run(MuPhysWorld *w, int steps) {
    for (int i = 0; i < steps; i++) mu_phys_step(w, DT);
}

static float body_y(MuPhysWorld *w, MuBodyId id) {
    MuVec2 p = {0.f, 0.f};
    mu_phys_get_transform(w, id, &p, NULL);
    return p.y;
}

/* ------------------------------------------------------------------ */

static void test_free_fall_matches_analytic(void) {
    CASE("free fall matches analytic");
    MuPhysWorld *w = make_world(1000.f);
    MuBodyId ball = mu_phys_add_circle(w, MU_BODY_DYNAMIC, (MuVec2){0.f, 0.f}, 10.f, 1.f);
    mu_phys_set_damping(w, ball, 0.f, 0.f);

    const int steps = 60;
    run(w, steps);

    /* Semi-implicit Euler lands on g*dt^2 * n(n+1)/2 exactly, which is a touch above the
     * continuous 1/2 g t^2. Checking the discrete form keeps this a real assertion. */
    float expected = 1000.f * DT * DT * (float)(steps * (steps + 1)) * 0.5f;
    CHECK_NEAR(body_y(w, ball), expected, 0.5f);

    MuVec2 v = {0.f, 0.f};
    mu_phys_get_velocity(w, ball, &v, NULL);
    CHECK_NEAR(v.y, 1000.f * DT * (float)steps, 0.5f);
    mu_phys_destroy(w);
}

static void test_static_bodies_never_move(void) {
    CASE("static bodies never move");
    MuPhysWorld *w = make_world(1000.f);
    MuBodyId ground = mu_phys_add_box(w, MU_BODY_STATIC, (MuVec2){0.f, 100.f}, 200.f, 10.f, 1.f);
    for (int i = 0; i < 5; i++)
        mu_phys_add_circle(w, MU_BODY_DYNAMIC, (MuVec2){(float)i * 8.f, 0.f}, 10.f, 1.f);

    run(w, 240);

    MuVec2 p = {0.f, 0.f};
    mu_phys_get_transform(w, ground, &p, NULL);
    CHECK_NEAR(p.x, 0.f, 1e-4f);
    CHECK_NEAR(p.y, 100.f, 1e-4f);
    mu_phys_destroy(w);
}

static void test_box_comes_to_rest_without_sinking(void) {
    CASE("box rests without sinking");
    MuPhysWorld *w = make_world(1000.f);
    mu_phys_add_box(w, MU_BODY_STATIC, (MuVec2){0.f, 300.f}, 300.f, 20.f, 1.f);
    MuBodyId box = mu_phys_add_box(w, MU_BODY_DYNAMIC, (MuVec2){0.f, 100.f}, 20.f, 20.f, 1.f);

    run(w, 300);

    /* Resting on top of the ground: centre should sit one half-height above the surface,
     * within the solver's allowed slop. Sinking through would show as a larger y. */
    float surface = 300.f - 20.f;
    CHECK_NEAR(body_y(w, box), surface - 20.f, 2.0f);

    MuVec2 v = {0.f, 0.f};
    float av = 0.f;
    mu_phys_get_velocity(w, box, &v, &av);
    CHECK(fabsf(v.y) < 2.f);
    CHECK(fabsf(v.x) < 2.f);
    CHECK(fabsf(av) < 0.05f);
    mu_phys_destroy(w);
}

static void test_stack_stays_stacked(void) {
    CASE("stack stays stacked");
    MuPhysWorld *w = make_world(1000.f);
    mu_phys_add_box(w, MU_BODY_STATIC, (MuVec2){0.f, 400.f}, 300.f, 20.f, 1.f);

    /* Three boxes, each resting on the one below, with a small gap to settle out. */
    MuBodyId boxes[3];
    for (int i = 0; i < 3; i++) {
        float y = 380.f - 20.f - (float)i * 42.f;
        boxes[i] = mu_phys_add_box(w, MU_BODY_DYNAMIC, (MuVec2){0.f, y}, 20.f, 20.f, 1.f);
        mu_phys_set_material(w, boxes[i], 0.f, 0.6f);
    }

    run(w, 600);

    for (int i = 0; i < 3; i++) {
        float expected = 380.f - 20.f - (float)i * 40.f;
        /* Warm starting is what keeps this from sinking; without it the stack creeps. */
        CHECK_NEAR(body_y(w, boxes[i]), expected, 4.0f);

        MuVec2 p = {0.f, 0.f};
        mu_phys_get_transform(w, boxes[i], &p, NULL);
        CHECK(fabsf(p.x) < 4.f); /* must not slide out sideways */

        MuVec2 v = {0.f, 0.f};
        mu_phys_get_velocity(w, boxes[i], &v, NULL);
        CHECK(fabsf(v.y) < 5.f);
        if (fabsf(v.y) >= 5.f) printf("       box %d still moving: vy=%.3f\n", i, v.y);
    }
    mu_phys_destroy(w);
}

/*
 * A tall stack at the default iteration count — the case that actually depends on warm
 * starting. The three-box test settles either way, so it cannot tell the two apart;
 * seven boxes can. Removing the impulse carry-over in mu_physics.c makes this test fail
 * while the shorter stack test still passes, which is what keeps it honest.
 */
static void test_tall_stack_needs_warm_starting(void) {
    CASE("tall stack holds at default iterations");
    MuPhysConfig cfg;
    mu_phys_config_init(&cfg);
    cfg.gravity = (MuVec2){0.f, 1000.f};
    MuPhysWorld *w = mu_phys_create(&cfg); /* default 8 velocity iterations */

    const float ground_top = 500.f;
    mu_phys_add_box(w, MU_BODY_STATIC, (MuVec2){0.f, ground_top + 20.f}, 300.f, 20.f, 1.f);

    enum { N = 7 };
    MuBodyId boxes[N];
    for (int i = 0; i < N; i++) {
        float y = ground_top - 15.f - (float)i * 31.f; /* 1px gaps, settle onto each other */
        boxes[i] = mu_phys_add_box(w, MU_BODY_DYNAMIC, (MuVec2){0.f, y}, 15.f, 15.f, 1.f);
        mu_phys_set_material(w, boxes[i], 0.f, 0.6f);
    }

    run(w, 900);

    /* Without warm starting the stack visibly sinks: each box has to rediscover its
     * support impulse from zero every step, and four iterations is not enough. */
    for (int i = 0; i < N; i++) {
        float expected = ground_top - 15.f - (float)i * 30.f;
        CHECK_NEAR(body_y(w, boxes[i]), expected, 4.0f);

        MuVec2 p = {0.f, 0.f};
        mu_phys_get_transform(w, boxes[i], &p, NULL);
        /* Some lateral creep is expected from a sequential solver; toppling is not. */
        CHECK(fabsf(p.x) < 20.f);
    }

    /* And it must be genuinely at rest, not slowly creeping. */
    MuVec2 top_v = {0.f, 0.f};
    mu_phys_get_velocity(w, boxes[N - 1], &top_v, NULL);
    CHECK(fabsf(top_v.y) < 3.f);
    mu_phys_destroy(w);
}

static void test_elastic_collision_swaps_velocity(void) {
    CASE("elastic collision swaps velocity");
    MuPhysWorld *w = make_world(0.f); /* no gravity: isolate the collision */

    MuBodyId a = mu_phys_add_circle(w, MU_BODY_DYNAMIC, (MuVec2){-100.f, 0.f}, 10.f, 1.f);
    MuBodyId b = mu_phys_add_circle(w, MU_BODY_DYNAMIC, (MuVec2){100.f, 0.f}, 10.f, 1.f);
    mu_phys_set_material(w, a, 1.f, 0.f);
    mu_phys_set_material(w, b, 1.f, 0.f);
    mu_phys_set_damping(w, a, 0.f, 0.f);
    mu_phys_set_damping(w, b, 0.f, 0.f);
    mu_phys_set_velocity(w, a, (MuVec2){200.f, 0.f}, 0.f);
    mu_phys_set_velocity(w, b, (MuVec2){-200.f, 0.f}, 0.f);

    run(w, 120);

    /* Equal masses, fully elastic, head on: velocities exchange. */
    MuVec2 va = {0.f, 0.f}, vb = {0.f, 0.f};
    mu_phys_get_velocity(w, a, &va, NULL);
    mu_phys_get_velocity(w, b, &vb, NULL);
    CHECK(va.x < -100.f);
    CHECK(vb.x > 100.f);

    /* Momentum is conserved regardless of how well restitution is modelled. */
    CHECK_NEAR(va.x + vb.x, 0.f, 1.f);
    mu_phys_destroy(w);
}

static void test_restitution_controls_bounce(void) {
    CASE("restitution controls bounce");
    MuPhysWorld *w = make_world(1000.f);
    mu_phys_add_box(w, MU_BODY_STATIC, (MuVec2){0.f, 400.f}, 300.f, 20.f, 1.f);

    MuBodyId bouncy = mu_phys_add_circle(w, MU_BODY_DYNAMIC, (MuVec2){-50.f, 100.f}, 10.f, 1.f);
    MuBodyId dead = mu_phys_add_circle(w, MU_BODY_DYNAMIC, (MuVec2){50.f, 100.f}, 10.f, 1.f);
    mu_phys_set_material(w, bouncy, 0.85f, 0.2f);
    mu_phys_set_material(w, dead, 0.f, 0.2f);
    mu_phys_set_damping(w, bouncy, 0.f, 0.f);
    mu_phys_set_damping(w, dead, 0.f, 0.f);

    /* Track the highest point each reaches after first touching down. */
    float best_bouncy = 1e9f, best_dead = 1e9f;
    for (int i = 0; i < 240; i++) {
        mu_phys_step(w, DT);
        if (i > 60) {
            float yb = body_y(w, bouncy), yd = body_y(w, dead);
            if (yb < best_bouncy) best_bouncy = yb;
            if (yd < best_dead) best_dead = yd;
        }
    }

    /* The elastic ball must rebound substantially higher than the inelastic one. */
    CHECK(best_bouncy < best_dead - 20.f);
    if (!(best_bouncy < best_dead - 20.f))
        printf("       bouncy peak y=%.1f, dead peak y=%.1f (lower is higher up)\n", best_bouncy,
               best_dead);
    mu_phys_destroy(w);
}

static void test_friction_holds_on_slope(void) {
    CASE("friction holds on slope");
    /* ~20 degrees needs mu > tan(20) = 0.36 to hold. Test both sides of that. */
    const float angle = 0.35f;

    for (int slippery = 0; slippery < 2; slippery++) {
        MuPhysWorld *w = make_world(1000.f);
        MuVec2 ramp_verts[4] = {{-300.f, -10.f}, {300.f, -10.f}, {300.f, 10.f}, {-300.f, 10.f}};
        MuBodyId ramp = mu_phys_add_polygon(w, MU_BODY_STATIC, (MuVec2){0.f, 400.f}, ramp_verts, 4, 1.f);
        mu_phys_set_transform(w, ramp, (MuVec2){0.f, 400.f}, angle);
        mu_phys_set_material(w, ramp, 0.f, slippery ? 0.02f : 1.2f);

        MuBodyId box = mu_phys_add_box(w, MU_BODY_DYNAMIC, (MuVec2){0.f, 360.f}, 15.f, 15.f, 1.f);
        mu_phys_set_material(w, box, 0.f, slippery ? 0.02f : 1.2f);

        run(w, 40); /* let it land */
        MuVec2 settled = {0.f, 0.f};
        mu_phys_get_transform(w, box, &settled, NULL);
        run(w, 240);
        MuVec2 after = {0.f, 0.f};
        mu_phys_get_transform(w, box, &after, NULL);

        float slid = fabsf(after.x - settled.x);
        if (slippery) {
            CHECK(slid > 20.f);
            if (slid <= 20.f) printf("       low friction: slid only %.1f\n", slid);
        } else {
            CHECK(slid < 12.f);
            if (slid >= 12.f) printf("       high friction: slid %.1f\n", slid);
        }
        mu_phys_destroy(w);
    }
}

static void test_deterministic(void) {
    CASE("deterministic");
    float ys[2][6];
    for (int run_i = 0; run_i < 2; run_i++) {
        MuPhysWorld *w = make_world(1000.f);
        mu_phys_add_box(w, MU_BODY_STATIC, (MuVec2){0.f, 400.f}, 300.f, 20.f, 1.f);
        MuBodyId ids[6];
        for (int i = 0; i < 6; i++) {
            ids[i] = mu_phys_add_circle(w, MU_BODY_DYNAMIC, (MuVec2){(float)i * 9.f - 20.f, (float)i * 30.f},
                                        10.f, 1.f);
            mu_phys_set_material(w, ids[i], 0.3f, 0.4f);
        }
        run(w, 300);
        for (int i = 0; i < 6; i++) ys[run_i][i] = body_y(w, ids[i]);
        mu_phys_destroy(w);
    }
    /* Identical inputs must give bit-identical outputs — no time, no rand, no pointers. */
    for (int i = 0; i < 6; i++) CHECK(ys[0][i] == ys[1][i]);
}

static void test_handles_reject_stale_ids(void) {
    CASE("handles reject stale ids");
    MuPhysWorld *w = make_world(1000.f);
    MuBodyId a = mu_phys_add_circle(w, MU_BODY_DYNAMIC, (MuVec2){0.f, 0.f}, 10.f, 1.f);
    CHECK(mu_phys_valid(w, a));
    CHECK(mu_phys_body_count(w) == 1);

    mu_phys_remove(w, a);
    CHECK(!mu_phys_valid(w, a));
    CHECK(mu_phys_body_count(w) == 0);

    /* The freed slot gets reused; the old id must not address the new occupant. */
    MuBodyId b = mu_phys_add_circle(w, MU_BODY_DYNAMIC, (MuVec2){50.f, 0.f}, 10.f, 1.f);
    CHECK(mu_phys_valid(w, b));
    CHECK(!mu_phys_valid(w, a));
    CHECK(a != b);

    /* Operations on a stale id must be inert, not crash or corrupt. */
    mu_phys_set_velocity(w, a, (MuVec2){999.f, 999.f}, 9.f);
    MuVec2 v = {0.f, 0.f};
    CHECK(mu_phys_get_velocity(w, b, &v, NULL));
    CHECK_NEAR(v.x, 0.f, 1e-6f);
    mu_phys_destroy(w);
}

static void test_no_tunneling_at_moderate_speed(void) {
    CASE("no tunneling at moderate speed");
    MuPhysWorld *w = make_world(0.f);
    mu_phys_add_box(w, MU_BODY_STATIC, (MuVec2){0.f, 200.f}, 300.f, 20.f, 1.f);

    MuBodyId ball = mu_phys_add_circle(w, MU_BODY_DYNAMIC, (MuVec2){0.f, 0.f}, 12.f, 1.f);
    mu_phys_set_velocity(w, ball, (MuVec2){0.f, 1500.f}, 0.f); /* 25 px/step */
    mu_phys_set_material(w, ball, 0.f, 0.3f);

    run(w, 120);
    /* Discrete stepping, so it must not pass through a 40px-thick wall at this speed. */
    CHECK(body_y(w, ball) < 200.f);
    if (body_y(w, ball) >= 200.f) printf("       tunneled to y=%.1f\n", body_y(w, ball));
    mu_phys_destroy(w);
}

static void test_iteration_and_counts(void) {
    CASE("iteration and counts");
    MuPhysWorld *w = make_world(1000.f);
    mu_phys_add_box(w, MU_BODY_STATIC, (MuVec2){0.f, 300.f}, 300.f, 20.f, 1.f);
    for (int i = 0; i < 4; i++)
        mu_phys_add_circle(w, MU_BODY_DYNAMIC, (MuVec2){(float)i * 25.f - 40.f, 100.f}, 10.f, 1.f);
    CHECK(mu_phys_body_count(w) == 5);

    int seen = 0;
    for (MuBodyId id = mu_phys_next(w, MU_BODY_NONE); id != MU_BODY_NONE; id = mu_phys_next(w, id)) {
        CHECK(mu_phys_valid(w, id));
        seen++;
    }
    CHECK(seen == 5);

    run(w, 200);
    CHECK(mu_phys_contact_count(w) > 0); /* they should be resting on the ground by now */
    mu_phys_destroy(w);
}

int main(void) {
    test_free_fall_matches_analytic();
    test_static_bodies_never_move();
    test_box_comes_to_rest_without_sinking();
    test_stack_stays_stacked();
    test_tall_stack_needs_warm_starting();
    test_elastic_collision_swaps_velocity();
    test_restitution_controls_bounce();
    test_friction_holds_on_slope();
    test_deterministic();
    test_handles_reject_stale_ids();
    test_no_tunneling_at_moderate_speed();
    test_iteration_and_counts();

    if (g_failures) {
        printf("\n%d check(s) failed\n", g_failures);
        return 1;
    }
    printf("all physics checks passed\n");
    return 0;
}
