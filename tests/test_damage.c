/*
 * Damage tracking tests.
 *
 * The important one is test_ab_matches_full_repaint: it renders the same interactions
 * twice, once with partial repaint and once forcing full repaint, and compares the
 * surfaces pixel for pixel. A missed invalidation shows up as a difference, which is
 * the failure mode that is otherwise miserable to find.
 */

#include "markup/mu_core.h"
#include "markup/mu_input.h"
#include "markup/mu_layout_flex.h"
#include "markup/mu_soft.h"
#include "markup/mu_style.h"
#include "markup/mu_widgets_basic.h"

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

#define W 300
#define H 200

typedef struct Fixture {
    MuContext ctx;
    MuRenderContext rc;
    MuNode *root;
    MuNode *button;
    MuNode *check;
    MuNode *slider;
} Fixture;

static void fixture_init(Fixture *f) {
    memset(f, 0, sizeof(*f));
    mu_context_init(&f->ctx, 1 << 16);
    mu_style_init(&f->ctx, NULL);
    mu_soft_render_init(&f->rc, W, H);
    mu_render_bind_measure(&f->rc);
    mu_widgets_basic_register(&f->ctx);

    f->root = mu_make_panel(&f->ctx, true);
    f->root->role = "page";
    mu_node_set_bounds(f->root, (MuRect){0, 0, W, H});
    mu_context_set_root(&f->ctx, f->root);

    f->button = mu_make_button(&f->ctx, "Go", NULL, NULL);
    f->check = mu_make_checkbox(&f->ctx, false, NULL, NULL);
    f->slider = mu_make_slider(&f->ctx, 0.f, 100.f, 20.f);
    mu_node_add_child(&f->ctx, f->root, f->button);
    mu_node_add_child(&f->ctx, f->root, f->check);
    mu_node_add_child(&f->ctx, f->root, f->slider);
    mu_layout_run(&f->ctx);
}

static void fixture_free(Fixture *f) {
    mu_context_shutdown(&f->ctx);
    mu_soft_render_shutdown(&f->rc);
    mu_style_shutdown(&f->ctx);
}

static const MuColor BG = {30, 30, 36, 255};

/** One frame: collect, clear+paint the damage, reset. Returns the damage rect used. */
static MuRect frame(Fixture *f, bool *out_painted) {
    mu_frame_begin(&f->ctx);
    mu_layout_run(&f->ctx);
    mu_damage_collect(&f->ctx);

    MuRect area = {0, 0, 0, 0};
    bool painted = !mu_damage_empty(&f->ctx);
    if (painted) {
        area = mu_damage_rect(&f->ctx);
        mu_soft_begin_frame_rect(&f->rc, BG, area);
        mu_paint_damaged(&f->ctx, &f->rc);
    }
    mu_damage_reset(&f->ctx);
    mu_frame_end(&f->ctx);
    if (out_painted) *out_painted = painted;
    return area;
}

static bool rect_contains(MuRect outer, MuRect inner) {
    return inner.x >= outer.x && inner.y >= outer.y && inner.x + inner.w <= outer.x + outer.w &&
           inner.y + inner.h <= outer.y + outer.h;
}

/* ------------------------------------------------------------------ */

static void test_first_frame_damages_everything(void) {
    CASE("first frame damages everything");
    Fixture f;
    fixture_init(&f);

    bool painted = false;
    MuRect area = frame(&f, &painted);
    CHECK(painted);
    /* Every node is new, so the union must cover the root. */
    CHECK(rect_contains(area, f.root->bounds));
    fixture_free(&f);
}

static void test_idle_frame_damages_nothing(void) {
    CASE("idle frame damages nothing");
    Fixture f;
    fixture_init(&f);
    frame(&f, NULL); /* first frame settles everything */

    for (int i = 0; i < 5; i++) {
        bool painted = true;
        frame(&f, &painted);
        CHECK(!painted);
        if (painted) {
            printf("       idle frame %d still reported damage\n", i);
            break;
        }
    }
    fixture_free(&f);
}

