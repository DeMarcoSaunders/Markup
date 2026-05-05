#include "demo_font.h"
#include "markup/mu_core.h"
#include "markup/mu_layout_flex.h"
#include "markup/mu_raylib.h"
#include "markup/mu_style.h"
#include "markup/mu_widgets_basic.h"
#include <raylib.h>
#include <string.h>

/** Dark tokens (demo). Inter Variable is loaded in demo_font for sharp type. */
static MuStyleModule demo_markup_theme(void) {
    MuStyleModule t;
    memset(&t, 0, sizeof(t));
    t.page_bg = (MuColor){15, 17, 21, 255};
    t.toolbar_bg = (MuColor){18, 21, 27, 255};
    t.toolbar_border = (MuColor){38, 44, 58, 255};
    t.panel_bg = (MuColor){28, 31, 40, 255};
    t.panel_border = (MuColor){48, 54, 70, 255};
    t.modal_overlay = (MuColor){2, 4, 8, 200};
    t.modal_bg = (MuColor){33, 38, 50, 255};
    t.label_fg = (MuColor){232, 236, 244, 255};
    t.muted_fg = (MuColor){142, 152, 176, 255};
    t.button_bg = (MuColor){32, 174, 160, 255};
    t.button_fg = (MuColor){248, 250, 252, 255};
    t.button_border = (MuColor){18, 120, 110, 255};
    t.button_bg_hover = (MuColor){56, 200, 186, 255};
    t.button_fg_hover = (MuColor){255, 255, 255, 255};
    t.button_bg_pressed = (MuColor){22, 140, 128, 255};
    t.input_bg = (MuColor){20, 24, 32, 255};
    t.input_fg = (MuColor){230, 234, 242, 255};
    t.input_border = (MuColor){60, 68, 90, 255};
    t.slider_track = (MuColor){40, 46, 60, 255};
    t.slider_thumb = (MuColor){56, 198, 186, 255};
    t.checkbox_border = (MuColor){100, 110, 140, 255};
    t.checkbox_fill = (MuColor){32, 174, 160, 255};
    t.default_font_size = 17;
    t.default_radius = 8.f;
    return t;
}

static void close_modal(void *ctxv) {
    MuContext *ctx = (MuContext *)ctxv;
    MuNode *modal = (MuNode *)ctx->user_ptr;
    mu_modal_set_visible(ctx, modal, false);
}

static void open_modal(void *ctxv) {
    MuContext *ctx = (MuContext *)ctxv;
    MuNode *modal = (MuNode *)ctx->user_ptr;
    mu_modal_set_visible(ctx, modal, true);
}

static MuNode *make_horizontal_spacer(MuContext *ctx) {
    MuNode *sp = mu_make_panel(ctx, true);
    sp->role = "page";
    sp->flex_grow = 1;
    sp->flex_basis = 10.f;
    return sp;
}

static void mu_demo_panel_tune(MuNode *panel, float gap, float pad_tb, float pad_lr) {
    if (!panel || !panel->state)
        return;
    MuFlexLayoutState *fl = (MuFlexLayoutState *)panel->state;
    fl->gap = gap;
    fl->pad_top = fl->pad_bottom = pad_tb;
    fl->pad_left = fl->pad_right = pad_lr;
}

static void demo_flex_justify_align(MuNode *panel, MuFlexJustify j, MuFlexAlign a) {
    if (!panel || !panel->state)
        return;
    MuFlexLayoutState *fl = (MuFlexLayoutState *)panel->state;
    fl->justify = j;
    fl->align_items = a;
}

