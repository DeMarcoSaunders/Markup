/*
 * Text rasterization tests for the software backend.
 *
 * Headless: loads a real TTF, measures and draws into an offscreen surface, and asserts
 * on the resulting pixels. The font path is passed in by CTest so the test does not
 * depend on the working directory.
 */

#include "markup/mu_soft.h"
#include "markup/mu_text.h"

#include <stdio.h>
#include <string.h>

static int g_failures;
static const char *g_case = "";
static const char *g_font_path;

#define CASE(name) (g_case = (name))

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            printf("FAIL [%s] %s:%d: %s\n", g_case, __FILE__, __LINE__, #cond);                    \
            g_failures++;                                                                          \
        }                                                                                          \
    } while (0)

static const MuColor WHITE = {255, 255, 255, 255};
static const MuColor BLACK = {0, 0, 0, 255};

static MuTextStyle style_at(float size) {
    MuTextStyle s;
    mu_text_style_init(&s);
    s.size = size;
    s.letter_spacing = 0.f;
    return s;
}

/** Count pixels that differ from the cleared background. */
static int ink(const MuRenderContext *rc) {
    int n = 0;
    for (int y = 0; y < 200; y++)
        for (int x = 0; x < 400; x++)
            if (mu_soft_get_pixel(rc, x, y).r > 8) n++;
    return n;
}

/* ------------------------------------------------------------------ */

static void test_font_loads(void) {
    CASE("font loads");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 400, 200);
    CHECK(!mu_soft_has_font(&rc));
    CHECK(mu_soft_set_font_file(&rc, g_font_path));
    CHECK(mu_soft_has_font(&rc));
    mu_soft_render_shutdown(&rc);
}

static void test_missing_font_rejected(void) {
    CASE("missing font rejected");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 400, 200);
    CHECK(!mu_soft_set_font_file(&rc, "no/such/file.ttf"));
    CHECK(!mu_soft_has_font(&rc));
    /* A non-font file must be rejected too, not accepted as garbage. */
    CHECK(!mu_soft_set_font_file(&rc, __FILE__));
    mu_soft_render_shutdown(&rc);
}

static void test_measure_is_proportional(void) {
    CASE("measure is proportional");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 400, 200);
    CHECK(mu_soft_set_font_file(&rc, g_font_path));

    MuTextStyle st = style_at(16.f);
    MuTextMetrics m1 = {0}, m2 = {0}, wide = {0}, narrow = {0}, empty = {0};

    mu_text_measure(&rc, "Hello", &st, &m1);
    mu_text_measure(&rc, "Hello Hello", &st, &m2);
    CHECK(m1.width > 0.f);
    CHECK(m1.height > 0.f);
    CHECK(m2.width > m1.width);

    /* Proportional font: 'W' must be wider than 'i'. An advance-blind stub would tie. */
    mu_text_measure(&rc, "W", &st, &wide);
    mu_text_measure(&rc, "i", &st, &narrow);
    CHECK(wide.width > narrow.width);

    mu_text_measure(&rc, "", &st, &empty);
    CHECK(empty.width == 0.f);
    CHECK(empty.height > 0.f); /* empty line still has height */

    mu_soft_render_shutdown(&rc);
}

static void test_measure_scales_with_size(void) {
    CASE("measure scales with size");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 400, 200);
    CHECK(mu_soft_set_font_file(&rc, g_font_path));

    MuTextStyle small = style_at(12.f), large = style_at(48.f);
    MuTextMetrics ms = {0}, ml = {0};
    mu_text_measure(&rc, "Markup", &small, &ms);
    mu_text_measure(&rc, "Markup", &large, &ml);

    CHECK(ml.width > ms.width * 2.f);
    CHECK(ml.height > ms.height * 2.f);
    mu_soft_render_shutdown(&rc);
}

