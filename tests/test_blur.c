/*
 * Backdrop blur tests.
 *
 * Blur has properties that pin it down exactly: a uniform field must survive untouched,
 * a step edge must spread symmetrically, average intensity must be preserved, and edges
 * of the surface must not darken. Those catch the mistakes that actually happen — a
 * wrong window size, an off-by-one in the sliding sum, or sampling past the edge as
 * black instead of clamping.
 */

#include "markup/mu_soft.h"

#include <stdio.h>
#include <stdlib.h>

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

#define W 120
#define H 90

static const MuColor CLEAR_C = {0, 0, 0, 0};
static const MuColor GREY = {128, 128, 128, 255};
static const MuColor RED = {255, 0, 0, 255};
static const MuColor BLUE = {0, 0, 255, 255};
static const MuColor SENTINEL = {17, 34, 51, 255};

static int px_r(const MuRenderContext *rc, int x, int y) {
    return mu_soft_get_pixel(rc, x, y).r;
}

/* ------------------------------------------------------------------ */

static void test_uniform_field_survives(void) {
    CASE("uniform field survives blur");
    MuRenderContext rc;
    mu_soft_render_init(&rc, W, H);
    mu_soft_begin_frame(&rc, GREY);

    mu_draw_backdrop_blur(&rc, (MuRect){20.f, 20.f, 60.f, 40.f}, 12.f, CLEAR_C, 0.f);

    /* Blurring a constant field must return the same constant. Any deviation means the
     * window size and the divisor disagree, or the edge clamp is wrong. */
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            MuColor p = mu_soft_get_pixel(&rc, x, y);
            if (p.r != 128 || p.g != 128 || p.b != 128) {
                printf("FAIL [%s] pixel (%d,%d) = {%u,%u,%u}, expected {128,128,128}\n", g_case, x, y,
                       p.r, p.g, p.b);
                g_failures++;
                goto done;
            }
        }
done:
    mu_soft_render_shutdown(&rc);
}

static void test_edge_of_surface_does_not_darken(void) {
    CASE("surface edge does not darken");
    MuRenderContext rc;
    mu_soft_render_init(&rc, W, H);
    mu_soft_begin_frame(&rc, GREY);

    /* Blur flush against the top-left corner: the kernel reaches off-surface. Sampling
     * those as black would pull the corner down — clamping keeps it at 128. */
    mu_draw_backdrop_blur(&rc, (MuRect){0.f, 0.f, 40.f, 40.f}, 16.f, CLEAR_C, 0.f);

    CHECK(px_r(&rc, 0, 0) == 128);
    CHECK(px_r(&rc, 1, 1) == 128);
    CHECK(px_r(&rc, 39, 39) == 128);
    mu_soft_render_shutdown(&rc);
}

static void test_step_edge_spreads(void) {
    CASE("step edge spreads symmetrically");
    MuRenderContext rc;
    mu_soft_render_init(&rc, W, H);
    mu_soft_begin_frame(&rc, (MuColor){0, 0, 0, 255});
    /* Left half red, right half black, hard edge at x=60. */
    mu_draw_rect(&rc, (MuRect){0.f, 0.f, 60.f, (float)H}, RED, CLEAR_C, 0.f, 0.f);

    int before_left = px_r(&rc, 50, 45);
    int before_right = px_r(&rc, 70, 45);
    CHECK(before_left == 255);
    CHECK(before_right == 0);

    mu_draw_backdrop_blur(&rc, (MuRect){10.f, 10.f, 100.f, 70.f}, 15.f, CLEAR_C, 0.f);

    /* The hard edge must become a ramp, and the midpoint should sit near half. */
    int mid = px_r(&rc, 60, 45);
    CHECK(mid > 90 && mid < 165);
    if (!(mid > 90 && mid < 165)) printf("       midpoint = %d, expected ~128\n", mid);

    /* Monotonic falloff across the transition. */
    int a = px_r(&rc, 52, 45), b = px_r(&rc, 60, 45), c = px_r(&rc, 68, 45);
    CHECK(a > b && b > c);

    /* Symmetry: equal distances either side should sum to roughly the full range. */
    int l = px_r(&rc, 55, 45), r = px_r(&rc, 65, 45);
    CHECK(abs((l + r) - 255) < 40);
    mu_soft_render_shutdown(&rc);
}

static void test_average_is_preserved(void) {
    CASE("average intensity preserved");
    MuRenderContext rc;
    mu_soft_render_init(&rc, W, H);
    mu_soft_begin_frame(&rc, (MuColor){0, 0, 0, 255});
    mu_draw_rect(&rc, (MuRect){0.f, 0.f, 60.f, (float)H}, RED, CLEAR_C, 0.f, 0.f);

    /* Blur a strip well inside the field, then compare its mean before and after. A
     * blur redistributes energy; it must not create or destroy it. */
    const int x0 = 30, x1 = 90, y0 = 30, y1 = 60;
    long before = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) before += px_r(&rc, x, y);

    mu_draw_backdrop_blur(&rc, (MuRect){(float)x0, (float)y0, (float)(x1 - x0), (float)(y1 - y0)}, 9.f,
                          CLEAR_C, 0.f);

    long after = 0;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) after += px_r(&rc, x, y);

    double rel = (double)(after - before) / (double)before;
    CHECK(rel > -0.06 && rel < 0.06);
    if (!(rel > -0.06 && rel < 0.06)) printf("       mean shifted by %.1f%%\n", rel * 100.0);
    mu_soft_render_shutdown(&rc);
}

