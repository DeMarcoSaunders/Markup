/*
 * UTF-8 editing tests for the text input widget.
 *
 * Headless: builds a real MuContext with the software backend supplying text metrics,
 * then drives the widget through the public API and the input dispatcher exactly as a
 * backend would. Regression cover for the mixed byte/codepoint editing bug, where
 * mu_textinput_insert_utf8 inserted whole sequences but backspace removed one byte.
 */

#include "markup/mu_core.h"
#include "markup/mu_input.h"
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

static void check_text(const MuNode *in, const char *want) {
    const char *got = mu_textinput_get_text(in);
    if (!got) got = "(null)";
    if (strcmp(got, want) != 0) {
        printf("FAIL [%s] text: got \"%s\" want \"%s\"\n", g_case, got, want);
        g_failures++;
    }
}

/* Multi-byte samples: 2-byte, 3-byte, 4-byte. */
#define E_ACUTE "\xC3\xA9"             /* U+00E9  é  */
#define EURO "\xE2\x82\xAC"            /* U+20AC  €  */
#define EMOJI "\xF0\x9F\x8E\x88"       /* U+1F388 🎈 */

/** Every non-continuation byte must start a valid, complete sequence. */
static bool is_well_formed_utf8(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        int n;
        if (*p < 0x80u) n = 1;
        else if ((*p & 0xE0u) == 0xC0u) n = 2;
        else if ((*p & 0xF0u) == 0xE0u) n = 3;
        else if ((*p & 0xF8u) == 0xF0u) n = 4;
        else return false; /* stray continuation or invalid lead */
        for (int i = 1; i < n; i++)
            if ((p[i] & 0xC0u) != 0x80u) return false;
        p += n;
    }
    return true;
}

typedef struct Fixture {
    MuContext ctx;
    MuRenderContext rc;
    MuNode *root;
    MuNode *input;
} Fixture;

static void fixture_init(Fixture *f, const char *initial) {
    mu_context_init(&f->ctx, 1 << 16);
    mu_style_init(&f->ctx, NULL);
    mu_soft_render_init(&f->rc, 200, 60);
    mu_render_bind_measure(&f->rc);
    mu_widgets_basic_register(&f->ctx);

    f->root = mu_make_panel(&f->ctx, true);
    mu_context_set_root(&f->ctx, f->root);
    f->input = mu_make_textinput(&f->ctx, initial);
    mu_node_add_child(&f->ctx, f->root, f->input);
    f->input->bounds = (MuRect){0.f, 0.f, 200.f, 30.f};
    mu_focus_set(&f->ctx, f->input->id);
}

static void fixture_free(Fixture *f) {
    mu_context_shutdown(&f->ctx);
    mu_soft_render_shutdown(&f->rc);
    mu_style_shutdown(&f->ctx);
}

static void press(Fixture *f, int key) {
    MuKeyEvent ev = {key, true, false};
    mu_input_dispatch_key(&f->ctx, &ev);
}

/* ------------------------------------------------------------------ */

static void test_backspace_two_byte(void) {
    CASE("backspace 2-byte");
    Fixture f;
    fixture_init(&f, "a" E_ACUTE);
    press(&f, MU_KEY_BACKSPACE);
    check_text(f.input, "a");
    CHECK(is_well_formed_utf8(mu_textinput_get_text(f.input)));
    fixture_free(&f);
}

static void test_backspace_three_byte(void) {
    CASE("backspace 3-byte");
    Fixture f;
    fixture_init(&f, "x" EURO);
    press(&f, MU_KEY_BACKSPACE);
    check_text(f.input, "x");
    CHECK(is_well_formed_utf8(mu_textinput_get_text(f.input)));
    fixture_free(&f);
}

static void test_backspace_four_byte(void) {
    CASE("backspace 4-byte");
    Fixture f;
    fixture_init(&f, "hi" EMOJI);
    press(&f, MU_KEY_BACKSPACE);
    check_text(f.input, "hi");
    CHECK(is_well_formed_utf8(mu_textinput_get_text(f.input)));
    fixture_free(&f);
}

static void test_backspace_runs_buffer_empty(void) {
    CASE("backspace to empty stays well-formed");
    Fixture f;
    fixture_init(&f, E_ACUTE EURO EMOJI);
    for (int i = 0; i < 3; i++) {
        press(&f, MU_KEY_BACKSPACE);
        CHECK(is_well_formed_utf8(mu_textinput_get_text(f.input)));
    }
    check_text(f.input, "");
    /* Extra backspaces on an empty buffer must not underflow. */
    press(&f, MU_KEY_BACKSPACE);
    check_text(f.input, "");
    fixture_free(&f);
}

