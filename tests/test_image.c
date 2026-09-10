/*
 * Image blit tests for the software backend.
 *
 * Images are built in memory rather than loaded from disk: the expected pixel values
 * are then exact, and the tests run with no PNG decoder present (which is also the
 * bare-metal situation).
 */

#include "markup/mu_image.h"
#include "markup/mu_soft.h"

#include <stdio.h>
#include <stdlib.h>
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

static const MuColor BLACK = {0, 0, 0, 255};
static const MuColor WHITE = {255, 255, 255, 255};

static int near_i(int a, int b, int tol) {
    int d = a - b;
    return (d < 0 ? -d : d) <= tol;
}

static void check_px(const MuRenderContext *rc, int x, int y, MuColor want, int tol) {
    MuColor got = mu_soft_get_pixel(rc, x, y);
    if (!near_i(got.r, want.r, tol) || !near_i(got.g, want.g, tol) || !near_i(got.b, want.b, tol)) {
        printf("FAIL [%s] pixel (%d,%d): got {%u,%u,%u} want ~{%u,%u,%u} (tol %d)\n", g_case, x, y, got.r,
               got.g, got.b, want.r, want.g, want.b, tol);
        g_failures++;
    }
}

/** 2x2: red, green / blue, white. */
static uint32_t make_quad(MuRenderContext *rc) {
    static const uint8_t px[] = {
        255, 0, 0, 255,  0, 255, 0, 255,
        0, 0, 255, 255,  255, 255, 255, 255,
    };
    return mu_soft_image_from_rgba(rc, px, 2, 2, 0);
}

/** 4x1 strip of distinct opaque colours, for source-rect tests. */
static uint32_t make_strip(MuRenderContext *rc) {
    static const uint8_t px[] = {
        255, 0, 0, 255,  0, 255, 0, 255,  0, 0, 255, 255,  255, 255, 0, 255,
    };
    return mu_soft_image_from_rgba(rc, px, 4, 1, 0);
}

/* ------------------------------------------------------------------ */

static void test_from_rgba_registers_size(void) {
    CASE("from_rgba registers size");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 64, 64);

    uint32_t id = make_quad(&rc);
    CHECK(id != MU_IMAGE_INVALID);

    /* Regression: the loader used to read width/height after freeing the decode buffer,
     * registering every image as 0x0 so nothing ever drew. */
    float w = -1.f, h = -1.f;
    CHECK(mu_image_get_size(&rc, id, &w, &h));
    CHECK(w == 2.f);
    CHECK(h == 2.f);

    CHECK(mu_soft_image_from_rgba(&rc, NULL, 2, 2, 0) == MU_IMAGE_INVALID);
    CHECK(!mu_image_get_size(&rc, MU_IMAGE_INVALID, &w, &h));
    mu_soft_render_shutdown(&rc);
}

static void test_fill_maps_quadrants(void) {
    CASE("fill maps quadrants");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 64, 64);
    uint32_t id = make_quad(&rc);
    mu_soft_begin_frame(&rc, BLACK);

    MuDrawImageOpts o;
    mu_draw_image_opts_init(&o);
    o.fit = MU_IMAGE_FIT_FILL;
    mu_draw_image(&rc, id, (MuRect){0.f, 0.f, 40.f, 40.f}, &o);

    /* Well inside each quadrant, bilinear has converged to the source texel. */
    check_px(&rc, 3, 3, (MuColor){255, 0, 0, 255}, 6);
    check_px(&rc, 36, 3, (MuColor){0, 255, 0, 255}, 6);
    check_px(&rc, 3, 36, (MuColor){0, 0, 255, 255}, 6);
    check_px(&rc, 36, 36, WHITE, 6);
    mu_soft_render_shutdown(&rc);
}

static void test_bilinear_interpolates(void) {
    CASE("bilinear interpolates");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 64, 64);
    uint32_t id = make_quad(&rc);
    mu_soft_begin_frame(&rc, BLACK);

    MuDrawImageOpts o;
    mu_draw_image_opts_init(&o);
    o.fit = MU_IMAGE_FIT_FILL;
    mu_draw_image(&rc, id, (MuRect){0.f, 0.f, 40.f, 40.f}, &o);

    /* Midway between red and green must be a blend of both, not either one.
     * Nearest-neighbour sampling would snap to a pure source colour and fail. */
    MuColor mid = mu_soft_get_pixel(&rc, 20, 3);
    CHECK(mid.r > 40 && mid.r < 215);
    CHECK(mid.g > 40 && mid.g < 215);
    mu_soft_render_shutdown(&rc);
}