static void test_tint_mixes(void) {
    CASE("tint mixes toward the tint colour");
    MuRenderContext rc;
    mu_soft_render_init(&rc, W, H);
    mu_soft_begin_frame(&rc, (MuColor){0, 0, 0, 255});

    /* Half-strength white tint over black must land near mid grey. */
    mu_draw_backdrop_blur(&rc, (MuRect){20.f, 20.f, 60.f, 40.f}, 6.f, (MuColor){255, 255, 255, 128},
                          0.f);
    int v = px_r(&rc, 50, 40);
    CHECK(v > 118 && v < 138);
    if (!(v > 118 && v < 138)) printf("       tinted value = %d, expected ~128\n", v);

    /* Fully opaque tint replaces the backdrop entirely. */
    mu_draw_backdrop_blur(&rc, (MuRect){20.f, 20.f, 60.f, 40.f}, 6.f, BLUE, 0.f);
    MuColor p = mu_soft_get_pixel(&rc, 50, 40);
    CHECK(p.b > 245 && p.r < 10);
    mu_soft_render_shutdown(&rc);
}

static void test_respects_clip_and_bounds(void) {
    CASE("writes stay inside clip and bounds");
    MuRenderContext rc;
    mu_soft_render_init(&rc, W, H);
    mu_soft_begin_frame(&rc, SENTINEL);

    mu_push_scissor(&rc, (MuRect){40.f, 30.f, 20.f, 20.f});
    /* Absurd area and radius: nothing may be written outside the scissor. */
    mu_draw_backdrop_blur(&rc, (MuRect){-200.f, -200.f, 600.f, 600.f}, 24.f, RED, 0.f);
    mu_pop_scissor(&rc);

    int escaped = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int inside = (x >= 40 && x < 60 && y >= 30 && y < 50);
            MuColor p = mu_soft_get_pixel(&rc, x, y);
            bool same = (p.r == SENTINEL.r && p.g == SENTINEL.g && p.b == SENTINEL.b);
            if (!inside && !same) escaped++;
        }
    CHECK(escaped == 0);
    if (escaped) printf("       %d pixel(s) written outside the clip\n", escaped);
    mu_soft_render_shutdown(&rc);
}

static void test_rounded_corners_masked(void) {
    CASE("rounded corners are masked");
    MuRenderContext rc;
    mu_soft_render_init(&rc, W, H);
    mu_soft_begin_frame(&rc, SENTINEL);

    /* Opaque tint with a big corner radius: the corner must be left untouched. */
    mu_draw_backdrop_blur(&rc, (MuRect){20.f, 20.f, 60.f, 60.f}, 8.f, RED, 20.f);

    MuColor corner = mu_soft_get_pixel(&rc, 21, 21);
    CHECK(corner.r == SENTINEL.r && corner.b == SENTINEL.b);
    MuColor centre = mu_soft_get_pixel(&rc, 50, 50);
    CHECK(centre.r > 245);
    /* Mid-edge is inside the rounded shape. */
    MuColor edge = mu_soft_get_pixel(&rc, 50, 21);
    CHECK(edge.r > 200);
    mu_soft_render_shutdown(&rc);
}

static void test_zero_radius_is_identity(void) {
    CASE("zero blur radius leaves pixels alone");
    MuRenderContext rc;
    mu_soft_render_init(&rc, W, H);
    mu_soft_begin_frame(&rc, (MuColor){0, 0, 0, 255});
    mu_draw_rect(&rc, (MuRect){0.f, 0.f, 60.f, (float)H}, RED, CLEAR_C, 0.f, 0.f);

    mu_draw_backdrop_blur(&rc, (MuRect){10.f, 10.f, 100.f, 70.f}, 0.f, CLEAR_C, 0.f);

    /* No blur, no tint: the step edge must still be perfectly sharp. */
    CHECK(px_r(&rc, 59, 45) == 255);
    CHECK(px_r(&rc, 60, 45) == 0);
    mu_soft_render_shutdown(&rc);
}

static void test_repeated_application_is_stable(void) {
    CASE("repeated blur does not run away");
    MuRenderContext rc;
    mu_soft_render_init(&rc, W, H);
    mu_soft_begin_frame(&rc, GREY);

    /* Re-blurring a uniform field many times must not drift: this is the feedback case
     * that damage expansion exists to prevent, and the arithmetic must not compound
     * rounding error either. */
    for (int i = 0; i < 40; i++)
        mu_draw_backdrop_blur(&rc, (MuRect){10.f, 10.f, 80.f, 60.f}, 10.f, CLEAR_C, 0.f);

    CHECK(px_r(&rc, 50, 40) == 128);
    CHECK(px_r(&rc, 11, 11) == 128);
    mu_soft_render_shutdown(&rc);
}

int main(void) {
    test_uniform_field_survives();
    test_edge_of_surface_does_not_darken();
    test_step_edge_spreads();
    test_average_is_preserved();
    test_tint_mixes();
    test_respects_clip_and_bounds();
    test_rounded_corners_masked();
    test_zero_radius_is_identity();
    test_repeated_application_is_stable();

    if (g_failures) {
        printf("\n%d check(s) failed\n", g_failures);
        return 1;
    }
    printf("all blur checks passed\n");
    return 0;
}