int main(void) {
    const int W = 960, H = 640;
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(W, H, "MarkUp v2 — demo minimal");
    SetTargetFPS(60);

    MuContext ctx;
    mu_context_init(&ctx, 65536);
    {
        MuStyleModule skin = demo_markup_theme();
        mu_style_init(&ctx, &skin);
    }
    MuRenderContext rc;
    mu_render_init(&rc);
    demo_font_try_load_ui(&rc, ctx.style);

    mu_widgets_basic_register(&ctx);

    MuNode *root = mu_make_panel(&ctx, true);
    root->role = "page";
    root->bounds = (MuRect){0, 0, (float)W, (float)H};
    mu_demo_panel_tune(root, 0.f, 0.f, 0.f);
    mu_context_set_root(&ctx, root);

    MuNode *toolbar = mu_make_panel(&ctx, false);
    toolbar->role = "toolbar";
    toolbar->flex_grow = 0;
    toolbar->flex_basis = 54.f;
    mu_demo_panel_tune(toolbar, 16.f, 0.f, 28.f);
    demo_flex_justify_align(toolbar, MU_JUSTIFY_START, MU_ALIGN_CENTER);

    MuNode *tb_title = mu_make_label(&ctx, "Markup");
    MuNode *tb_sub = mu_make_label(&ctx, "minimal flex + widgets");
    tb_sub->role = "subtitle";
    mu_node_add_child(&ctx, toolbar, tb_title);
    mu_node_add_child(&ctx, toolbar, tb_sub);

    MuNode *body = mu_make_panel(&ctx, true);
    body->role = "page";
    body->flex_grow = 1;
    demo_flex_justify_align(body, MU_JUSTIFY_CENTER, MU_ALIGN_CENTER);
    mu_demo_panel_tune(body, 0.f, 28.f, 20.f);

    MuNode *row = mu_make_panel(&ctx, false);
    row->role = "page";
    demo_flex_justify_align(row, MU_JUSTIFY_CENTER, MU_ALIGN_STRETCH);
    mu_demo_panel_tune(row, 16.f, 0.f, 0.f);

    MuNode *sp_left = make_horizontal_spacer(&ctx);

    MuNode *card = mu_make_panel(&ctx, true);
    card->role = "panel";
    card->flex_grow = 0;
    card->flex_basis = 480.f;
    mu_demo_panel_tune(card, 18.f, 26.f, 28.f);

    MuNode *sp_right = make_horizontal_spacer(&ctx);

    mu_node_add_child(&ctx, row, sp_left);
    mu_node_add_child(&ctx, row, card);
    mu_node_add_child(&ctx, row, sp_right);
    mu_node_add_child(&ctx, body, row);

    MuNode *sec_title = mu_make_label(&ctx, "Widget gallery");
    mu_node_add_child(&ctx, card, sec_title);

    MuNode *lead = mu_make_label(&ctx, "Flexible layout, pointer routing, and themed controls.");
    lead->role = "subtitle";
    mu_node_add_child(&ctx, card, lead);

    mu_node_add_child(&ctx, card, mu_make_button(&ctx, "Open modal", open_modal, &ctx));
    mu_node_add_child(&ctx, card, mu_make_slider(&ctx, 0, 100, 35));

    MuNode *dd = mu_make_dropdown(&ctx);
    mu_dropdown_add_option(dd, "Ocean");
    mu_dropdown_add_option(dd, "Forest");
    mu_dropdown_add_option(dd, "Aurora");
    mu_node_add_child(&ctx, card, dd);

    mu_node_add_child(&ctx, card, mu_make_textinput(&ctx, "Focused field"));

    MuNode *check_row = mu_make_panel(&ctx, false);
    check_row->role = "page";
    mu_demo_panel_tune(check_row, 12.f, 0.f, 0.f);
    mu_node_add_child(&ctx, check_row, mu_make_checkbox(&ctx, true, NULL, NULL));
    mu_node_add_child(&ctx, check_row, mu_make_label(&ctx, "Enable telemetry (demo)"));
    mu_node_add_child(&ctx, card, check_row);

    mu_node_add_child(&ctx, root, toolbar);
    mu_node_add_child(&ctx, root, body);

    MuNode *toast = mu_make_toast(&ctx, "Notice in the corner");
    mu_node_add_child(&ctx, root, toast);

    MuNode *modal = mu_make_modal(&ctx);
    modal->bounds = (MuRect){0, 0, (float)W, (float)H};
    MuNode *dlg = mu_make_panel(&ctx, true);
    dlg->role = "panel";
    mu_demo_panel_tune(dlg, 18.f, 22.f, 26.f);
    mu_node_add_child(&ctx, dlg, mu_make_label(&ctx, "Modal layer"));
    MuNode *md_sub = mu_make_label(&ctx, "Drawn above the tree with a dimmed backdrop.");
    md_sub->role = "subtitle";
    mu_node_add_child(&ctx, dlg, md_sub);
    mu_node_add_child(&ctx, dlg, mu_make_button(&ctx, "Dismiss", close_modal, &ctx));
    mu_node_add_child(&ctx, modal, dlg);

    mu_modal_bind_layer(&ctx, modal);
    ctx.user_ptr = modal;

    while (!WindowShouldClose()) {
        mu_frame_begin(&ctx);

        float sw = (float)GetScreenWidth();
        float sh = (float)GetScreenHeight();
        root->bounds = (MuRect){0, 0, sw, sh};
        modal->bounds = root->bounds;

        mu_layout_run(&ctx);

        if ((modal->flags & MU_NODE_VISIBLE) && dlg) {
            dlg->bounds = (MuRect){(sw - 400) * 0.5f, (sh - 220) * 0.5f, 400, 220};
            const MuNodeOps *ops = mu_get_node_ops(&ctx, dlg->kind);
            if (ops && ops->layout_children)
                ops->layout_children(&ctx, dlg);
        }

        toast->bounds = (MuRect){sw - 268, 20, 252, 50};

        mu_raylib_frame(&ctx, &rc);

        BeginDrawing();
        if (ctx.style)
            ClearBackground((Color){ctx.style->page_bg.r, ctx.style->page_bg.g, ctx.style->page_bg.b,
                                    ctx.style->page_bg.a});
        else
            ClearBackground(RAYWHITE);

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