static void test_tint_modulates(void) {
    CASE("tint modulates");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 64, 64);
    uint32_t id = make_quad(&rc);

    MuDrawImageOpts o;
    mu_draw_image_opts_init(&o);
    o.fit = MU_IMAGE_FIT_FILL;

    /* White source quadrant under a half-red tint should lose green and blue. */
    o.tint = (MuColor){255, 0, 0, 255};
    mu_soft_begin_frame(&rc, BLACK);
    mu_draw_image(&rc, id, (MuRect){0.f, 0.f, 40.f, 40.f}, &o);
    MuColor t = mu_soft_get_pixel(&rc, 36, 36);
    CHECK(t.r > 200);
    CHECK(t.g < 20);
    CHECK(t.b < 20);

    /* A fully transparent tint must draw nothing at all. */
    o.tint = (MuColor){255, 255, 255, 0};
    mu_soft_begin_frame(&rc, BLACK);
    mu_draw_image(&rc, id, (MuRect){0.f, 0.f, 40.f, 40.f}, &o);
    check_px(&rc, 20, 20, BLACK, 0);
    mu_soft_render_shutdown(&rc);
}

static void test_alpha_blends(void) {
    CASE("alpha blends over destination");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 32, 32);

    /* Single half-transparent white texel over an opaque black background. */
    static const uint8_t px[] = {255, 255, 255, 128};
    uint32_t id = mu_soft_image_from_rgba(&rc, px, 1, 1, 0);
    mu_soft_begin_frame(&rc, BLACK);

    MuDrawImageOpts o;
    mu_draw_image_opts_init(&o);
    o.fit = MU_IMAGE_FIT_FILL;
    mu_draw_image(&rc, id, (MuRect){0.f, 0.f, 32.f, 32.f}, &o);

    check_px(&rc, 16, 16, (MuColor){128, 128, 128, 255}, 2);
    mu_soft_render_shutdown(&rc);
}

static void test_contain_letterboxes(void) {
    CASE("contain letterboxes");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 80, 80);
    uint32_t id = make_quad(&rc);
    mu_soft_begin_frame(&rc, BLACK);

    MuDrawImageOpts o;
    mu_draw_image_opts_init(&o);
    o.fit = MU_IMAGE_FIT_CONTAIN;
    /* Square source into a wide box: bars left and right, image centred. */
    mu_draw_image(&rc, id, (MuRect){0.f, 0.f, 80.f, 40.f}, &o);

    check_px(&rc, 2, 20, BLACK, 0);  /* left bar untouched */
    check_px(&rc, 77, 20, BLACK, 0); /* right bar untouched */
    CHECK(mu_soft_get_pixel(&rc, 40, 20).r > 0 || mu_soft_get_pixel(&rc, 40, 20).b > 0);
    mu_soft_render_shutdown(&rc);
}

static void test_cover_stays_inside_bounds(void) {
    CASE("cover stays inside bounds");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 80, 80);
    uint32_t id = make_quad(&rc);
    mu_soft_begin_frame(&rc, BLACK);

    MuDrawImageOpts o;
    mu_draw_image_opts_init(&o);
    o.fit = MU_IMAGE_FIT_COVER;
    /* COVER scales past the box on one axis; nothing may paint outside it. */
    mu_draw_image(&rc, id, (MuRect){20.f, 30.f, 40.f, 20.f}, &o);

    int escaped = 0;
    for (int y = 0; y < 80; y++)
        for (int x = 0; x < 80; x++) {
            int inside = (x >= 20 && x < 60 && y >= 30 && y < 50);
            MuColor p = mu_soft_get_pixel(&rc, x, y);
            if (!inside && (p.r || p.g || p.b)) escaped++;
        }
    CHECK(escaped == 0);
    if (escaped) printf("       %d pixel(s) painted outside dst\n", escaped);
    mu_soft_render_shutdown(&rc);
}

static void test_src_rect_does_not_bleed(void) {
    CASE("source rect does not bleed");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 64, 64);
    uint32_t id = make_strip(&rc);
    mu_soft_begin_frame(&rc, BLACK);

    MuDrawImageOpts o;
    mu_draw_image_opts_init(&o);
    o.fit = MU_IMAGE_FIT_FILL;
    o.src = (MuRect){1.f, 0.f, 1.f, 1.f}; /* the green cell only */
    mu_draw_image(&rc, id, (MuRect){0.f, 0.f, 40.f, 40.f}, &o);

    /* Sampling must clamp to the cell: no red from the left, no blue from the right.
     * This is the sprite-sheet bleeding case. */
    for (int x = 0; x < 40; x += 4) {
        MuColor p = mu_soft_get_pixel(&rc, x, 20);
        CHECK(p.g > 200);
        CHECK(p.r < 12);
        CHECK(p.b < 12);
    }
    mu_soft_render_shutdown(&rc);
}

