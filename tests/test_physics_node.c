/*
 * Physics bound to the node tree.
 *
 * The point of interest is not that bodies fall — test_physics covers the simulation —
 * but that falling bodies move their nodes, that damage tracking notices without anything
 * being marked by hand, and that the fixed-step accumulator behaves when frames are
 * ragged.
 */

#include "markup/mu_core.h"
#include "markup/mu_layout_flex.h"
#include "markup/mu_physics_node.h"
#include "markup/mu_soft.h"
#include "markup/mu_style.h"
#include "markup/mu_widgets_basic.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_failures;
static const char *g_case = "";

#define CASE(name) (g_case = (name))

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                              \
            printf("FAIL [%s] %s:%d: %s\n", g_case, __FILE__, __LINE__, #cond);                    \
            g_failures++;                                                                          \
        }                                                                                          \
    } while (0)

#define W 320
#define H 240

typedef struct Fixture {
    MuContext ctx;
    MuRenderContext rc;
    MuNode *root;
    MuNode *world;
} Fixture;

static void fixture_init(Fixture *f) {
    memset(f, 0, sizeof(*f));
    mu_context_init(&f->ctx, 1 << 16);
    mu_style_init(&f->ctx, NULL);
    mu_soft_render_init(&f->rc, W, H);
    mu_render_bind_measure(&f->rc);
    mu_widgets_basic_register(&f->ctx);
    mu_physics_register(&f->ctx);

    f->root = mu_make_panel(&f->ctx, true);
    f->root->role = "page";
    mu_node_set_bounds(f->root, (MuRect){0, 0, W, H});
    mu_context_set_root(&f->ctx, f->root);

    /*
     * The world goes on the modal layer, which mu_layout_run never walks, so its bounds
     * stay exactly where the test puts them. A panel is always a flex container — putting
     * the world under one would have layout reassign its position every frame, and the
     * tests would be asserting against flex rather than against physics.
     */
    MuNode *layer = mu_make_panel(&f->ctx, true);
    layer->role = "group";
    mu_modal_bind_layer(&f->ctx, layer);
    mu_node_set_bounds(layer, (MuRect){0, 0, W, H});

    f->world = mu_make_physics_world(&f->ctx, NULL);
    mu_node_add_child(&f->ctx, layer, f->world);
    mu_node_set_bounds(f->world, (MuRect){20.f, 20.f, 280.f, 200.f});
}

static void fixture_free(Fixture *f) {
    mu_context_shutdown(&f->ctx);
    mu_soft_render_shutdown(&f->rc);
    mu_style_shutdown(&f->ctx);
}

/** One frame. Returns whether anything was damaged. */
static bool frame(Fixture *f, float dt) {
    mu_frame_begin(&f->ctx);
    mu_layout_run(&f->ctx);
    mu_physics_advance(f->world, dt);
    mu_damage_collect(&f->ctx);
    bool painted = !mu_damage_empty(&f->ctx);
    if (painted) {
        MuRect area = mu_damage_rect(&f->ctx);
        mu_soft_begin_frame_rect(&f->rc, (MuColor){20, 20, 24, 255}, area);
        mu_paint_damaged(&f->ctx, &f->rc);
    }
    mu_damage_reset(&f->ctx);
    mu_frame_end(&f->ctx);
    return painted;
}

/* ------------------------------------------------------------------ */

static void test_body_drives_node_bounds(void) {
    CASE("a falling body moves its node");
    Fixture f;
    fixture_init(&f);

    MuPhysWorld *w = mu_physics_world(f.world);
    CHECK(w != NULL);

    MuNode *ball = mu_make_panel(&f.ctx, false);
    MuBodyId body = mu_phys_add_circle(w, MU_BODY_DYNAMIC, (MuVec2){100.f, 20.f}, 10.f, 1.f);
    CHECK(mu_physics_attach(&f.ctx, f.world, ball, body));

    /* Bounds are written on advance, so one frame settles them before we compare. */
    frame(&f, 1.f / 60.f);
    MuRect start = ball->bounds;

    /* Local coordinates: the body is at local x=100 inside a world node at x=20. */
    CHECK(fabsf(start.x - (20.f + 100.f - 10.f)) < 0.51f);
    CHECK(fabsf(start.w - 20.f) < 0.51f);

    for (int i = 0; i < 30; i++) frame(&f, 1.f / 60.f);

    CHECK(ball->bounds.y > start.y + 5.f);
    CHECK(fabsf(ball->bounds.x - start.x) < 0.51f); /* gravity is straight down */
    if (!(ball->bounds.y > start.y + 5.f))
        printf("       ball did not fall: y %.1f -> %.1f\n", start.y, ball->bounds.y);
    fixture_free(&f);
}

/**
 * Nothing marks the ball dirty, and its style flags never change. If damage did not
 * discover the moved bounds on its own, a falling body would leave a trail of stale
 * pixels — so an idle-looking frame that reports no damage is the failure.
 */
static void test_motion_damages_without_marking(void) {
    CASE("motion is discovered by damage tracking");
    Fixture f;
    fixture_init(&f);

    MuPhysWorld *w = mu_physics_world(f.world);
    MuNode *ball = mu_make_panel(&f.ctx, false);
    MuBodyId body = mu_phys_add_circle(w, MU_BODY_DYNAMIC, (MuVec2){100.f, 20.f}, 10.f, 1.f);
    mu_physics_attach(&f.ctx, f.world, ball, body);

    for (int i = 0; i < 3; i++) frame(&f, 1.f / 60.f); /* settle the snapshots */

    MuRect before = ball->bounds;
    bool painted = frame(&f, 1.f / 60.f);
    CHECK(painted);
    CHECK(ball->bounds.y != before.y);
    if (!painted) printf("       a moving body produced no damage\n");
    fixture_free(&f);
}

