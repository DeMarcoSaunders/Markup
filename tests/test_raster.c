/*
 * Phase 1 rasterizer tests for the software backend.
 *
 * Headless: allocates its own surface, asserts individual pixel values. No window,
 * no GPU, no SDL. Run via `ctest` or execute directly.
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

static int color_eq(MuColor a, MuColor b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static int color_near(MuColor a, MuColor b, int tol) {
    int dr = (int)a.r - (int)b.r, dg = (int)a.g - (int)b.g;
    int db = (int)a.b - (int)b.b, da = (int)a.a - (int)b.a;
    if (dr < 0) dr = -dr;
    if (dg < 0) dg = -dg;
    if (db < 0) db = -db;
    if (da < 0) da = -da;
    return dr <= tol && dg <= tol && db <= tol && da <= tol;
}

static void check_pixel(const MuRenderContext *rc, int x, int y, MuColor want) {
    MuColor got = mu_soft_get_pixel(rc, x, y);
    if (!color_eq(got, want)) {
        printf("FAIL [%s] pixel (%d,%d): got {%u,%u,%u,%u} want {%u,%u,%u,%u}\n", g_case, x, y, got.r, got.g,
               got.b, got.a, want.r, want.g, want.b, want.a);
        g_failures++;
    }
}

static const MuColor BLACK = {0, 0, 0, 255};
static const MuColor RED = {255, 0, 0, 255};
static const MuColor GREEN = {0, 255, 0, 255};
static const MuColor SENTINEL = {17, 34, 51, 255};

/* ------------------------------------------------------------------ */

static void test_pack_roundtrip(void) {
    CASE("pack roundtrip");
    MuColor c = {0x12, 0x34, 0x56, 0x78};
    CHECK(color_eq(mu_soft_unpack(mu_soft_pack(c)), c));
    /* ARGB8888 byte layout, as SDL_PIXELFORMAT_ARGB8888 expects. */
    CHECK(mu_soft_pack((MuColor){0xAA, 0xBB, 0xCC, 0xDD}) == 0xDDAABBCCu);
}

static void test_opaque_fill_bounds(void) {
    CASE("opaque fill bounds");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 32, 32);
    mu_soft_begin_frame(&rc, BLACK);

    mu_draw_rect(&rc, (MuRect){10.f, 8.f, 6.f, 4.f}, RED, (MuColor){0, 0, 0, 0}, 0.f, 0.f);

    /* Interior corners of the half-open rect [10,16) x [8,12). */
    check_pixel(&rc, 10, 8, RED);
    check_pixel(&rc, 15, 8, RED);
    check_pixel(&rc, 10, 11, RED);
    check_pixel(&rc, 15, 11, RED);
    /* One pixel outside on every side must be untouched. */
    check_pixel(&rc, 9, 8, BLACK);
    check_pixel(&rc, 16, 8, BLACK);
    check_pixel(&rc, 10, 7, BLACK);
    check_pixel(&rc, 10, 12, BLACK);

    mu_soft_render_shutdown(&rc);
}

static void test_blend_identity(void) {
    CASE("blend identity");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 8, 8);

    /* alpha 255 replaces exactly */
    mu_soft_begin_frame(&rc, BLACK);
    mu_draw_rect(&rc, (MuRect){0.f, 0.f, 8.f, 8.f}, GREEN, (MuColor){0, 0, 0, 0}, 0.f, 0.f);
    check_pixel(&rc, 4, 4, GREEN);

    /* alpha 0 leaves the destination bit-identical */
    mu_soft_begin_frame(&rc, BLACK);
    mu_draw_rect(&rc, (MuRect){0.f, 0.f, 8.f, 8.f}, (MuColor){255, 255, 255, 0}, (MuColor){0, 0, 0, 0}, 0.f,
                 0.f);
    check_pixel(&rc, 4, 4, BLACK);

    mu_soft_render_shutdown(&rc);
}