static void test_hover_damages_only_that_node(void) {
    CASE("hover damages only that node");
    Fixture f;
    fixture_init(&f);
    frame(&f, NULL);

    MuRect b = f.button->bounds;
    mu_input_update_hover(&f.ctx, (MuVec2){b.x + b.w * 0.5f, b.y + b.h * 0.5f});

    bool painted = false;
    MuRect area = frame(&f, &painted);
    CHECK(painted);
    /* Must cover the button but not escalate to the whole window. */
    CHECK(rect_contains(area, b));
    CHECK(area.w < (float)W);
    if (area.w >= (float)W)
        printf("       hover damaged full width (%g) — expected ~%g\n", area.w, b.w);
    fixture_free(&f);
}

static void test_move_damages_old_and_new(void) {
    CASE("move damages old and new");
    Fixture f;
    fixture_init(&f);
    frame(&f, NULL);

    MuRect before = f.button->bounds;
    MuRect after = before;
    after.x += 100.f;
    after.y += 60.f;
    mu_node_set_bounds(f.button, after);

    /* Layout would put it back, so damage-collect directly for this one. */
    mu_damage_collect(&f.ctx);
    MuRect area = mu_damage_rect(&f.ctx);
    CHECK(!mu_damage_empty(&f.ctx));
    CHECK(rect_contains(area, before)); /* vacated area must repaint too */
    CHECK(rect_contains(area, after));
    mu_damage_reset(&f.ctx);
    fixture_free(&f);
}

static void test_hidden_node_damages_vacated_area(void) {
    CASE("hiding damages vacated area");
    Fixture f;
    fixture_init(&f);
    frame(&f, NULL);

    MuRect was = f.button->bounds;
    f.button->flags &= ~MU_NODE_VISIBLE;

    mu_damage_collect(&f.ctx);
    CHECK(!mu_damage_empty(&f.ctx));
    CHECK(rect_contains(mu_damage_rect(&f.ctx), was));
    mu_damage_reset(&f.ctx);
    fixture_free(&f);
}

static void test_widget_state_marks_dirty(void) {
    CASE("widget state marks dirty");

    /* A checkbox toggle and a slider drag change neither bounds nor flags, so they are
     * exactly the cases the snapshot cannot see and must mark explicitly. */
    Fixture f;
    fixture_init(&f);
    frame(&f, NULL);

    MuRect cb = f.check->bounds;
    MuPointerEvent press = {{cb.x + 2.f, cb.y + 2.f}, 0, true, false, false};
    MuPointerEvent release = {{cb.x + 2.f, cb.y + 2.f}, 0, false, true, false};
    mu_input_dispatch_pointer(&f.ctx, &press);
    mu_input_dispatch_pointer(&f.ctx, &release);

    bool painted = false;
    frame(&f, &painted);
    CHECK(painted);
    if (!painted) printf("       checkbox toggle produced no damage\n");
    fixture_free(&f);
}

/**
 * The safety net. Drive identical interactions on two contexts — one repainting only
 * damage, one forced to full repaint — and require identical surfaces throughout.
 */
