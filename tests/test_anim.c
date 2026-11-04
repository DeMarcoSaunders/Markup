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
#include "markup/mu_input.h"
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
        /* The region, not its bounding box -- clearing more than mu_paint_damaged
         * repaints would leave holes of flat clear colour. */
        mu_soft_begin_frame_region(&f->rc, (MuColor){20, 20, 24, 255}, mu_damage_region(&f->ctx));
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

/* ---------------- widget motion: paint_offset + the animate op ---------------- */

/*
 * No modal layer here: a visible one takes every hit test, and these tests are about
 * what the pointer hits. The button sits in the ordinary flex tree, which is the case
 * that matters — its position belongs to layout.
 */
typedef struct WFixture {
    MuContext ctx;
    MuRenderContext rc;
    MuNode *root;
} WFixture;

static void wfixture_init(WFixture *f, bool force_all) {
    memset(f, 0, sizeof(*f));
    mu_context_init(&f->ctx, 1 << 16);
    mu_style_init(&f->ctx, NULL);
    mu_soft_render_init(&f->rc, W, H);
    mu_render_bind_measure(&f->rc);
    mu_widgets_basic_register(&f->ctx);
    f->ctx.damage_force_all = force_all;

    f->root = mu_make_panel(&f->ctx, true);
    f->root->role = "page";
    mu_layout_set_padding_all(f->root, 20.f);
    mu_node_set_bounds(f->root, (MuRect){0, 0, W, H});
    mu_context_set_root(&f->ctx, f->root);
}

static void wfixture_free(WFixture *f) {
    mu_context_shutdown(&f->ctx);
    mu_soft_render_shutdown(&f->rc);
    mu_style_shutdown(&f->ctx);
}

/* The real frame order: layout, input, clock, collect (which steps motion), paint. */
static void wframe(WFixture *f, float dt, const MuVec2 *pointer) {
    mu_frame_begin(&f->ctx);
    mu_layout_run(&f->ctx);
    if (pointer) mu_input_update_hover(&f->ctx, *pointer);
    mu_frame_set_dt(&f->ctx, dt);
    mu_damage_collect(&f->ctx);
    if (!mu_damage_empty(&f->ctx)) {
        /* The region, not its bounding box -- clearing more than mu_paint_damaged
         * repaints would leave holes of flat clear colour. */
        mu_soft_begin_frame_region(&f->rc, (MuColor){20, 20, 24, 255}, mu_damage_region(&f->ctx));
        mu_paint_damaged(&f->ctx, &f->rc);
    }
    mu_damage_reset(&f->ctx);
    mu_frame_end(&f->ctx);
}

static bool surfaces_equal(MuRenderContext *a, MuRenderContext *b) {
    int aw, ah, ap, bw, bh, bp;
    const unsigned char *pa = (const unsigned char *)mu_present_pixel_data(a, &aw, &ah, &ap);
    const unsigned char *pb = (const unsigned char *)mu_present_pixel_data(b, &bw, &bh, &bp);
    if (!pa || !pb || aw != bw || ah != bh) return false;
    for (int y = 0; y < ah; y++)
        if (memcmp(pa + (size_t)y * ap, pb + (size_t)y * bp, (size_t)aw * 4u) != 0) return false;
    return true;
}