static void test_blend_midpoint(void) {
    CASE("blend midpoint");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 8, 8);
    mu_soft_begin_frame(&rc, BLACK);

    /* 50% white over opaque black -> mid grey, opaque. */
    mu_draw_rect(&rc, (MuRect){0.f, 0.f, 8.f, 8.f}, (MuColor){255, 255, 255, 128}, (MuColor){0, 0, 0, 0}, 0.f,
                 0.f);
    MuColor got = mu_soft_get_pixel(&rc, 4, 4);
    MuColor want = {128, 128, 128, 255};
    if (!color_near(got, want, 1)) {
        printf("FAIL [%s] got {%u,%u,%u,%u} want ~{128,128,128,255}\n", g_case, got.r, got.g, got.b, got.a);
        g_failures++;
    }

    mu_soft_render_shutdown(&rc);
}

static void test_scissor_clips(void) {
    CASE("scissor clips");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 32, 32);
    mu_soft_begin_frame(&rc, SENTINEL);

    mu_push_scissor(&rc, (MuRect){8.f, 8.f, 8.f, 8.f});
    mu_draw_rect(&rc, (MuRect){0.f, 0.f, 32.f, 32.f}, RED, (MuColor){0, 0, 0, 0}, 0.f, 0.f);
    mu_pop_scissor(&rc);

    check_pixel(&rc, 8, 8, RED);
    check_pixel(&rc, 15, 15, RED);
    check_pixel(&rc, 7, 8, SENTINEL);
    check_pixel(&rc, 16, 8, SENTINEL);
    check_pixel(&rc, 8, 7, SENTINEL);
    check_pixel(&rc, 8, 16, SENTINEL);

    mu_soft_render_shutdown(&rc);
}

/* Nothing may ever escape the clip region — the cheapest way to catch span bugs. */
static void test_clip_canary(void) {
    CASE("clip canary");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 24, 24);
    mu_soft_begin_frame(&rc, SENTINEL);

    mu_push_scissor(&rc, (MuRect){4.f, 4.f, 16.f, 16.f});
    /* Deliberately absurd geometry, including negative origin. */
    mu_draw_rect(&rc, (MuRect){-100.f, -100.f, 400.f, 400.f}, GREEN, RED, 3.f, 0.f);
    mu_pop_scissor(&rc);

    int escaped = 0;
    for (int y = 0; y < 24; y++) {
        for (int x = 0; x < 24; x++) {
            int inside = (x >= 4 && x < 20 && y >= 4 && y < 20);
            if (!inside && !color_eq(mu_soft_get_pixel(&rc, x, y), SENTINEL)) escaped++;
        }
    }
    CHECK(escaped == 0);
    if (escaped) printf("       %d pixel(s) escaped the clip region\n", escaped);

    mu_soft_render_shutdown(&rc);
}

static void test_clip_stack_balance(void) {
    CASE("clip stack balance");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 16, 16);

    /* Overflow the stack, then unwind fully: depth must return to zero, not desync. */
    const int overflow_by = 10;
    for (int i = 0; i < MU_SOFT_CLIP_MAX + overflow_by; i++)
        mu_push_scissor(&rc, (MuRect){0.f, 0.f, 16.f, 16.f});
    CHECK(rc.scissor_overflow == overflow_by);
    for (int i = 0; i < MU_SOFT_CLIP_MAX + overflow_by; i++) mu_pop_scissor(&rc);
    CHECK(rc.scissor_depth == 0);
    CHECK(rc.scissor_overflow == 0);

    /* Popping an empty stack must not underflow. */
    mu_pop_scissor(&rc);
    CHECK(rc.scissor_depth == 0);

    mu_soft_render_shutdown(&rc);
}

static void test_border_only(void) {
    CASE("border only");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 16, 16);
    mu_soft_begin_frame(&rc, BLACK);

    mu_draw_rect(&rc, (MuRect){2.f, 2.f, 10.f, 10.f}, (MuColor){0, 0, 0, 0}, RED, 2.f, 0.f);

    check_pixel(&rc, 2, 2, RED);   /* top-left of border */
    check_pixel(&rc, 11, 11, RED); /* bottom-right of border */
    check_pixel(&rc, 3, 3, RED);   /* still within 2px band */
    check_pixel(&rc, 6, 6, BLACK); /* interior left unfilled */
    check_pixel(&rc, 1, 1, BLACK); /* outside */

    mu_soft_render_shutdown(&rc);
}

/* --- Phase 2: rounded rects + antialiasing ------------------------------- */