static void test_ab_matches_full_repaint(void) {
    CASE("partial repaint matches full repaint");

    Fixture partial, full;
    fixture_init(&partial);
    fixture_init(&full);
    full.ctx.damage_force_all = true;

    /* Same script on both, comparing after every step. */
    for (int step = 0; step < 12; step++) {
        Fixture *fs[2] = {&partial, &full};
        for (int i = 0; i < 2; i++) {
            Fixture *f = fs[i];
            MuRect b = f->button->bounds;
            MuRect cb = f->check->bounds;
            MuRect sl = f->slider->bounds;

            switch (step % 4) {
            case 0: /* hover the button */
                mu_input_update_hover(f->ctx.root ? &f->ctx : &f->ctx,
                                      (MuVec2){b.x + b.w * 0.5f, b.y + b.h * 0.5f});
                break;
            case 1: { /* toggle the checkbox */
                MuPointerEvent pr = {{cb.x + 2.f, cb.y + 2.f}, 0, true, false, false};
                MuPointerEvent rl = {{cb.x + 2.f, cb.y + 2.f}, 0, false, true, false};
                mu_input_dispatch_pointer(&f->ctx, &pr);
                mu_input_dispatch_pointer(&f->ctx, &rl);
                break;
            }
            case 2: { /* drag the slider */
                f->ctx.captured_pointer_id = f->slider->id;
                MuPointerEvent dg = {{sl.x + sl.w * (0.2f + 0.06f * step), sl.y + 2.f}, 0, false, false, true};
                mu_input_dispatch_pointer(&f->ctx, &dg);
                f->ctx.captured_pointer_id = 0;
                break;
            }
            default: /* move the hover away */
                mu_input_update_hover(&f->ctx, (MuVec2){-10.f, -10.f});
                break;
            }
            frame(f, NULL);
        }

        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                MuColor a = mu_soft_get_pixel(&partial.rc, x, y);
                MuColor b = mu_soft_get_pixel(&full.rc, x, y);
                if (a.r != b.r || a.g != b.g || a.b != b.b || a.a != b.a) {
                    printf("FAIL [%s] step %d pixel (%d,%d): partial {%u,%u,%u,%u} vs full {%u,%u,%u,%u}\n",
                           g_case, step, x, y, a.r, a.g, a.b, a.a, b.r, b.g, b.b, b.a);
                    g_failures++;
                    goto done; /* one report is enough */
                }
            }
        }
    }
done:
    fixture_free(&partial);
    fixture_free(&full);
}

/**
 * A backdrop-blurred node must pull its whole area, plus the blur reach, into the damage
 * rect the moment anything touches it. Two reasons: its appearance depends on content it
 * does not own, and the blur kernel samples past the damage edge into pixels that still
 * hold last frame's composited output.
 */
static void test_backdrop_expands_damage(void) {
    CASE("backdrop blur expands damage");
    Fixture f;
    fixture_init(&f);

    MuNode *glass = mu_make_panel(&f.ctx, true);
    glass->role = "glass";
    mu_node_set_backdrop_blur(glass, 12.f);
    mu_node_add_child(&f.ctx, f.root, glass);
    frame(&f, NULL); /* let layout place it and settle the snapshots */

    /* Assert against wherever layout actually put the node: flex owns these bounds, so
     * hardcoding a rectangle here would only test that layout had not moved. */
    MuRect g = glass->bounds;
    CHECK(g.w > 0.f && g.h > 0.f);

    /* Touch the glass itself. Ordinarily that would damage exactly its bounds; the
     * expansion must widen it by the blur reach, because the kernel reads that far out. */
    mu_node_mark_paint_dirty(glass);

    mu_damage_collect(&f.ctx);
    MuRect area = mu_damage_rect(&f.ctx);
    CHECK(!mu_damage_empty(&f.ctx));

    MuRect needed = {g.x - 12.f, g.y - 12.f, g.w + 24.f, g.h + 24.f};
    CHECK(rect_contains(area, needed));
    if (!rect_contains(area, needed))
        printf("       damage %.0f,%.0f %.0fx%.0f does not cover glass+blur\n", area.x, area.y, area.w,
               area.h);
    mu_damage_reset(&f.ctx);
    fixture_free(&f);
}

/**
 * The A/B check with glass in the scene. If the expansion above were missing, the blur
 * would re-read its own previous output near the damage boundary and smear a little more
 * each frame, so the partial and full renders would diverge.
 */
