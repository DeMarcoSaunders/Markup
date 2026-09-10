/*
 * Spring animation.
 *
 * Two properties carry the design and get the sharpest tests. The closed-form solution
 * must be stable at any dt — a stepped integrator would diverge where these ask it not
 * to. And animations must genuinely finish, snapping onto the target and dropping out,
 * because a spring that merely gets very close would keep damage tracking awake forever.
 */

#include "markup/mu_anim.h"
#include "markup/mu_core.h"
#include "markup/mu_layout_flex.h"
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
        if (!(cond)) {                                                                             \
            printf("FAIL [%s] %s:%d: %s\n", g_case, __FILE__, __LINE__, #cond);                    \
            g_failures++;                                                                          \
        }                                                                                          \
    } while (0)

/* ---------------- the spring on its own ---------------- */

static void test_converges_and_stops(void) {
    CASE("a spring reaches its target and reports done");
    MuSpring s;
    mu_spring_init(&s, 0.f, MU_SPRING_RESPONSE, MU_SPRING_STIFF);
    mu_spring_set_target(&s, 100.f);

    int steps = 0;
    while (mu_spring_step(&s, 1.f / 60.f) && steps < 1000) steps++;

    CHECK(steps < 1000);
    CHECK(s.value == 100.f);   /* snapped exactly, not merely close */
    CHECK(s.velocity == 0.f);
    CHECK(mu_spring_at_rest(&s));
    if (steps >= 1000) printf("       never settled\n");
    printf("       (settled in %d frames at 60Hz)\n", steps);
}

/**
 * The reason for solving analytically. A semi-implicit integrator at these step sizes
 * would ring up and diverge; the closed form simply lands where that much time puts it.
 */
static void test_stable_at_absurd_dt(void) {
    CASE("stable at any step size");
    const float dts[5] = {1.f / 240.f, 1.f / 60.f, 0.25f, 1.f, 10.f};

    for (int i = 0; i < 5; i++) {
        MuSpring s;
        mu_spring_init(&s, 0.f, MU_SPRING_RESPONSE, MU_SPRING_BOUNCY);
        mu_spring_set_target(&s, 100.f);

        for (int k = 0; k < 200; k++) {
            mu_spring_step(&s, dts[i]);
            /* Never diverge, never go non-finite, never leave a sane envelope. */
            if (!isfinite(s.value) || fabsf(s.value) > 1000.f) {
                printf("FAIL [%s] dt=%.4f blew up at step %d: value=%f\n", g_case, dts[i], k,
                       s.value);
                g_failures++;
                break;
            }
        }
        CHECK(fabsf(s.value - 100.f) < 0.5f);
    }
}

/** Same elapsed time by different routes should land in the same place. */
static void test_frame_rate_independent(void) {
    CASE("motion does not depend on frame rate");
    MuSpring a, b;
    mu_spring_init(&a, 0.f, 0.5f, MU_SPRING_SNAPPY);
    mu_spring_init(&b, 0.f, 0.5f, MU_SPRING_SNAPPY);
    mu_spring_set_target(&a, 200.f);
    mu_spring_set_target(&b, 200.f);

    for (int i = 0; i < 12; i++) mu_spring_step(&a, 1.f / 60.f);  /* 0.2s in 12 frames */
    for (int i = 0; i < 48; i++) mu_spring_step(&b, 1.f / 240.f); /* 0.2s in 48 frames */

    CHECK(fabsf(a.value - b.value) < 0.01f);
    if (fabsf(a.value - b.value) >= 0.01f)
        printf("       60Hz gave %.4f, 240Hz gave %.4f\n", a.value, b.value);
}

/** Damping ratio should mean what it says. */
static void test_damping_shapes_motion(void) {
    CASE("damping decides whether it overshoots");
    MuSpring stiff, bouncy;
    mu_spring_init(&stiff, 0.f, MU_SPRING_RESPONSE, MU_SPRING_STIFF);
    mu_spring_init(&bouncy, 0.f, MU_SPRING_RESPONSE, MU_SPRING_BOUNCY);
    mu_spring_set_target(&stiff, 100.f);
    mu_spring_set_target(&bouncy, 100.f);

    float stiff_max = 0.f, bouncy_max = 0.f;
    for (int i = 0; i < 400; i++) {
        mu_spring_step(&stiff, 1.f / 240.f);
        mu_spring_step(&bouncy, 1.f / 240.f);
        if (stiff.value > stiff_max) stiff_max = stiff.value;
        if (bouncy.value > bouncy_max) bouncy_max = bouncy.value;
    }

    CHECK(stiff_max <= 100.f + 0.01f); /* critical damping never crosses */
    CHECK(bouncy_max > 100.5f);        /* underdamped must overshoot */
    printf("       (peak: stiff %.2f, bouncy %.2f)\n", stiff_max, bouncy_max);
}