/** A body resting on a static floor should stop, and stop producing damage with it. */
static void test_resting_body_stops_damaging(void) {
    CASE("a settled body stops damaging");
    Fixture f;
    fixture_init(&f);

    MuPhysWorld *w = mu_physics_world(f.world);
    /* Floor across the bottom of the world's local space. */
    mu_phys_add_box(w, MU_BODY_STATIC, (MuVec2){140.f, 195.f}, 140.f, 5.f, 0.f);

    MuNode *ball = mu_make_panel(&f.ctx, false);
    MuBodyId body = mu_phys_add_circle(w, MU_BODY_DYNAMIC, (MuVec2){140.f, 40.f}, 10.f, 1.f);
    mu_phys_set_material(w, body, 0.f, 0.5f);
    mu_physics_attach(&f.ctx, f.world, ball, body);

    for (int i = 0; i < 400; i++) frame(&f, 1.f / 60.f);

    /* Resting on the floor: centre one radius above its top surface. */
    float expected_y = 20.f + (195.f - 5.f - 20.f);
    CHECK(fabsf(ball->bounds.y - expected_y) < 3.f);
    if (fabsf(ball->bounds.y - expected_y) >= 3.f)
        printf("       rested at y=%.1f, expected ~%.1f\n", ball->bounds.y, expected_y);

    /* Now that it has settled, frames should go quiet. */
    int damaged = 0;
    for (int i = 0; i < 10; i++)
        if (frame(&f, 1.f / 60.f)) damaged++;
    CHECK(damaged == 0);
    if (damaged) printf("       %d/10 settled frames still damaged\n", damaged);
    fixture_free(&f);
}

/**
 * The step is fixed regardless of how ragged the frame times are: a short frame banks its
 * time rather than running a short step, and a long one is clamped rather than trying to
 * catch up in full.
 */
static void test_fixed_step_accumulator(void) {
    CASE("stepping is fixed, not frame-shaped");
    Fixture f;
    fixture_init(&f);
    mu_physics_set_timing(f.world, 1.f / 100.f, 0.25f);

    /* Too short for a step: banked, not run. */
    mu_physics_advance(f.world, 0.004f);
    CHECK(mu_physics_last_steps(f.world) == 0);

    /* The banked 0.004 plus 0.007 crosses one step, leaving 0.001 over. */
    mu_physics_advance(f.world, 0.007f);
    CHECK(mu_physics_last_steps(f.world) == 1);

    /* A 50 ms frame is five steps at 100 Hz. */
    mu_physics_advance(f.world, 0.05f - 0.001f);
    CHECK(mu_physics_last_steps(f.world) == 5);

    /* A stall is clamped: 10 s would be 1000 steps, the ceiling allows 25. */
    mu_physics_advance(f.world, 10.f);
    int steps = mu_physics_last_steps(f.world);
    CHECK(steps <= 25);
    if (steps > 25) printf("       a 10s stall ran %d steps\n", steps);
    fixture_free(&f);
}

/** Moving the container moves the simulation with it; bodies are in local coordinates. */
static void test_world_bounds_offset_children(void) {
    CASE("bodies follow the container");
    Fixture f;
    fixture_init(&f);

    MuPhysWorld *w = mu_physics_world(f.world);
    MuNode *ball = mu_make_panel(&f.ctx, false);
    MuBodyId body = mu_phys_add_circle(w, MU_BODY_STATIC, (MuVec2){50.f, 60.f}, 8.f, 0.f);
    mu_physics_attach(&f.ctx, f.world, ball, body);

    frame(&f, 1.f / 60.f);
    CHECK(fabsf(ball->bounds.x - (20.f + 50.f - 8.f)) < 0.51f);

    mu_node_set_bounds(f.world, (MuRect){120.f, 20.f, 180.f, 200.f});
    frame(&f, 1.f / 60.f);
    CHECK(fabsf(ball->bounds.x - (120.f + 50.f - 8.f)) < 0.51f);
    if (fabsf(ball->bounds.x - (120.f + 50.f - 8.f)) >= 0.51f)
        printf("       ball at x=%.1f did not follow the container\n", ball->bounds.x);
    fixture_free(&f);
}

/** Removing a body must unbind its node, or the world writes into whatever it was. */
static void test_removed_body_stops_driving(void) {
    CASE("removing a body unbinds its node");
    Fixture f;
    fixture_init(&f);

    MuPhysWorld *w = mu_physics_world(f.world);
    MuNode *ball = mu_make_panel(&f.ctx, false);
    MuBodyId body = mu_phys_add_circle(w, MU_BODY_DYNAMIC, (MuVec2){100.f, 20.f}, 10.f, 1.f);
    mu_physics_attach(&f.ctx, f.world, ball, body);
    for (int i = 0; i < 5; i++) frame(&f, 1.f / 60.f);

    mu_physics_remove_body(f.world, body);
    CHECK(!mu_phys_valid(w, body));

    MuRect frozen = ball->bounds;
    for (int i = 0; i < 20; i++) frame(&f, 1.f / 60.f);
    CHECK(ball->bounds.y == frozen.y);
    if (ball->bounds.y != frozen.y) printf("       node kept moving after removal\n");
    fixture_free(&f);
}

int main(void) {
    test_body_drives_node_bounds();
    test_motion_damages_without_marking();
    test_resting_body_stops_damaging();
    test_fixed_step_accumulator();
    test_world_bounds_offset_children();
    test_removed_body_stops_driving();

    if (g_failures) {
        printf("\n%d check(s) failed\n", g_failures);
        return 1;
    }
    printf("all physics-node checks passed\n");
    return 0;
}