static void test_glass_partial_matches_full(void) {
    CASE("glass partial repaint matches full");
    Fixture partial, full;
    fixture_init(&partial);
    fixture_init(&full);
    full.ctx.damage_force_all = true;

    Fixture *fs[2] = {&partial, &full};
    MuNode *glass[2];
    for (int i = 0; i < 2; i++) {
        glass[i] = mu_make_panel(&fs[i]->ctx, true);
        glass[i]->role = "glass";
        mu_node_set_backdrop_blur(glass[i], 10.f);
        mu_node_add_child(&fs[i]->ctx, fs[i]->root, glass[i]);
    }

    for (int step = 0; step < 10; step++) {
        for (int i = 0; i < 2; i++) {
            Fixture *f = fs[i];
            /* Slide a button under the glass so the backdrop keeps changing. */
            mu_node_set_bounds(f->button, (MuRect){20.f + (float)step * 6.f, 60.f, 40.f, 20.f});
            mu_node_set_bounds(glass[i], (MuRect){60.f, 40.f, 120.f, 80.f});
            frame(f, NULL);
        }

        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                MuColor a = mu_soft_get_pixel(&partial.rc, x, y);
                MuColor b = mu_soft_get_pixel(&full.rc, x, y);
                if (a.r != b.r || a.g != b.g || a.b != b.b) {
                    printf("FAIL [%s] step %d pixel (%d,%d): partial {%u,%u,%u} vs full {%u,%u,%u}\n",
                           g_case, step, x, y, a.r, a.g, a.b, b.r, b.g, b.b);
                    g_failures++;
                    goto done;
                }
            }
    }
done:
    fixture_free(&partial);
    fixture_free(&full);
}

/* ---------------- backdrop blur cache ---------------- */

/**
 * A glass panel over the existing widgets, with a button of its own inside it.
 *
 * Flex owns the panel's position, so the tests read glass->bounds rather than asserting a
 * rectangle. Calling mu_node_set_bounds every frame would be worse than useless here: the
 * layout pass overwrites it anyway, and the call marks the panel dirty, which is itself
 * enough to suppress the caching these tests exist to observe.
 */
static MuNode *glass_setup(Fixture *f, MuNode **out_inner) {
    MuNode *glass = mu_make_panel(&f->ctx, true);
    glass->role = "glass";
    mu_node_set_backdrop_blur(glass, 10.f);
    mu_node_add_child(&f->ctx, f->root, glass);

    MuNode *inner = mu_make_button(&f->ctx, "In", NULL, NULL);
    mu_node_add_child(&f->ctx, glass, inner);

    /* Settle: layout, first paint, then idle frames so the snapshots agree. */
    for (int i = 0; i < 3; i++) frame(f, NULL);
    if (out_inner) *out_inner = inner;
    return glass;
}

/**
 * The entire point of the cache: something changing *inside* a glass panel must not
 * recompute the blur. The inner button damages pixels the panel owns, but the blur reads
 * what sits behind the panel, and nothing touched that.
 */
static void test_backdrop_cache_hits_on_inner_change(void) {
    CASE("backdrop cache hits when only the panel's own subtree changes");
    Fixture f;
    fixture_init(&f);
    MuNode *inner = NULL;
    glass_setup(&f, &inner);

    /* Without a populated cache, the hit below would prove nothing. */
    CHECK(f.rc.backdrop_misses > 0);
    uint32_t hits = f.rc.backdrop_hits, misses = f.rc.backdrop_misses;

    mu_node_mark_paint_dirty(inner);
    frame(&f, NULL);

    CHECK(f.rc.backdrop_hits == hits + 1);
    CHECK(f.rc.backdrop_misses == misses);
    if (f.rc.backdrop_misses != misses)
        printf("       inner-only change still recomputed the blur\n");
    fixture_free(&f);
}

/** The converse: content behind the panel changing must invalidate. */
static void test_backdrop_cache_misses_when_backdrop_changes(void) {
    CASE("backdrop cache misses when content behind the panel changes");
    Fixture f;
    fixture_init(&f);
    glass_setup(&f, NULL);

    uint32_t hits = f.rc.backdrop_hits, misses = f.rc.backdrop_misses;

    /* The root panel is painted before the glass and spans the whole surface, so
     * repainting it genuinely changes what the blur reads. */
    mu_node_mark_paint_dirty(f.root);
    frame(&f, NULL);

    CHECK(f.rc.backdrop_misses == misses + 1);
    CHECK(f.rc.backdrop_hits == hits);
    if (f.rc.backdrop_hits != hits)
        printf("       stale blur served while the backdrop changed\n");
    fixture_free(&f);
}