static void test_draw_produces_ink(void) {
    CASE("draw produces ink");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 400, 200);
    CHECK(mu_soft_set_font_file(&rc, g_font_path));
    mu_soft_begin_frame(&rc, BLACK);

    MuTextStyle st = style_at(32.f);
    mu_draw_text(&rc, "Markup", 20.f, 40.f, &st, WHITE);

    int n = ink(&rc);
    CHECK(n > 100);
    if (n <= 100) printf("       only %d inked pixels — glyphs are not rasterizing\n", n);
    mu_soft_render_shutdown(&rc);
}

static void test_draw_is_antialiased(void) {
    CASE("draw is antialiased");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 400, 200);
    CHECK(mu_soft_set_font_file(&rc, g_font_path));
    mu_soft_begin_frame(&rc, BLACK);

    MuTextStyle st = style_at(32.f);
    mu_draw_text(&rc, "Se", 20.f, 40.f, &st, WHITE);

    int partial = 0;
    for (int y = 0; y < 200; y++)
        for (int x = 0; x < 400; x++) {
            int v = mu_soft_get_pixel(&rc, x, y).r;
            if (v > 8 && v < 247) partial++;
        }
    CHECK(partial > 0);
    mu_soft_render_shutdown(&rc);
}

static void test_no_font_draws_nothing_but_measures(void) {
    CASE("no font: measures, draws nothing");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 400, 200);
    mu_soft_begin_frame(&rc, BLACK);

    MuTextStyle st = style_at(32.f);
    MuTextMetrics m = {0};
    mu_text_measure(&rc, "Markup", &st, &m);
    /* Layout must stay plausible without a font, or every label collapses to zero. */
    CHECK(m.width > 0.f);
    CHECK(m.height > 0.f);

    mu_draw_text(&rc, "Markup", 20.f, 40.f, &st, WHITE);
    CHECK(ink(&rc) == 0);
    mu_soft_render_shutdown(&rc);
}

static void test_draw_respects_clip(void) {
    CASE("draw respects clip");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 400, 200);
    CHECK(mu_soft_set_font_file(&rc, g_font_path));
    mu_soft_begin_frame(&rc, BLACK);

    mu_push_scissor(&rc, (MuRect){50.f, 50.f, 60.f, 30.f});
    MuTextStyle st = style_at(64.f);
    mu_draw_text(&rc, "OVERFLOWING TEXT", 0.f, 0.f, &st, WHITE);
    mu_pop_scissor(&rc);

    int escaped = 0;
    for (int y = 0; y < 200; y++)
        for (int x = 0; x < 400; x++) {
            int inside = (x >= 50 && x < 110 && y >= 50 && y < 80);
            if (!inside && mu_soft_get_pixel(&rc, x, y).r > 8) escaped++;
        }
    CHECK(escaped == 0);
    if (escaped) printf("       %d glyph pixel(s) escaped the clip region\n", escaped);
    mu_soft_render_shutdown(&rc);
}

static void test_utf8_multibyte_renders(void) {
    CASE("utf-8 multi-byte renders");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 400, 200);
    CHECK(mu_soft_set_font_file(&rc, g_font_path));

    MuTextStyle st = style_at(32.f);
    MuTextMetrics ascii = {0}, accented = {0};
    mu_text_measure(&rc, "e", &st, &ascii);
    mu_text_measure(&rc, "\xC3\xA9", &st, &accented); /* é */

    /* Decoded as one codepoint, not two bytes: width should be close to plain 'e'. */
    CHECK(accented.width > 0.f);
    CHECK(accented.width < ascii.width * 1.6f);

    mu_soft_begin_frame(&rc, BLACK);
    mu_draw_text(&rc, "\xE2\x82\xAC\xC3\xA9", 20.f, 40.f, &st, WHITE); /* €é */
    CHECK(ink(&rc) > 50);
    mu_soft_render_shutdown(&rc);
}

