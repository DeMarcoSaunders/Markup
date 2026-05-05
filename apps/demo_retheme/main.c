#include "demo_font.h"
#include "markup/mu_core.h"
#include "markup/mu_style.h"
#include "markup/mu_layout_flex.h"
#include "markup/mu_raylib.h"
#include "markup/mu_widgets_basic.h"
#include <raylib.h>
#include <stdio.h>
#include <string.h>

static MuStyleModule g_theme_a;
static MuStyleModule g_theme_b;
static bool g_use_alt;

static void build_themes(void) {
    memset(&g_theme_a, 0, sizeof(g_theme_a));
    g_theme_a.page_bg = (MuColor){248, 250, 252, 255};
    g_theme_a.panel_bg = (MuColor){255, 255, 255, 255};
    g_theme_a.panel_border = (MuColor){229, 231, 235, 255};
    g_theme_a.label_fg = (MuColor){31, 41, 55, 255};
    g_theme_a.button_bg = (MuColor){59, 130, 246, 255};
    g_theme_a.button_fg = (MuColor){255, 255, 255, 255};
    g_theme_a.button_border = (MuColor){37, 99, 235, 255};
    g_theme_a.button_bg_hover = (MuColor){96, 165, 250, 255};
    g_theme_a.button_fg_hover = (MuColor){255, 255, 255, 255};
    g_theme_a.button_bg_pressed = (MuColor){37, 99, 235, 255};
    g_theme_a.input_bg = (MuColor){255, 255, 255, 255};
    g_theme_a.input_fg = (MuColor){31, 41, 55, 255};
    g_theme_a.input_border = (MuColor){209, 213, 219, 255};
    g_theme_a.slider_track = (MuColor){229, 231, 235, 255};
    g_theme_a.slider_thumb = (MuColor){59, 130, 246, 255};
    g_theme_a.checkbox_border = (MuColor){107, 114, 128, 255};
    g_theme_a.checkbox_fill = (MuColor){59, 130, 246, 255};
    g_theme_a.modal_bg = (MuColor){255, 255, 255, 255};
    g_theme_a.modal_overlay = (MuColor){0, 0, 0, 160};
    g_theme_a.toolbar_bg = (MuColor){243, 244, 246, 255};
    g_theme_a.toolbar_border = (MuColor){209, 213, 219, 255};
    g_theme_a.muted_fg = (MuColor){107, 114, 128, 255};
    g_theme_a.default_font_size = 16;
    g_theme_a.default_radius = 8;

    g_theme_b = g_theme_a;
    /* Alt “theme pack” — warm accent, darker page */
    g_theme_b.page_bg = (MuColor){44, 36, 33, 255};
    g_theme_b.label_fg = (MuColor){253, 246, 240, 255};
    g_theme_b.panel_bg = (MuColor){62, 52, 48, 255};
    g_theme_b.panel_border = (MuColor){92, 80, 74, 255};
    g_theme_b.button_bg = (MuColor){234, 88, 12, 255};
    g_theme_b.button_border = (MuColor){194, 65, 12, 255};
    g_theme_b.button_bg_hover = (MuColor){251, 146, 60, 255};
    g_theme_b.button_bg_pressed = (MuColor){194, 65, 12, 255};
    g_theme_b.input_bg = (MuColor){55, 48, 44, 255};
    g_theme_b.input_fg = g_theme_b.label_fg;
    g_theme_b.input_border = (MuColor){120, 100, 90, 255};
    g_theme_b.slider_thumb = (MuColor){234, 88, 12, 255};
    g_theme_b.slider_track = (MuColor){80, 70, 64, 255};
    g_theme_b.checkbox_fill = (MuColor){234, 88, 12, 255};
    g_theme_b.modal_bg = (MuColor){55, 48, 44, 255};
    g_theme_b.toolbar_bg = (MuColor){50, 42, 38, 255};
    g_theme_b.toolbar_border = (MuColor){90, 78, 70, 255};
    g_theme_b.muted_fg = (MuColor){215, 200, 190, 255};
}

static void toggle_theme(void *ctxv) {
    MuContext *ctx = (MuContext *)ctxv;
    g_use_alt = !g_use_alt;
    mu_style_init(ctx, g_use_alt ? &g_theme_b : &g_theme_a);
}

int main(void) {
    const int W = 880, H = 560;
    InitWindow(W, H, "MarkUp v2 — demo_retheme");
    SetTargetFPS(60);

    build_themes();

    MuContext ctx;
    mu_context_init(&ctx, 65536);
    mu_style_init(&ctx, &g_theme_a);
    MuRenderContext rc;
    mu_render_init(&rc);
    demo_font_try_load_ui(&rc, ctx.style);

    mu_widgets_basic_register(&ctx);

    MuNode *root = mu_make_panel(&ctx, true);
    root->role = "page";
    root->bounds = (MuRect){0, 0, (float)W, (float)H};
    mu_context_set_root(&ctx, root);

    mu_node_add_child(&ctx, root, mu_make_label(&ctx, "Same tree — swap token bundle via mu_style_init"));
    mu_node_add_child(&ctx, root, mu_make_button(&ctx, "Toggle theme", toggle_theme, &ctx));
    mu_node_add_child(&ctx, root, mu_make_slider(&ctx, 0, 100, 60));

    while (!WindowShouldClose()) {
        mu_frame_begin(&ctx);
        float sw = (float)GetScreenWidth();
        float sh = (float)GetScreenHeight();
        root->bounds = (MuRect){0, 0, sw, sh};
        mu_layout_run(&ctx);
        mu_raylib_frame(&ctx, &rc);

        BeginDrawing();
        if (ctx.style)
            ClearBackground((Color){ctx.style->page_bg.r, ctx.style->page_bg.g, ctx.style->page_bg.b,
                                    ctx.style->page_bg.a});
        else
            ClearBackground(BLACK);

        mu_paint_all(&ctx, &rc);
        EndDrawing();
        mu_frame_end(&ctx);
    }

    mu_render_shutdown(&rc);
    mu_style_shutdown(&ctx);
    mu_context_shutdown(&ctx);
    CloseWindow();
    return 0;
}