/**
 * Damage that misses the panel entirely must not invalidate it — otherwise the cache
 * would be correct but useless, recomputing on every unrelated interaction.
 *
 * Flex stacks the widgets in a column, so the button sits clear of the panel. The
 * overlap precondition is asserted rather than assumed: if layout ever changes and the
 * button lands under the glass, this test must fail loudly rather than quietly checking
 * nothing.
 */
static void test_backdrop_cache_survives_distant_damage(void) {
    CASE("backdrop cache survives damage that misses the panel");
    Fixture f;
    fixture_init(&f);
    MuNode *glass = glass_setup(&f, NULL);

    const float reach = 10.f + 1.f;
    MuRect g = glass->bounds, b = f.button->bounds;
    bool clear_of_panel = !(b.x < g.x + g.w + reach && g.x - reach < b.x + b.w &&
                            b.y < g.y + g.h + reach && g.y - reach < b.y + b.h);
    CHECK(clear_of_panel);
    if (!clear_of_panel) {
        printf("       button %.0f,%.0f %.0fx%.0f overlaps glass %.0f,%.0f %.0fx%.0f + reach\n", b.x,
               b.y, b.w, b.h, g.x, g.y, g.w, g.h);
        fixture_free(&f);
        return;
    }

    uint32_t hits = f.rc.backdrop_hits, misses = f.rc.backdrop_misses;
    f.button->flags |= MU_NODE_HOVERED;
    frame(&f, NULL);

    /* Best case, and what actually happens: the damage never reaches the panel, so it is
     * pruned from the repaint entirely — neither recomputed nor even replayed. */
    CHECK(f.rc.backdrop_misses == misses);
    CHECK(f.rc.backdrop_hits == hits);
    if (f.rc.backdrop_misses != misses)
        printf("       unrelated damage recomputed the blur\n");

    /* The part that matters for later frames: that skipped frame must not have marked the
     * blur stale. Force the panel to repaint and it should still be served from cache. */
    mu_node_mark_paint_dirty(glass);
    frame(&f, NULL);
    CHECK(f.rc.backdrop_hits == hits + 1);
    CHECK(f.rc.backdrop_misses == misses);
    if (f.rc.backdrop_misses != misses)
        printf("       unrelated damage left the blur marked stale\n");
    fixture_free(&f);
}

/**
 * Correctness rather than bookkeeping: drive changes inside the panel so the cache is
 * carrying the work, and compare every pixel against a fixture that recomputes the blur
 * every frame. damage_force_all makes mu_node_backdrop_unchanged refuse, so the reference
 * never consults a cache and the two paths stay genuinely independent.
 *
 * The hit assertion is what gives this teeth — without it the test would pass just as
 * happily if the cache never engaged at all.
 */
static void test_glass_cache_matches_recompute(void) {
    CASE("cached glass matches a from-scratch blur");
    Fixture partial, full;
    fixture_init(&partial);
    fixture_init(&full);
    full.ctx.damage_force_all = true;

    Fixture *fs[2] = {&partial, &full};
    MuNode *glass[2], *inner[2];
    for (int i = 0; i < 2; i++) glass[i] = glass_setup(fs[i], &inner[i]);

    uint32_t hits_before = partial.rc.backdrop_hits;

    for (int step = 0; step < 10; step++) {
        for (int i = 0; i < 2; i++) {
            Fixture *f = fs[i];
            /* Toggle hover on the button *inside* the panel. That is exactly the case the
             * cache targets: a real visual change, entirely within the panel's own
             * subtree, leaving the backdrop untouched. */
            inner[i]->flags ^= MU_NODE_HOVERED;
            frame(f, NULL);
        }

        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                MuColor a = mu_soft_get_pixel(&partial.rc, x, y);
                MuColor b = mu_soft_get_pixel(&full.rc, x, y);
                if (a.r != b.r || a.g != b.g || a.b != b.b) {
                    printf("FAIL [%s] step %d pixel (%d,%d): cached {%u,%u,%u} vs fresh {%u,%u,%u}\n",
                           g_case, step, x, y, a.r, a.g, a.b, b.r, b.g, b.b);
                    g_failures++;
                    goto done;
                }
            }
    }

    CHECK(partial.rc.backdrop_hits > hits_before);
    if (partial.rc.backdrop_hits == hits_before)
        printf("       cache never engaged, so this compared two recomputes\n");
    /* If the reference used a cache it would not be a reference. */
    CHECK(full.rc.backdrop_hits == 0);