static void test_bold_is_wider(void) {
    CASE("synthetic bold is wider");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 400, 200);
    CHECK(mu_soft_set_font_file(&rc, g_font_path));

    MuTextStyle normal = style_at(24.f);
    MuTextStyle bold = style_at(24.f);
    bold.weight = MU_TEXT_WEIGHT_BOLD;

    MuTextMetrics mn = {0}, mb = {0};
    mu_text_measure(&rc, "Weight", &normal, &mn);
    mu_text_measure(&rc, "Weight", &bold, &mb);
    CHECK(mb.width > mn.width);

    mu_soft_begin_frame(&rc, BLACK);
    mu_draw_text(&rc, "Weight", 10.f, 40.f, &normal, WHITE);
    int n_normal = ink(&rc);
    mu_soft_begin_frame(&rc, BLACK);
    mu_draw_text(&rc, "Weight", 10.f, 40.f, &bold, WHITE);
    int n_bold = ink(&rc);
    CHECK(n_bold > n_normal); /* double-struck, so more coverage */

    mu_soft_render_shutdown(&rc);
}

static void test_glyph_cache_reuse(void) {
    CASE("glyph cache reuse");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 400, 200);
    CHECK(mu_soft_set_font_file(&rc, g_font_path));

    /* Same string drawn many times must stay stable — a cache that mis-keys or
     * re-packs would drift. Compare ink counts across repeats. */
    MuTextStyle st = style_at(20.f);
    mu_soft_begin_frame(&rc, BLACK);
    mu_draw_text(&rc, "cache", 10.f, 10.f, &st, WHITE);
    int first = ink(&rc);

    for (int i = 0; i < 50; i++) {
        mu_soft_begin_frame(&rc, BLACK);
        mu_draw_text(&rc, "cache", 10.f, 10.f, &st, WHITE);
    }
    CHECK(ink(&rc) == first);

    /* Many distinct sizes must not corrupt earlier entries. */
    for (int s = 8; s < 40; s++) {
        MuTextStyle sz = style_at((float)s);
        mu_soft_begin_frame(&rc, BLACK);
        mu_draw_text(&rc, "size", 10.f, 10.f, &sz, WHITE);
    }
    mu_soft_begin_frame(&rc, BLACK);
    mu_draw_text(&rc, "cache", 10.f, 10.f, &st, WHITE);
    CHECK(ink(&rc) == first);

    mu_soft_render_shutdown(&rc);
}

static void test_wrapped_text_uses_measure(void) {
    CASE("wrapped text works unchanged");
    MuRenderContext rc;
    mu_soft_render_init(&rc, 400, 200);
    CHECK(mu_soft_set_font_file(&rc, g_font_path));
    mu_render_bind_measure(&rc);

    /* mu_text_layout.c is backend-neutral and builds on mu_text_measure; wrapping to a
     * narrow width must produce a taller, narrower box than a single line. */
    MuTextStyle st = style_at(16.f);
    const char *para = "the quick brown fox jumps over the lazy dog";
    MuTextMetrics one = {0}, wrapped = {0};
    mu_text_measure(&rc, para, &st, &one);
    mu_text_measure_wrapped(&rc, para, &st, 120.f, &wrapped);

    CHECK(wrapped.width <= 120.f);
    CHECK(wrapped.height > one.height);

    mu_soft_render_shutdown(&rc);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: test_text <font.ttf>\n");
        return 2;
    }
    g_font_path = argv[1];

    test_font_loads();
    test_missing_font_rejected();
    test_measure_is_proportional();
    test_measure_scales_with_size();
    test_draw_produces_ink();
    test_draw_is_antialiased();
    test_no_font_draws_nothing_but_measures();
    test_draw_respects_clip();
    test_utf8_multibyte_renders();
    test_bold_is_wider();
    test_glyph_cache_reuse();
    test_wrapped_text_uses_measure();

    if (g_failures) {
        printf("\n%d check(s) failed\n", g_failures);
        return 1;
    }
    printf("all text checks passed\n");
    return 0;
}