static void test_delete_forward_multibyte(void) {
    CASE("delete forward multi-byte");
    Fixture f;
    fixture_init(&f, EURO "z");
    press(&f, MU_KEY_HOME);
    press(&f, MU_KEY_DELETE);
    check_text(f.input, "z");
    CHECK(is_well_formed_utf8(mu_textinput_get_text(f.input)));
    fixture_free(&f);
}

static void test_arrows_step_whole_characters(void) {
    CASE("arrows step whole characters");
    Fixture f;
    fixture_init(&f, EMOJI EURO E_ACUTE);
    press(&f, MU_KEY_HOME);
    /* One RIGHT should clear the whole 4-byte emoji, so DELETE removes the euro. */
    press(&f, MU_KEY_RIGHT);
    press(&f, MU_KEY_DELETE);
    check_text(f.input, EMOJI E_ACUTE);
    CHECK(is_well_formed_utf8(mu_textinput_get_text(f.input)));

    /* One LEFT from the end should sit before the é, so backspace removes the emoji. */
    press(&f, MU_KEY_END);
    press(&f, MU_KEY_LEFT);
    press(&f, MU_KEY_BACKSPACE);
    check_text(f.input, E_ACUTE);
    CHECK(is_well_formed_utf8(mu_textinput_get_text(f.input)));
    fixture_free(&f);
}

static void test_insert_utf8_then_backspace(void) {
    CASE("insert_utf8 then backspace");
    Fixture f;
    fixture_init(&f, "");
    /* This is the exact path the SDL backend uses for SDL_EVENT_TEXT_INPUT. */
    mu_textinput_insert_utf8(&f.ctx, f.input, EURO);
    check_text(f.input, EURO);
    press(&f, MU_KEY_BACKSPACE);
    check_text(f.input, "");
    fixture_free(&f);
}

static void test_char_path_encodes_non_ascii(void) {
    CASE("codepoint path encodes non-ASCII");
    Fixture f;
    fixture_init(&f, "");
    /* The raylib backend delivers codepoints here; they must be encoded, not dropped. */
    mu_input_dispatch_char(&f.ctx, 0x00E9u); /* é */
    check_text(f.input, E_ACUTE);
    mu_input_dispatch_char(&f.ctx, 0x1F388u); /* 🎈 */
    check_text(f.input, E_ACUTE EMOJI);
    CHECK(is_well_formed_utf8(mu_textinput_get_text(f.input)));

    press(&f, MU_KEY_BACKSPACE);
    check_text(f.input, E_ACUTE);
    fixture_free(&f);
}

static void test_control_and_invalid_codepoints_rejected(void) {
    CASE("control and invalid codepoints rejected");
    Fixture f;
    fixture_init(&f, "ok");
    mu_input_dispatch_char(&f.ctx, 0x09u);   /* tab */
    mu_input_dispatch_char(&f.ctx, 0x7Fu);   /* DEL */
    mu_input_dispatch_char(&f.ctx, 0xD800u); /* lone surrogate */
    mu_input_dispatch_char(&f.ctx, 0x110000u);
    check_text(f.input, "ok");
    fixture_free(&f);
}

static void test_middle_edit_preserves_neighbours(void) {
    CASE("middle edit preserves neighbours");
    Fixture f;
    fixture_init(&f, "a" EURO "b");
    press(&f, MU_KEY_END);
    press(&f, MU_KEY_LEFT);      /* before 'b' */
    press(&f, MU_KEY_BACKSPACE); /* removes the euro, not a fragment of it */
    check_text(f.input, "ab");
    CHECK(is_well_formed_utf8(mu_textinput_get_text(f.input)));
    fixture_free(&f);
}

int main(void) {
    test_backspace_two_byte();
    test_backspace_three_byte();
    test_backspace_four_byte();
    test_backspace_runs_buffer_empty();
    test_delete_forward_multibyte();
    test_arrows_step_whole_characters();
    test_insert_utf8_then_backspace();
    test_char_path_encodes_non_ascii();
    test_control_and_invalid_codepoints_rejected();
    test_middle_edit_preserves_neighbours();

    if (g_failures) {
        printf("\n%d check(s) failed\n", g_failures);
        return 1;
    }
    printf("all textinput checks passed\n");
    return 0;
}