done:
    fixture_free(&partial);
    fixture_free(&full);
}

/**
 * A glass panel parked on the modal layer, as the demo does.
 *
 * That layer paints last and mu_layout_run never touches it, so the panel can be
 * positioned squarely over the flex content below — which is what makes the backdrop
 * genuinely change when those widgets do.
 */
static MuNode *glass_overlay_setup(Fixture *f, MuRect at) {
    MuNode *overlay = mu_make_panel(&f->ctx, true);
    overlay->role = "group";
    mu_modal_bind_layer(&f->ctx, overlay);
    mu_node_set_bounds(overlay, (MuRect){0.f, 0.f, (float)W, (float)H});

    MuNode *glass = mu_make_panel(&f->ctx, true);
    glass->role = "glass";
    mu_node_set_backdrop_blur(glass, 10.f);
    mu_node_add_child(&f->ctx, overlay, glass);
    mu_node_set_bounds(glass, at);

    for (int i = 0; i < 3; i++) frame(f, NULL);
    return glass;
}

/**
 * The strong test: content really moving underneath a glass panel, compared pixel for
 * pixel against a fixture that recomputes the blur every frame.
 *
 * This is the one that catches a cache serving stale pixels. The bookkeeping tests would
 * still pass if `try` returned the wrong image; only comparing against a from-scratch
 * render can tell that the pixels themselves are right.
 */
static void test_glass_over_moving_backdrop_matches_recompute(void) {
    CASE("glass over changing backdrop matches a from-scratch blur");
    Fixture partial, full;
    fixture_init(&partial);
    fixture_init(&full);
    full.ctx.damage_force_all = true;

    Fixture *fs[2] = {&partial, &full};
    const MuRect at = {20.f, 20.f, 200.f, 140.f};
    for (int i = 0; i < 2; i++) glass_overlay_setup(fs[i], at);

    /* If the panel did not actually cover the button, the loop below would be comparing
     * two identical static images and proving nothing. */
    MuRect b = partial.button->bounds;
    bool covers_button = b.x < at.x + at.w && at.x < b.x + b.w && b.y < at.y + at.h &&
                         at.y < b.y + b.h;
    CHECK(covers_button);
    if (!covers_button)
        printf("       glass %.0f,%.0f %.0fx%.0f does not cover button %.0f,%.0f %.0fx%.0f\n", at.x,
               at.y, at.w, at.h, b.x, b.y, b.w, b.h);

    uint32_t misses_before = partial.rc.backdrop_misses;

    for (int step = 0; step < 8; step++) {
        for (int i = 0; i < 2; i++) {
            /* The button sits behind the panel, so its highlight really does change what
             * the blur reads. */
            fs[i]->button->flags ^= MU_NODE_HOVERED;
            frame(fs[i], NULL);
        }

        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                MuColor a = mu_soft_get_pixel(&partial.rc, x, y);
                MuColor c = mu_soft_get_pixel(&full.rc, x, y);
                if (a.r != c.r || a.g != c.g || a.b != c.b) {
                    printf("FAIL [%s] step %d pixel (%d,%d): cached {%u,%u,%u} vs fresh {%u,%u,%u}\n",
                           g_case, step, x, y, a.r, a.g, a.b, c.r, c.g, c.b);
                    g_failures++;
                    goto done;
                }
            }
    }

    /* The backdrop changed on every step, so the blur must have been recomputed. A cache
     * that hit here would be serving a stale image. */
    CHECK(partial.rc.backdrop_misses > misses_before);
done:
    fixture_free(&partial);
    fixture_free(&full);
}