static void test_rounded_cuts_corners(void) {
    CASE("rounded cuts corners");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 40, 40);
    mu_soft_begin_frame(&rc, BLACK);

    /* radius 10 on a 40x40 box: the extreme corner pixel must be outside the shape,
     * while the centre and the mid-edge points stay fully covered. */
    mu_draw_rect(&rc, (MuRect){0.f, 0.f, 40.f, 40.f}, RED, (MuColor){0, 0, 0, 0}, 0.f, 10.f);

    check_pixel(&rc, 0, 0, BLACK);   /* top-left corner cut away */
    check_pixel(&rc, 39, 0, BLACK);  /* top-right */
    check_pixel(&rc, 0, 39, BLACK);  /* bottom-left */
    check_pixel(&rc, 39, 39, BLACK); /* bottom-right */
    check_pixel(&rc, 20, 20, RED);   /* centre */
    check_pixel(&rc, 20, 0, RED);    /* middle of the flat top edge */
    check_pixel(&rc, 0, 20, RED);    /* middle of the flat left edge */

    mu_soft_render_shutdown(&rc);
}

static void test_rounded_antialiases(void) {
    CASE("rounded edge is antialiased");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 40, 40);
    mu_soft_begin_frame(&rc, BLACK);
    mu_draw_rect(&rc, (MuRect){0.f, 0.f, 40.f, 40.f}, RED, (MuColor){0, 0, 0, 0}, 0.f, 10.f);

    /* A hard-edged rasteriser produces only pure black or pure red. The curve should
     * yield partially-covered pixels somewhere along it. */
    int partial = 0;
    for (int y = 0; y < 40; y++) {
        for (int x = 0; x < 40; x++) {
            MuColor p = mu_soft_get_pixel(&rc, x, y);
            if (p.r > 0 && p.r < 255) partial++;
        }
    }
    CHECK(partial > 0);
    if (!partial) printf("       no partially-covered pixels: edge is not antialiased\n");

    mu_soft_render_shutdown(&rc);
}

static void test_rounded_respects_clip(void) {
    CASE("rounded respects clip");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 48, 48);
    mu_soft_begin_frame(&rc, SENTINEL);

    mu_push_scissor(&rc, (MuRect){12.f, 12.f, 16.f, 16.f});
    mu_draw_rect(&rc, (MuRect){-20.f, -20.f, 200.f, 200.f}, GREEN, RED, 3.f, 14.f);
    mu_pop_scissor(&rc);

    int escaped = 0;
    for (int y = 0; y < 48; y++)
        for (int x = 0; x < 48; x++) {
            int inside = (x >= 12 && x < 28 && y >= 12 && y < 28);
            if (!inside && !color_eq(mu_soft_get_pixel(&rc, x, y), SENTINEL)) escaped++;
        }
    CHECK(escaped == 0);
    if (escaped) printf("       %d pixel(s) escaped the clip region\n", escaped);

    mu_soft_render_shutdown(&rc);
}

static void test_rounded_border_leaves_interior(void) {
    CASE("rounded border leaves interior");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 40, 40);
    mu_soft_begin_frame(&rc, BLACK);

    /* Border only: the ring should be drawn but the middle left untouched. */
    mu_draw_rect(&rc, (MuRect){4.f, 4.f, 32.f, 32.f}, (MuColor){0, 0, 0, 0}, RED, 3.f, 8.f);

    check_pixel(&rc, 20, 20, BLACK); /* interior untouched */
    check_pixel(&rc, 20, 5, RED);    /* top edge band */
    check_pixel(&rc, 20, 34, RED);   /* bottom edge band */
    check_pixel(&rc, 5, 20, RED);    /* left edge band */
    check_pixel(&rc, 34, 20, RED);   /* right edge band */
    check_pixel(&rc, 20, 10, BLACK); /* inside the ring, past its thickness */

    mu_soft_render_shutdown(&rc);
}