/** Re-aiming mid-flight keeps the motion continuous instead of restarting it. */
static void test_retarget_keeps_velocity(void) {
    CASE("re-aiming mid-flight preserves velocity");
    MuSpring s;
    mu_spring_init(&s, 0.f, MU_SPRING_RESPONSE, MU_SPRING_STIFF);
    mu_spring_set_target(&s, 100.f);
    for (int i = 0; i < 5; i++) mu_spring_step(&s, 1.f / 60.f);

    float v_before = s.velocity;
    CHECK(v_before > 1.f); /* genuinely moving, or the test proves nothing */

    mu_spring_set_target(&s, 200.f);
    CHECK(s.velocity == v_before); /* re-aiming alone must not disturb it */

    mu_spring_step(&s, 1.f / 60.f);
    CHECK(s.velocity > 0.f); /* still travelling the same way, not restarted */
}

static void test_overdamped_does_not_oscillate(void) {
    CASE("overdamped crawls in without crossing");
    MuSpring s;
    mu_spring_init(&s, 0.f, MU_SPRING_RESPONSE, 2.0f);
    mu_spring_set_target(&s, 100.f);
    for (int i = 0; i < 2000; i++) {
        mu_spring_step(&s, 1.f / 240.f);
        CHECK(s.value <= 100.f + 0.01f);
        if (s.value > 100.f + 0.01f) break;
    }
    CHECK(fabsf(s.value - 100.f) < 1.f);
}

/* ---------------- bound to nodes ---------------- */

#define W 320
#define H 240

typedef struct Fixture {
    MuContext ctx;
    MuRenderContext rc;
    MuNode *root;
    MuNode *layer;
    MuAnimator *anim;
} Fixture;

static void fixture_init(Fixture *f) {
    memset(f, 0, sizeof(*f));
    mu_context_init(&f->ctx, 1 << 16);
    mu_style_init(&f->ctx, NULL);
    mu_soft_render_init(&f->rc, W, H);
    mu_render_bind_measure(&f->rc);
    mu_widgets_basic_register(&f->ctx);
    f->anim = mu_anim_create();

    f->root = mu_make_panel(&f->ctx, true);
    f->root->role = "page";
    mu_node_set_bounds(f->root, (MuRect){0, 0, W, H});
    mu_context_set_root(&f->ctx, f->root);

    /* Animated nodes go on the modal layer: mu_layout_run never walks it, so flex cannot
     * overwrite the bounds the animator is writing. */
    f->layer = mu_make_panel(&f->ctx, true);
    f->layer->role = "group";
    mu_modal_bind_layer(&f->ctx, f->layer);
    mu_node_set_bounds(f->layer, (MuRect){0, 0, W, H});
}

static void fixture_free(Fixture *f) {
    mu_anim_destroy(f->anim);
    mu_context_shutdown(&f->ctx);
    mu_soft_render_shutdown(&f->rc);
    mu_style_shutdown(&f->ctx);
}