static void test_radius_rounds_corners(void) {
    CASE("radius rounds corners");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 64, 64);
    uint32_t id = make_quad(&rc);
    mu_soft_begin_frame(&rc, BLACK);

    MuDrawImageOpts o;
    mu_draw_image_opts_init(&o);
    o.fit = MU_IMAGE_FIT_FILL;
    o.radius = 14.f;
    mu_draw_image(&rc, id, (MuRect){0.f, 0.f, 40.f, 40.f}, &o);

    check_px(&rc, 0, 0, BLACK, 0);   /* corner cut away */
    check_px(&rc, 39, 39, BLACK, 0);
    CHECK(mu_soft_get_pixel(&rc, 20, 20).r > 0 || mu_soft_get_pixel(&rc, 20, 20).b > 0);
    mu_soft_render_shutdown(&rc);
}

static void test_respects_clip(void) {
    CASE("respects clip");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 64, 64);
    uint32_t id = make_quad(&rc);
    mu_soft_begin_frame(&rc, BLACK);

    mu_push_scissor(&rc, (MuRect){10.f, 10.f, 12.f, 12.f});
    MuDrawImageOpts o;
    mu_draw_image_opts_init(&o);
    o.fit = MU_IMAGE_FIT_FILL;
    mu_draw_image(&rc, id, (MuRect){-50.f, -50.f, 300.f, 300.f}, &o);
    mu_pop_scissor(&rc);

    int escaped = 0;
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            int inside = (x >= 10 && x < 22 && y >= 10 && y < 22);
            MuColor p = mu_soft_get_pixel(&rc, x, y);
            if (!inside && (p.r || p.g || p.b)) escaped++;
        }
    CHECK(escaped == 0);
    if (escaped) printf("       %d pixel(s) escaped the clip region\n", escaped);
    mu_soft_render_shutdown(&rc);
}

static void test_invalid_inputs_are_safe(void) {
    CASE("invalid inputs are safe");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 32, 32);
    uint32_t id = make_quad(&rc);
    mu_soft_begin_frame(&rc, BLACK);

    /* None of these may write anything or crash. */
    mu_draw_image(&rc, MU_IMAGE_INVALID, (MuRect){0, 0, 10, 10}, NULL);
    mu_draw_image(&rc, 9999u, (MuRect){0, 0, 10, 10}, NULL);
    mu_draw_image(&rc, id, (MuRect){0, 0, 0, 0}, NULL);
    mu_draw_image(&rc, id, (MuRect){0, 0, -5, -5}, NULL);
    check_px(&rc, 5, 5, BLACK, 0);

    /* NULL opts must fall back to defaults and still draw. */
    mu_draw_image(&rc, id, (MuRect){0, 0, 32, 32}, NULL);
    CHECK(mu_soft_get_pixel(&rc, 16, 16).r > 0 || mu_soft_get_pixel(&rc, 16, 16).b > 0);
    mu_soft_render_shutdown(&rc);
}

/*
 * Decoding a real file off disk. Separate from the in-memory tests because it is the
 * path that silently did nothing when libpng was absent: mu_image_decode_rgba_file
 * returned false without opening anything, so every load failed identically whether the
 * file was missing, corrupt, or perfectly fine.
 */
static void test_decodes_real_png(const char *png_path) {
    CASE("decodes a real png from disk");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 64, 64);

    uint32_t id = mu_image_load_file(&rc, png_path);
    CHECK(id != MU_IMAGE_INVALID);
    if (id == MU_IMAGE_INVALID) {
        const char *why = mu_image_decode_last_error();
        printf("       load failed: %s\n", why ? why : "no reason recorded");
    } else {
        float w = 0.f, h = 0.f;
        CHECK(mu_image_get_size(&rc, id, &w, &h));
        CHECK(w > 0.f && h > 0.f);
    }
    mu_soft_render_shutdown(&rc);
}

static void test_decode_failures_are_explained(void) {
    CASE("decode failures are explained");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 64, 64);

    /* A missing file and a non-image must both fail, and must be distinguishable —
     * MU_IMAGE_INVALID alone cannot tell them apart. */
    CHECK(mu_image_load_file(&rc, "definitely/not/here.png") == MU_IMAGE_INVALID);
    const char *missing = mu_image_decode_last_error();
    CHECK(missing != NULL);

    CHECK(mu_image_load_file(&rc, __FILE__) == MU_IMAGE_INVALID);
    const char *not_image = mu_image_decode_last_error();
    CHECK(not_image != NULL);

    if (missing && not_image) CHECK(strcmp(missing, not_image) != 0);
    mu_soft_render_shutdown(&rc);
}

int main(int argc, char **argv) {
    test_from_rgba_registers_size();
    test_fill_maps_quadrants();
    test_bilinear_interpolates();
    test_tint_modulates();
    test_alpha_blends();
    test_contain_letterboxes();
    test_cover_stays_inside_bounds();
    test_src_rect_does_not_bleed();
    test_radius_rounds_corners();
    test_respects_clip();
    test_invalid_inputs_are_safe();
    test_decode_failures_are_explained();
    if (argc > 1) test_decodes_real_png(argv[1]);

    if (g_failures) {
        printf("\n%d check(s) failed\n", g_failures);
        return 1;
    }
    printf("all image checks passed\n");
    return 0;
}