static bool rect_same(MuRect a, MuRect b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

/**
 * The bug a bounds-based lift has: with the pointer near the bottom edge, lifting the
 * bounds carries the button out from under it, hover drops, it falls back, and repeats.
 * A paint offset leaves bounds — and so hit testing — exactly where layout put them.
 */
static void test_hover_lift_keeps_hit_area(void) {
    CASE("hover lift is visual only: hit area and bounds never move");
    WFixture f;
    wfixture_init(&f, false);
    MuNode *btn = mu_make_button(&f.ctx, "Lift", NULL, NULL);
    mu_node_add_child(&f.ctx, f.root, btn);
    mu_button_set_hover_lift(&f.ctx, btn, 4.f);
    wframe(&f, 1.f / 60.f, NULL);

    MuRect laid_out = btn->bounds;
    /* 1px above the bottom edge: inside the button, but not inside it once lifted 4px. */
    MuVec2 pointer = {laid_out.x + laid_out.w * 0.5f, laid_out.y + laid_out.h - 1.f};

    int lost_hover = 0, bounds_moved = 0, frames = 0;
    for (; frames < 120; frames++) {
        wframe(&f, 1.f / 60.f, &pointer);
        if (!(btn->flags & MU_NODE_HOVERED)) lost_hover++;
        if (!rect_same(btn->bounds, laid_out)) bounds_moved++;
        if (frames > 2 && mu_animating(&f.ctx) == 0) break;
    }
    CHECK(lost_hover == 0);
    CHECK(bounds_moved == 0); /* also proves paint restored bounds bit-exactly */
    CHECK(btn->paint_offset.y == -4.f);
    CHECK(mu_animating(&f.ctx) == 0);
    if (lost_hover) printf("       hover dropped on %d of %d frames\n", lost_hover, frames);

    /* Pointer leaves: it springs back down and lands exactly. */
    MuVec2 away = {1.f, 1.f};
    for (int i = 0; i < 120; i++) {
        wframe(&f, 1.f / 60.f, &away);
        if (i > 2 && mu_animating(&f.ctx) == 0) break;
    }
    CHECK(btn->paint_offset.y == 0.f);
    wfixture_free(&f);
}

/**
 * Damage must follow where things are drawn, not where layout put them. Run the same
 * hover in and out under partial repaint and under a full repaint every frame; any frame
 * whose pixels differ means a lifted or falling button left stale pixels behind.
 */
static void test_hover_lift_damage_matches_full_repaint(void) {
    CASE("hover lift: partial repaint == full repaint, every frame");
    WFixture a, b;
    wfixture_init(&a, false);
    wfixture_init(&b, true);
    MuNode *ba = mu_make_button(&a.ctx, "Lift", NULL, NULL);
    MuNode *bb = mu_make_button(&b.ctx, "Lift", NULL, NULL);
    mu_node_add_child(&a.ctx, a.root, ba);
    mu_node_add_child(&b.ctx, b.root, bb);
    mu_button_set_hover_lift(&a.ctx, ba, 6.f);
    mu_button_set_hover_lift(&b.ctx, bb, 6.f);
    wframe(&a, 1.f / 60.f, NULL);
    wframe(&b, 1.f / 60.f, NULL);

    MuVec2 on = {ba->bounds.x + 10.f, ba->bounds.y + ba->bounds.h * 0.5f};
    MuVec2 off = {1.f, H - 1.f};
    int mismatched = 0, first_bad = -1;
    for (int i = 0; i < 80; i++) {
        const MuVec2 *p = i < 40 ? &on : &off;
        wframe(&a, 1.f / 60.f, p);
        wframe(&b, 1.f / 60.f, p);
        if (!surfaces_equal(&a.rc, &b.rc)) {
            if (first_bad < 0) first_bad = i;
            mismatched++;
        }
    }
    CHECK(mismatched == 0);
    if (mismatched) printf("       %d frames differ, first at frame %d\n", mismatched, first_bad);
    wfixture_free(&a);
    wfixture_free(&b);
}

/** No clock means no animation, not an animation that never ends. */
static void test_motion_without_clock_lands(void) {
    CASE("dt = 0: widget motion lands at once and nothing keeps animating");
    WFixture f;
    wfixture_init(&f, false);
    MuNode *btn = mu_make_button(&f.ctx, "Lift", NULL, NULL);
    mu_node_add_child(&f.ctx, f.root, btn);
    mu_button_set_hover_lift(&f.ctx, btn, 4.f);
    wframe(&f, 0.f, NULL);
    MuVec2 on = {btn->bounds.x + 10.f, btn->bounds.y + 5.f};
    wframe(&f, 0.f, &on);
    CHECK(btn->paint_offset.y == -4.f);
    CHECK(mu_animating(&f.ctx) == 0);
    wfixture_free(&f);
}

static MuNode *add_long_scroll(WFixture *f) {
    MuNode *scroll = mu_make_scroll(&f->ctx, MU_SCROLL_VERTICAL);
    mu_node_add_child(&f->ctx, f->root, scroll);
    MuNode *content = mu_scroll_content(scroll);
    for (int i = 0; i < 20; i++) {
        MuNode *row = mu_make_panel(&f->ctx, false);
        row->role = "tile";
        mu_layout_set_min_size(row, 0.f, 30.f);
        mu_node_add_child(&f->ctx, content, row);
    }
    mu_scroll_set_spring(scroll, true);
    return scroll;
}

static int settle_pair(WFixture *a, WFixture *b) {
    int i = 0;
    for (; i < 240; i++) {
        wframe(a, 1.f / 60.f, NULL);
        wframe(b, 1.f / 60.f, NULL);
        if (i > 2 && mu_animating(&a->ctx) == 0 && mu_animating(&b->ctx) == 0) break;
    }
    return i;
}

/**
 * At the bottom, pushed further: the content is drawn past the end and comes back, while
 * the logical offset never leaves its max. The overrun must *travel* — a push is speed,
 * so the stretch builds over several frames — rather than appearing whole in the first
 * frame, which is the jump the earlier displacement version had. Every frame must match
 * a full repaint.
 */
static void test_scroll_rubber_band(void) {
    CASE("rubber band: travels out and back, offset stays clamped, no stale pixels");
    WFixture a, b;
    wfixture_init(&a, false);
    wfixture_init(&b, true);
    MuNode *sa = add_long_scroll(&a);
    MuNode *sb = add_long_scroll(&b);
    wframe(&a, 1.f / 60.f, NULL);
    wframe(&b, 1.f / 60.f, NULL);

    /* Get to the bottom and let the glide there finish. */
    mu_scroll_by(&a.ctx, sa, 0.f, -9999.f);
    mu_scroll_by(&b.ctx, sb, 0.f, -9999.f);
    settle_pair(&a, &b);
    float max_y = 0.f;
    mu_scroll_get_offset(sa, NULL, &max_y);
    CHECK(max_y > 0.f); /* the content really is longer than the view */

    /* Already at the end: all of this push is refused by the clamp. */
    mu_scroll_by(&a.ctx, sa, 0.f, -96.f);
    mu_scroll_by(&b.ctx, sb, 0.f, -96.f);

    MuNode *content = mu_scroll_content(sa);
    float deepest = 0.f, prev = 0.f, biggest_step = 0.f;
    int deepest_frame = -1, offset_drift = 0, mismatched = 0, frames = 0;
    for (; frames < 240; frames++) {
        wframe(&a, 1.f / 60.f, NULL);
        wframe(&b, 1.f / 60.f, NULL);
        float y = 0.f;
        mu_scroll_get_offset(sa, NULL, &y);
        if (y != max_y) offset_drift++;
        float o = content->paint_offset.y;
        if (o < deepest) {
            deepest = o;
            deepest_frame = frames;
        }
        if (fabsf(o - prev) > biggest_step) biggest_step = fabsf(o - prev);
        prev = o;
        if (!surfaces_equal(&a.rc, &b.rc)) mismatched++;
        if (frames > 2 && mu_animating(&a.ctx) == 0) break;
    }
    CHECK(deepest < -10.f);                   /* it visibly ran past the end */
    CHECK(deepest_frame >= 2);                /* and got there over several frames */
    CHECK(biggest_step < -deepest * 0.75f);   /* no single frame carries most of it */
    CHECK(content->paint_offset.y == 0.f);    /* then landed exactly */
    CHECK(offset_drift == 0);
    CHECK(mismatched == 0);
    CHECK(mu_animating(&a.ctx) == 0);
    printf("       (ran %.1fpx past the end, deepest at frame %d, largest step %.1fpx, settled in %d)\n",
           -deepest, deepest_frame, biggest_step, frames);
    wfixture_free(&a);
    wfixture_free(&b);
}

/**
 * Inside the range a wheel step glides: the offset is the new value at once (that is
 * what hit testing uses), the content is drawn on its way there and lands exactly, never
 * overshooting the line — critically damped.
 */
static void test_scroll_glide(void) {
    CASE("spring scroll: an in-range step glides, lands exactly, never overshoots");
    WFixture a, b;
    wfixture_init(&a, false);
    wfixture_init(&b, true);
    MuNode *sa = add_long_scroll(&a);
    MuNode *sb = add_long_scroll(&b);
    settle_pair(&a, &b);

    mu_scroll_by(&a.ctx, sa, 0.f, -64.f);
    mu_scroll_by(&b.ctx, sb, 0.f, -64.f);
    float y = 0.f;
    mu_scroll_get_offset(sa, NULL, &y);
    CHECK(y == 64.f); /* the real offset moves at once */

    MuNode *content = mu_scroll_content(sa);
    int mismatched = 0, crossed = 0, frames = 0;
    float first = 0.f;
    for (; frames < 240; frames++) {
        wframe(&a, 1.f / 60.f, NULL);
        wframe(&b, 1.f / 60.f, NULL);
        if (frames == 0) first = content->paint_offset.y;
        if (content->paint_offset.y < -0.01f) crossed++;
        if (!surfaces_equal(&a.rc, &b.rc)) mismatched++;
        if (frames > 2 && mu_animating(&a.ctx) == 0) break;
    }
    CHECK(first > 20.f); /* drawn still near where it was, not jumped */
    CHECK(content->paint_offset.y == 0.f);
    CHECK(crossed == 0);
    CHECK(mismatched == 0);
    printf("       (first frame still %.1fpx short, landed in %d frames)\n", first, frames);
    wfixture_free(&a);
    wfixture_free(&b);
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

    test_hover_lift_keeps_hit_area();
    test_hover_lift_damage_matches_full_repaint();
    test_motion_without_clock_lands();
    test_scroll_rubber_band();
    test_scroll_glide();

    if (g_failures) {
        printf("\n%d check(s) failed\n", g_failures);
        return 1;
    }
    printf("all animation checks passed\n");
    return 0;
}