static bool frame(Fixture *f, float dt) {
    mu_frame_begin(&f->ctx);
    mu_layout_run(&f->ctx);
    mu_anim_advance(&f->ctx, f->anim, dt);
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

static MuNode *add_card(Fixture *f, MuRect at) {
    MuNode *n = mu_make_panel(&f->ctx, true);
    mu_node_add_child(&f->ctx, f->layer, n);
    n->bounds = at;
    return n;
}

/**
 * The whole point: an animating node repaints without anything marking it, and once the
 * animation lands the frames go quiet. A spring that never quite finished would keep the
 * UI repainting forever, which is the failure this guards.
 */
static void test_animation_damages_then_settles(void) {
    CASE("animation damages while moving, then goes quiet");
    Fixture f;
    fixture_init(&f);
    MuNode *card = add_card(&f, (MuRect){10.f, 10.f, 60.f, 40.f});
    for (int i = 0; i < 3; i++) frame(&f, 1.f / 60.f); /* settle snapshots */

    mu_anim_to(f.anim, card, (MuRect){200.f, 150.f, 60.f, 40.f}, MU_SPRING_RESPONSE,
               MU_SPRING_STIFF);

    int moving_frames = 0;
    for (int i = 0; i < 600 && mu_anim_count(f.anim) > 0; i++)
        if (frame(&f, 1.f / 60.f)) moving_frames++;

    CHECK(moving_frames > 5);           /* it actually animated over several frames */
    CHECK(mu_anim_count(f.anim) == 0);  /* and the entry was dropped when done */
    CHECK(fabsf(card->bounds.x - 200.f) < 0.001f);
    CHECK(fabsf(card->bounds.y - 150.f) < 0.001f);

    int quiet = 0;
    for (int i = 0; i < 10; i++)
        if (!frame(&f, 1.f / 60.f)) quiet++;
    CHECK(quiet == 10);
    if (quiet != 10) printf("       %d/10 frames still damaged after settling\n", 10 - quiet);
    fixture_free(&f);
}

/** Keyed by id, so a node destroyed mid-animation drops out rather than dangling. */
static void test_destroyed_node_drops_out(void) {
    CASE("destroying a node mid-animation is safe");
    Fixture f;
    fixture_init(&f);
    MuNode *card = add_card(&f, (MuRect){10.f, 10.f, 60.f, 40.f});
    frame(&f, 1.f / 60.f);

    mu_anim_to(f.anim, card, (MuRect){200.f, 150.f, 60.f, 40.f}, MU_SPRING_RESPONSE,
               MU_SPRING_STIFF);
    frame(&f, 1.f / 60.f);
    CHECK(mu_anim_count(f.anim) == 1);

    /* No cancel first. The animator must cope on its own. */
    mu_node_remove_child(&f.ctx, f.layer, card);
    mu_node_destroy_recursive(&f.ctx, card);

    for (int i = 0; i < 5; i++) frame(&f, 1.f / 60.f);
    CHECK(mu_anim_count(f.anim) == 0);
    fixture_free(&f);
}

/** Re-aiming a live animation should redirect it, not restart it from the old bounds. */
static void test_retarget_midflight(void) {
    CASE("a node can be re-aimed mid-animation");
    Fixture f;
    fixture_init(&f);
    MuNode *card = add_card(&f, (MuRect){0.f, 10.f, 60.f, 40.f});
    frame(&f, 1.f / 60.f);

    mu_anim_to(f.anim, card, (MuRect){200.f, 10.f, 60.f, 40.f}, MU_SPRING_RESPONSE,
               MU_SPRING_STIFF);
    for (int i = 0; i < 6; i++) frame(&f, 1.f / 60.f);

    float mid = card->bounds.x;
    CHECK(mid > 1.f && mid < 199.f); /* genuinely in flight */

    mu_anim_to(f.anim, card, (MuRect){100.f, 10.f, 60.f, 40.f}, MU_SPRING_RESPONSE,
               MU_SPRING_STIFF);
    CHECK(mu_anim_count(f.anim) == 1); /* re-aimed, not a second entry */

    for (int i = 0; i < 600 && mu_anim_count(f.anim) > 0; i++) frame(&f, 1.f / 60.f);
    CHECK(fabsf(card->bounds.x - 100.f) < 0.001f);
    fixture_free(&f);
}

static void test_cancel_and_set(void) {
    CASE("cancel leaves it put; set jumps");
    Fixture f;
    fixture_init(&f);
    MuNode *card = add_card(&f, (MuRect){0.f, 10.f, 60.f, 40.f});
    frame(&f, 1.f / 60.f);

    mu_anim_to(f.anim, card, (MuRect){200.f, 10.f, 60.f, 40.f}, MU_SPRING_RESPONSE,
               MU_SPRING_STIFF);
    for (int i = 0; i < 6; i++) frame(&f, 1.f / 60.f);

    float stopped_at = card->bounds.x;
    mu_anim_cancel(f.anim, card);
    CHECK(!mu_anim_active(f.anim, card));
    for (int i = 0; i < 5; i++) frame(&f, 1.f / 60.f);
    CHECK(card->bounds.x == stopped_at); /* stays where it was abandoned */

    mu_anim_set(f.anim, card, (MuRect){5.f, 5.f, 10.f, 10.f});
    CHECK(card->bounds.x == 5.f);
    CHECK(mu_anim_count(f.anim) == 0);
    fixture_free(&f);
}

/** Several at once, each finishing independently. */
static void test_many_animations(void) {
    CASE("many animations run and retire independently");
    Fixture f;
    fixture_init(&f);

    MuNode *cards[12];
    for (int i = 0; i < 12; i++) cards[i] = add_card(&f, (MuRect){0.f, (float)(i * 8), 20.f, 6.f});
    frame(&f, 1.f / 60.f);

    for (int i = 0; i < 12; i++)
        mu_anim_to(f.anim, cards[i], (MuRect){100.f + i, (float)(i * 8), 20.f, 6.f},
                   0.2f + (float)i * 0.05f, MU_SPRING_STIFF);
    CHECK(mu_anim_count(f.anim) == 12);

    for (int i = 0; i < 900 && mu_anim_count(f.anim) > 0; i++) frame(&f, 1.f / 60.f);
    CHECK(mu_anim_count(f.anim) == 0);
    for (int i = 0; i < 12; i++) CHECK(fabsf(cards[i]->bounds.x - (100.f + i)) < 0.001f);
    fixture_free(&f);
}

int main(void) {
    test_converges_and_stops();
    test_stable_at_absurd_dt();
    test_frame_rate_independent();
    test_damping_shapes_motion();
    test_retarget_keeps_velocity();
    test_overdamped_does_not_oscillate();

    test_animation_damages_then_settles();
    test_destroyed_node_drops_out();
    test_retarget_midflight();
    test_cancel_and_set();
    test_many_animations();

    if (g_failures) {
        printf("\n%d check(s) failed\n", g_failures);
        return 1;
    }
    printf("all animation checks passed\n");
    return 0;
}