/**
 * Invalidation has to outlive the frame that raised it.
 *
 * A panel hidden while the content behind it changes has no chance to react: it is not
 * painted, so it cannot recompute, and by the time it reappears the damage that mattered
 * is several frames in the past. MU_NODE_BACKDROP_DIRTY is sticky precisely for this —
 * it is cleared when the blur is recomputed, not at the end of the frame.
 *
 * The quiet frame in the middle is what gives this test its teeth: without it, the
 * reappearance would coincide with the damage and a per-frame flag would look correct.
 */
static void test_backdrop_change_while_hidden_still_invalidates(void) {
    CASE("backdrop change while the panel is hidden still invalidates");
    Fixture partial, full;
    fixture_init(&partial);
    fixture_init(&full);
    full.ctx.damage_force_all = true;

    Fixture *fs[2] = {&partial, &full};
    MuNode *glass[2];
    for (int i = 0; i < 2; i++) glass[i] = glass_overlay_setup(fs[i], (MuRect){20.f, 20.f, 200.f, 140.f});

    uint32_t hits = partial.rc.backdrop_hits, misses = partial.rc.backdrop_misses;

    for (int i = 0; i < 2; i++) {
        Fixture *f = fs[i];
        glass[i]->flags &= ~MU_NODE_VISIBLE; /* hide */
        frame(f, NULL);
        f->button->flags |= MU_NODE_HOVERED; /* backdrop changes while hidden */
        frame(f, NULL);
        frame(f, NULL);                    /* a quiet frame: the damage is now in the past */
        glass[i]->flags |= MU_NODE_VISIBLE; /* reappear */
        frame(f, NULL);
    }

    /* The blur it had cached was built over a button that was not highlighted. */
    CHECK(partial.rc.backdrop_misses > misses);
    CHECK(partial.rc.backdrop_hits == hits);
    if (partial.rc.backdrop_hits != hits)
        printf("       panel reappeared with a blur cached before the backdrop changed\n");

    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            MuColor a = mu_soft_get_pixel(&partial.rc, x, y);
            MuColor b = mu_soft_get_pixel(&full.rc, x, y);
            if (a.r != b.r || a.g != b.g || a.b != b.b) {
                printf("FAIL [%s] pixel (%d,%d): cached {%u,%u,%u} vs fresh {%u,%u,%u}\n", g_case, x,
                       y, a.r, a.g, a.b, b.r, b.g, b.b);
                g_failures++;
                goto done;
            }
        }
done:
    fixture_free(&partial);
    fixture_free(&full);
}

/** A dropped entry must force a recompute rather than resurrect stale pixels. */
static void test_backdrop_cache_drop_forces_recompute(void) {
    CASE("dropping a cache entry forces recompute");
    Fixture f;
    fixture_init(&f);
    MuNode *inner = NULL;
    MuNode *glass = glass_setup(&f, &inner);

    mu_backdrop_cache_drop(&f.rc, glass);
    uint32_t misses = f.rc.backdrop_misses;

    mu_node_mark_paint_dirty(inner);
    frame(&f, NULL);

    CHECK(f.rc.backdrop_misses == misses + 1);
    fixture_free(&f);
}

int main(void) {
    test_first_frame_damages_everything();
    test_idle_frame_damages_nothing();
    test_hover_damages_only_that_node();
    test_move_damages_old_and_new();
    test_hidden_node_damages_vacated_area();
    test_widget_state_marks_dirty();
    test_ab_matches_full_repaint();
    test_backdrop_expands_damage();
    test_glass_partial_matches_full();
    test_backdrop_cache_hits_on_inner_change();
    test_backdrop_cache_misses_when_backdrop_changes();
    test_backdrop_cache_survives_distant_damage();
    test_glass_cache_matches_recompute();
    test_glass_over_moving_backdrop_matches_recompute();
    test_backdrop_change_while_hidden_still_invalidates();
    test_backdrop_cache_drop_forces_recompute();

    if (g_failures) {
        printf("\n%d check(s) failed\n", g_failures);
        return 1;
    }
    printf("all damage checks passed\n");
    return 0;
}