static void test_radius_clamped_to_half_extent(void) {
    CASE("radius clamped to half extent");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 40, 40);
    mu_soft_begin_frame(&rc, BLACK);

    /* Absurd radius on a square must degrade to a circle, not invert or escape. */
    mu_draw_rect(&rc, (MuRect){0.f, 0.f, 40.f, 40.f}, RED, (MuColor){0, 0, 0, 0}, 0.f, 999.f);

    check_pixel(&rc, 20, 20, RED);  /* centre filled */
    check_pixel(&rc, 0, 0, BLACK);  /* corners empty */
    check_pixel(&rc, 39, 39, BLACK);
    /* Extremes of the inscribed circle are essentially covered — but not exactly 255:
     * the pixel centre sits just inside a curve, so correct AA gives ~99% coverage. */
    CHECK(mu_soft_get_pixel(&rc, 20, 0).r >= 250);
    CHECK(mu_soft_get_pixel(&rc, 0, 20).r >= 250);

    mu_soft_render_shutdown(&rc);
}

static void test_square_path_stays_crisp(void) {
    CASE("zero radius stays crisp");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 24, 24);
    mu_soft_begin_frame(&rc, BLACK);
    mu_draw_rect(&rc, (MuRect){4.f, 4.f, 10.f, 10.f}, RED, (MuColor){0, 0, 0, 0}, 0.f, 0.f);

    /* No partial coverage anywhere: square rects must not be softened. */
    int partial = 0;
    for (int y = 0; y < 24; y++)
        for (int x = 0; x < 24; x++) {
            MuColor p = mu_soft_get_pixel(&rc, x, y);
            if (p.r > 0 && p.r < 255) partial++;
        }
    CHECK(partial == 0);
    check_pixel(&rc, 4, 4, RED);
    check_pixel(&rc, 13, 13, RED);
    check_pixel(&rc, 3, 4, BLACK);
    check_pixel(&rc, 14, 13, BLACK);

    mu_soft_render_shutdown(&rc);
}

static void test_present_hook(void) {
    CASE("present hook");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 20, 10);
    mu_soft_begin_frame(&rc, RED);

    int w = 0, h = 0, row_bytes = 0;
    const void *px = mu_present_pixel_data(&rc, &w, &h, &row_bytes);
    CHECK(px != NULL);
    CHECK(w == 20);
    CHECK(h == 10);
    CHECK(row_bytes == 20 * (int)sizeof(uint32_t));
    if (px) CHECK(((const uint32_t *)px)[0] == mu_soft_pack(RED));

    mu_soft_render_shutdown(&rc);
}

static void test_borrowed_surface(void) {
    CASE("borrowed surface");
    /* The bare-metal path: render straight into memory we do not own, with padded stride. */
    const int w = 12, h = 6, stride = 16;
    uint32_t *fb = (uint32_t *)calloc((size_t)stride * (size_t)h, sizeof(uint32_t));
    CHECK(fb != NULL);
    if (!fb) return;

    MuRenderContext rc;
    mu_soft_render_init_borrowed(&rc, fb, w, h, stride);
    mu_soft_begin_frame(&rc, GREEN);
    mu_draw_rect(&rc, (MuRect){0.f, 0.f, 4.f, 2.f}, RED, (MuColor){0, 0, 0, 0}, 0.f, 0.f);

    CHECK(fb[0] == mu_soft_pack(RED));
    CHECK(fb[(size_t)1 * stride + 0] == mu_soft_pack(RED));
    CHECK(fb[(size_t)1 * stride + 4] == mu_soft_pack(GREEN));
    /* Padding beyond `width` must never be written. */
    CHECK(fb[(size_t)0 * stride + 13] == 0u);

    mu_soft_render_shutdown(&rc);
    CHECK(fb[0] == mu_soft_pack(RED)); /* shutdown must not free borrowed memory */
    free(fb);
}

int main(void) {
    test_pack_roundtrip();
    test_opaque_fill_bounds();
    test_blend_identity();
    test_blend_midpoint();
    test_scissor_clips();
    test_clip_canary();
    test_clip_stack_balance();
    test_border_only();
    test_rounded_cuts_corners();
    test_rounded_antialiases();
    test_rounded_respects_clip();
    test_rounded_border_leaves_interior();
    test_radius_clamped_to_half_extent();
    test_square_path_stays_crisp();
    test_present_hook();
    test_borrowed_surface();

    if (g_failures) {
        printf("\n%d check(s) failed\n", g_failures);
        return 1;
    }
    printf("all raster checks passed\n");
    return 0;
}
