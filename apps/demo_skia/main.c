#include "demo_font_skia.h"
#include "markup_demo_font_config.h"
#include "markup/mu_core.h"
#include "markup/mu_layout_flex.h"
#include "markup/mu_render.h"
#include "markup/mu_sdl.h"
#include "markup/mu_skia.h"
#include "markup/mu_style.h"
#include "markup/mu_text.h"
#include "markup/mu_compose.h"
#include "markup/mu_popup.h"
#include "markup/mu_image.h"
#include "markup/mu_widgets_basic.h"
#include <stdio.h>
#include <string.h>

typedef struct DemoState DemoState;

typedef struct DemoTileUser {
    DemoState *demo;
    const char *name;
} DemoTileUser;

typedef struct DemoState {
    MuContext *ctx;
    MuNode *modal;
    MuNode *toast;
    MuNode *slider;
    MuNode *slider_label;
    MuNode *dropdown;
    MuNode *font_list;
    MuNode *gallery_tabs;
    MuStyleModule theme_a;
    MuStyleModule theme_b;
    bool use_alt_theme;
    int toast_frames;
    int last_dropdown_sel;
    float last_slider_value;
    uint32_t icon_ids[4];
    MuImageSheet icon_sheet;
    DemoTileUser tile_users[4];
} DemoState;

typedef struct DemoAccent {
    MuColor button_bg;
    MuColor button_border;
    MuColor button_bg_hover;
    MuColor button_bg_pressed;
    MuColor slider_thumb;
    MuColor checkbox_fill;
} DemoAccent;

static void demo_theme_struct(MuStyleModule *t) {
    memset(t, 0, sizeof(*t));
    t->page_bg = (MuColor){12, 14, 18, 255};
    t->toolbar_bg = (MuColor){20, 23, 30, 255};
    t->toolbar_border = (MuColor){46, 52, 68, 255};
    t->panel_bg = (MuColor){26, 30, 40, 255};
    t->panel_border = (MuColor){56, 62, 80, 255};
    t->modal_overlay = (MuColor){2, 4, 8, 200};
    t->modal_bg = (MuColor){33, 38, 50, 255};
    t->label_fg = (MuColor){236, 240, 248, 255};
    t->muted_fg = (MuColor){136, 146, 170, 255};
    t->button_fg = (MuColor){248, 250, 252, 255};
    t->button_fg_hover = (MuColor){255, 255, 255, 255};
    t->input_bg = (MuColor){20, 24, 32, 255};
    t->input_fg = (MuColor){230, 234, 242, 255};
    t->input_border = (MuColor){60, 68, 90, 255};
    t->slider_track = (MuColor){40, 46, 60, 255};
    t->checkbox_border = (MuColor){100, 110, 140, 255};
    t->default_font_size = 16;
    t->default_font_weight = MU_TEXT_WEIGHT_NORMAL;
    t->default_letter_spacing = 0.5f;
    t->default_radius = 10.f;
}

static void demo_theme_struct_warm(MuStyleModule *t) {
    demo_theme_struct(t);
    t->page_bg = (MuColor){36, 28, 26, 255};
    t->toolbar_bg = (MuColor){48, 38, 34, 255};
    t->toolbar_border = (MuColor){88, 72, 64, 255};
    t->panel_bg = (MuColor){58, 46, 42, 255};
    t->panel_border = (MuColor){96, 80, 72, 255};
    t->label_fg = (MuColor){252, 244, 236, 255};
    t->muted_fg = (MuColor){196, 176, 164, 255};
    t->input_bg = (MuColor){44, 34, 30, 255};
    t->input_border = (MuColor){110, 88, 78, 255};
    t->modal_bg = (MuColor){52, 40, 36, 255};
}

static DemoAccent demo_palette_accents(int sel) {
    switch (sel) {
    case 1:
        return (DemoAccent){
            {52, 168, 83, 255},   {34, 120, 62, 255},   {78, 196, 110, 255},
            {40, 130, 68, 255},   {78, 196, 110, 255},  {52, 168, 83, 255},
        };
    case 2:
        return (DemoAccent){
            {234, 120, 72, 255},  {180, 84, 40, 255},   {244, 150, 100, 255},
            {200, 96, 52, 255},   {244, 150, 100, 255}, {234, 120, 72, 255},
        };
    case 3:
        return (DemoAccent){
            {148, 156, 170, 255}, {100, 108, 122, 255}, {176, 184, 198, 255},
            {120, 128, 142, 255}, {176, 184, 198, 255}, {148, 156, 170, 255},
        };
    case 0:
    default:
        return (DemoAccent){
            {32, 174, 160, 255},  {18, 120, 110, 255},  {56, 200, 186, 255},
            {22, 140, 128, 255},  {56, 198, 186, 255},  {32, 174, 160, 255},
        };
    }
}

static void demo_apply_accent(MuStyleModule *t, const DemoAccent *a) {
    t->button_bg = a->button_bg;
    t->button_border = a->button_border;
    t->button_bg_hover = a->button_bg_hover;
    t->button_bg_pressed = a->button_bg_pressed;
    t->slider_thumb = a->slider_thumb;
    t->checkbox_fill = a->checkbox_fill;
}

static void demo_rebuild_themes(DemoState *d) {
    if (!d) return;
    demo_theme_struct(&d->theme_a);
    demo_theme_struct_warm(&d->theme_b);
    DemoAccent accent = demo_palette_accents(d->last_dropdown_sel);
    demo_apply_accent(&d->theme_a, &accent);
    demo_apply_accent(&d->theme_b, &accent);
}

static void demo_show_toast(DemoState *d, const char *msg) {
    if (!d || !d->toast) return;
    mu_toast_set_message(d->toast, msg);
    d->toast_frames = 150;
    d->toast->flags |= MU_NODE_VISIBLE;
    mu_layout_mark_dirty(d->toast);
}

static void demo_apply_theme(DemoState *d) {
    if (!d || !d->ctx) return;
    demo_rebuild_themes(d);
    mu_style_init(d->ctx, d->use_alt_theme ? &d->theme_b : &d->theme_a);
    if (d->ctx->root) mu_layout_mark_dirty(d->ctx->root);
}

static void demo_apply_palette(DemoState *d, int sel) {
    if (!d) return;
    d->last_dropdown_sel = sel;
    demo_apply_theme(d);
}

static void demo_toggle_theme(void *user) {
    DemoState *d = (DemoState *)user;
    d->use_alt_theme = !d->use_alt_theme;
    demo_apply_theme(d);
    demo_show_toast(d, d->use_alt_theme ? "Warm theme" : "Teal theme");
}

static void demo_toast_btn(void *user) {
    demo_show_toast((DemoState *)user, "Toast overlay — non-blocking feedback");
}

static void demo_open_modal(void *user) {
    DemoState *d = (DemoState *)user;
    mu_modal_set_visible(d->ctx, d->modal, true);
    demo_show_toast(d, "Modal opened");
}

static void demo_close_modal(void *user) {
    DemoState *d = (DemoState *)user;
    mu_modal_set_visible(d->ctx, d->modal, false);
}

static void demo_check_toggle(void *user, bool on) {
    DemoState *d = (DemoState *)user;
    demo_show_toast(d, on ? "Grid overlay enabled" : "Grid overlay off");
}

static void demo_tile_click(void *user) {
    DemoTileUser *tu = (DemoTileUser *)user;
    if (!tu || !tu->demo) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "Launch %s", tu->name ? tu->name : "App");
    demo_show_toast(tu->demo, buf);
}

static void demo_asset_path(char *out, size_t cap, const char *filename) {
    if (!out || cap == 0 || !filename) return;
    const char *base = MARKUP_DEMO_FONT_FILE_ABS;
    const char *slash = strrchr(base, '/');
    if (!slash) slash = strrchr(base, '\\');
    size_t dir_len = slash ? (size_t)(slash - base + 1) : 0;
    snprintf(out, cap, "%.*s%s", (int)dir_len, base, filename);
}

static uint32_t demo_load_image(MuRenderContext *rc, const char *rel_path, const char *filename) {
    uint32_t id = mu_image_load_file(rc, rel_path);
    if (id != MU_IMAGE_INVALID) return id;
    char abs[512];
    demo_asset_path(abs, sizeof(abs), filename);
    return mu_image_load_file(rc, abs);
}

static bool demo_load_icons(MuRenderContext *rc, DemoState *d) {
    static const char *files[] = {"icon_files.png", "icon_browser.png", "icon_settings.png", "icon_terminal.png"};
    static const char *rels[] = {
        "assets/icon_files.png",
        "assets/icon_browser.png",
        "assets/icon_settings.png",
        "assets/icon_terminal.png",
    };
    if (!rc || !d) return false;

    bool ok = true;
    for (int i = 0; i < 4; i++) {
        d->icon_ids[i] = demo_load_image(rc, rels[i], files[i]);
        if (d->icon_ids[i] == MU_IMAGE_INVALID) ok = false;
    }

    mu_image_sheet_init(&d->icon_sheet);
    if (!mu_image_sheet_load(&d->icon_sheet, rc, "assets/app_icons_sheet.png", 128.f, 128.f, 4)) {
        char abs[512];
        demo_asset_path(abs, sizeof(abs), "app_icons_sheet.png");
        mu_image_sheet_load(&d->icon_sheet, rc, abs, 128.f, 128.f, 4);
    }
    return ok;
}

static void demo_fill_launcher_icons(DemoState *d, MuIconDesc *out, int count, float size, bool labels) {
    static const char *names[] = {"Files", "Browser", "Settings", "Terminal"};
    if (!d || !out || count <= 0) return;
    if (count > 4) count = 4;
    for (int i = 0; i < count; i++) {
        mu_icon_desc_init(&out[i]);
        out[i].image_id = d->icon_ids[i];
        out[i].size = size;
        out[i].radius = size >= 40.f ? 12.f : 6.f;
        if (labels) out[i].label = names[i];
        d->tile_users[i].demo = d;
        d->tile_users[i].name = names[i];
        out[i].on_click = demo_tile_click;
        out[i].user = &d->tile_users[i];
    }
}

static MuNode *demo_taskbar_strip(MuContext *ctx, DemoState *d) {
    MuIconDesc items[4];
    demo_fill_launcher_icons(d, items, 4, 28.f, false);
    return mu_compose_icon_strip(ctx, NULL, items, 4, 6.f);
}

static MuNode *demo_taskbar(MuContext *ctx, DemoState *d) {
    MuNode *bar = mu_make_panel(ctx, false);
    bar->role = "toolbar";
    mu_layout_set_flex(bar, 0.f, 0.f, 52.f);
    mu_layout_set_gap(bar, 8.f);
    mu_layout_set_padding(bar, 6.f, 28.f, 6.f, 28.f);
    mu_layout_set_align_items(bar, MU_ALIGN_CENTER);
    mu_layout_set_justify(bar, MU_JUSTIFY_CENTER);
    mu_node_add_child(ctx, bar, demo_taskbar_strip(ctx, d));
    return bar;
}

static MuNode *demo_spacer(MuContext *ctx) {
    MuNode *sp = mu_make_panel(ctx, true);
    sp->role = "group";
    mu_layout_set_flex(sp, 1.f, 1.f, 0.f);
    return sp;
}

static MuNode *demo_brand(MuContext *ctx) {
    MuNode *brand = mu_make_panel(ctx, true);
    brand->role = "group";
    mu_layout_set_gap(brand, 2.f);
    mu_layout_set_padding_all(brand, 0.f);
    mu_layout_set_flex(brand, 0.f, 1.f, MU_FLEX_BASIS_AUTO);

    MuNode *title = mu_make_label(ctx, "Markup");
    title->role = "heading";
    mu_node_add_child(ctx, brand, title);

    MuNode *tag = mu_make_label(ctx, "Skia + SDL3 component gallery");
    tag->role = "subtitle";
    mu_node_add_child(ctx, brand, tag);
    return brand;
}

static void demo_fill_controls(MuContext *ctx, MuNode *parent, DemoState *d) {
    MuNode *hint = mu_make_label(ctx, "Palette changes accent color. Slider adjusts panel opacity.");
    hint->role = "subtitle";
    mu_node_add_child(ctx, parent, hint);

    MuNode *field = mu_make_textinput(ctx, "");
    mu_textinput_set_placeholder(field, "Type here…");
    mu_node_add_child(ctx, parent, field);

    MuNode *palette_lbl = mu_make_label(ctx, "Palette");
    palette_lbl->role = "subtitle";
    mu_node_add_child(ctx, parent, palette_lbl);

    MuNode *check_row = mu_make_panel(ctx, false);
    check_row->role = "group";
    mu_layout_set_gap(check_row, 10.f);
    mu_layout_set_align_items(check_row, MU_ALIGN_CENTER);
    mu_layout_set_padding_all(check_row, 0.f);
    mu_node_add_child(ctx, check_row, mu_make_checkbox(ctx, false, demo_check_toggle, d));
    MuNode *check_lbl = mu_make_label(ctx, "Show grid overlay");
    mu_node_add_child(ctx, check_row, check_lbl);
    mu_node_add_child(ctx, parent, check_row);

    d->dropdown = mu_make_dropdown(ctx);
    mu_dropdown_add_option(d->dropdown, "Ocean");
    mu_dropdown_add_option(d->dropdown, "Forest");
    mu_dropdown_add_option(d->dropdown, "Sunset");
    mu_dropdown_add_option(d->dropdown, "Mono");
    mu_node_add_child(ctx, parent, d->dropdown);

    d->slider = mu_make_slider(ctx, 0, 100, 42);
    mu_node_add_child(ctx, parent, d->slider);

    d->slider_label = mu_make_label(ctx, "Opacity: 42%");
    d->slider_label->role = "subtitle";
    mu_node_add_child(ctx, parent, d->slider_label);

    MuNode *btn = mu_make_button(ctx, "Open modal", demo_open_modal, d);
    mu_layout_set_margin(btn, 4.f, 0.f, 0.f, 0.f);
    mu_node_add_child(ctx, parent, btn);
}

static void demo_fill_apps(MuContext *ctx, MuNode *parent, DemoState *d) {
    MuNode *hint = mu_make_label(ctx, "App icons — tiles from PNGs, carousel from a sprite sheet.");
    hint->role = "subtitle";
    mu_node_add_child(ctx, parent, hint);

    MuIconDesc tiles[4];
    demo_fill_launcher_icons(d, tiles, 4, 48.f, true);

    MuNode *flow = mu_compose_flow(ctx, 16.f);
    for (int i = 0; i < 4; i++) {
        MuNode *tile = mu_compose_icon_chip(ctx, NULL, &tiles[i]);
        if (tile) {
            mu_layout_set_min_size(tile, 80.f, 0.f);
            tile->role = "tile";
            mu_node_add_child(ctx, flow, tile);
        }
    }
    mu_node_add_child(ctx, parent, flow);

    MuNode *carousel_hint = mu_make_label(ctx, "Carousel — scroll horizontally or drag the scrollbar.");
    carousel_hint->role = "subtitle";
    mu_node_add_child(ctx, parent, carousel_hint);

    MuIconDesc carousel_items[6];
    static const char *names[] = {"Files", "Browser", "Settings", "Terminal", "Files", "Browser"};
    for (int i = 0; i < 6; i++) {
        mu_icon_desc_init(&carousel_items[i]);
        int j = i % 4;
        if (d->icon_sheet.image_id != MU_IMAGE_INVALID) {
            carousel_items[i].image_id = d->icon_sheet.image_id;
            carousel_items[i].src = mu_image_sheet_cell(&d->icon_sheet, j);
        } else {
            carousel_items[i].image_id = d->icon_ids[j];
        }
        carousel_items[i].label = names[i];
        carousel_items[i].size = 52.f;
        carousel_items[i].radius = 10.f;
        d->tile_users[j].demo = d;
        d->tile_users[j].name = names[j];
        carousel_items[i].on_click = demo_tile_click;
        carousel_items[i].user = &d->tile_users[j];
    }
    MuNode *carousel = mu_compose_icon_carousel(ctx, NULL, carousel_items, 6, 52.f, 360.f);
    mu_node_add_child(ctx, parent, carousel);
}

static void demo_list_change(void *user, int index) {
    DemoState *d = (DemoState *)user;
    if (!d || !d->font_list || index < 0) return;
    char buf[80];
    snprintf(buf, sizeof(buf), "Selected: %s", mu_list_item_text(d->font_list, index));
    demo_show_toast(d, buf);
}

static void demo_fill_typography(MuContext *ctx, MuNode *parent, DemoState *demo, uint32_t mono_font) {
    MuNode *hint = mu_make_label(ctx, "Click a row to select. Scroll with the wheel or scrollbar.");
    hint->role = "subtitle";
    mu_node_add_child(ctx, parent, hint);

    MuNode *list = mu_compose_list(ctx, 200.f);
    mu_node_add_child(ctx, parent, list);
    if (demo) {
        demo->font_list = list;
        mu_list_set_on_change(list, demo_list_change, demo);
    }

    mu_list_add_item(ctx, list, "Regular body text at 16px");
    mu_list_add_item(ctx, list, "Semibold emphasis");
    mu_list_add_item(ctx, list, "Italic secondary voice");
    for (int i = 1; i <= 24; i++) {
        char line[48];
        snprintf(line, sizeof(line), "Scrollable list item %d", i);
        mu_list_add_item(ctx, list, line);
    }
    if (mono_font != MU_FONT_DEFAULT) mu_list_add_item(ctx, list, "ui.monospace.render()");
}

static void demo_tabs_change(void *user, int index) {
    DemoState *d = (DemoState *)user;
    if (!d || !d->gallery_tabs || index < 0) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "Tab: %s", mu_tabs_label(d->gallery_tabs, index));
    demo_show_toast(d, buf);
}

static MuNode *demo_gallery_tabs(MuContext *ctx, DemoState *demo, uint32_t mono_font) {
    MuNode *tabs = mu_compose_tabs(ctx);
    if (!tabs) return NULL;
    if (demo) {
        demo->gallery_tabs = tabs;
        mu_tabs_set_on_change(tabs, demo_tabs_change, demo);
    }

    MuNode *controls = mu_tabs_add(ctx, tabs, "Controls");
    demo_fill_controls(ctx, controls, demo);

    MuNode *apps = mu_tabs_add(ctx, tabs, "Launcher");
    demo_fill_apps(ctx, apps, demo);

    MuNode *type = mu_tabs_add(ctx, tabs, "Typography");
    demo_fill_typography(ctx, type, demo, mono_font);

    return tabs;
}

static void demo_sync_controls(DemoState *d) {
    if (!d) return;

    float v = mu_slider_get_value(d->slider);
    if (v != d->last_slider_value) {
        d->last_slider_value = v;
        char buf[48];
        snprintf(buf, sizeof(buf), "Opacity: %.0f%%", v);
        mu_label_set_text(d->slider_label, buf);
    }
    if (d->ctx && d->ctx->style) {
        uint8_t alpha = (uint8_t)(80.f + (v / 100.f) * 175.f);
        d->ctx->style->panel_bg.a = alpha;
        d->ctx->style->modal_bg.a = alpha;
    }

    int sel = mu_dropdown_get_selection(d->dropdown);
    if (sel >= 0 && sel != d->last_dropdown_sel) {
        demo_apply_palette(d, sel);
        char buf[64];
        snprintf(buf, sizeof(buf), "Accent: %s", mu_dropdown_selected_text(d->dropdown));
        demo_show_toast(d, buf);
    }

    if (d->toast_frames > 0) {
        d->toast_frames--;
        if (d->toast_frames == 0) d->toast->flags &= ~MU_NODE_VISIBLE;
    }
}

int main(void) {
    const int W = 1024, H = 680;

    MuSdlApp *app = mu_sdl_create("MarkUp v2 — Skia + SDL3", W, H);
    if (!app) return 1;

    MuContext ctx;
    mu_context_init(&ctx, 65536);

    DemoState demo;
    memset(&demo, 0, sizeof(demo));
    demo.ctx = &ctx;
    demo_theme_struct(&demo.theme_a);
    demo_theme_struct_warm(&demo.theme_b);
    demo.last_dropdown_sel = 0;
    demo.last_slider_value = 42.f;
    demo_rebuild_themes(&demo);
    mu_style_init(&ctx, &demo.theme_a);

    MuRenderContext rc;
    mu_skia_render_init(&rc, W, H);
    demo_font_skia_try_load(&rc, ctx.style);
    mu_render_bind_measure(&rc);

    uint32_t mono_font = mu_font_load_file(&rc, "assets/InterVariable.ttf", "Inter");
    if (mono_font == MU_FONT_DEFAULT)
        mono_font = mu_font_load_file(&rc, MARKUP_DEMO_FONT_FILE_ABS, "Inter");

    if (!demo_load_icons(&rc, &demo))
        fprintf(stderr, "demo_skia: warning — some icon assets failed to load (run from exe dir or rebuild)\n");

    mu_widgets_basic_register(&ctx);

    MuNode *popup_layer = mu_make_popup_layer(&ctx);
    mu_popup_bind_layer(&ctx, popup_layer);

    MuNode *root = mu_make_panel(&ctx, true);
    root->role = "page";
    mu_node_set_bounds(root, (MuRect){0, 0, (float)W, (float)H});
    mu_layout_set_padding_all(root, 0.f);
    mu_layout_set_gap(root, 0.f);
    mu_context_set_root(&ctx, root);

    MuNode *toolbar = mu_make_panel(&ctx, false);
    toolbar->role = "toolbar";
    mu_layout_set_flex(toolbar, 0.f, 0.f, 64.f);
    mu_layout_set_gap(toolbar, 10.f);
    mu_layout_set_padding(toolbar, 0.f, 28.f, 0.f, 28.f);
    mu_layout_set_align_items(toolbar, MU_ALIGN_CENTER);
    mu_node_add_child(&ctx, toolbar, demo_brand(&ctx));
    mu_node_add_child(&ctx, toolbar, demo_spacer(&ctx));
    mu_node_add_child(&ctx, toolbar, mu_make_button(&ctx, "Theme", demo_toggle_theme, &demo));
    mu_node_add_child(&ctx, toolbar, mu_make_button(&ctx, "Toast", demo_toast_btn, &demo));

    MuNode *body = mu_make_panel(&ctx, true);
    body->role = "page";
    mu_layout_set_flex(body, 1.f, 1.f, 0.f);
    mu_layout_set_padding(body, 20.f, 28.f, 28.f, 28.f);

    mu_node_add_child(&ctx, body, demo_gallery_tabs(&ctx, &demo, mono_font));
    mu_node_add_child(&ctx, root, toolbar);
    mu_node_add_child(&ctx, root, body);
    mu_node_add_child(&ctx, root, demo_taskbar(&ctx, &demo));

    demo.toast = mu_make_toast(&ctx, "");
    demo.toast->flags &= ~MU_NODE_VISIBLE;
    mu_node_add_child(&ctx, root, demo.toast);

    demo.modal = mu_make_modal(&ctx);
    mu_node_set_bounds(demo.modal, (MuRect){0, 0, (float)W, (float)H});
    MuNode *dlg = mu_make_panel(&ctx, true);
    dlg->role = "panel";
    mu_layout_set_gap(dlg, 14.f);
    mu_layout_set_padding(dlg, 24.f, 28.f, 24.f, 28.f);
    MuNode *dlg_title = mu_make_label(&ctx, "Modal layer");
    dlg_title->role = "heading";
    mu_node_add_child(&ctx, dlg, dlg_title);
    MuNode *dlg_sub = mu_make_label(&ctx, "Painted above the tree with a dimmed backdrop.");
    dlg_sub->role = "subtitle";
    mu_node_add_child(&ctx, dlg, dlg_sub);
    mu_node_add_child(&ctx, dlg, mu_make_button(&ctx, "Dismiss", demo_close_modal, &demo));
    mu_node_add_child(&ctx, demo.modal, dlg);
    mu_modal_bind_layer(&ctx, demo.modal);

    while (mu_sdl_poll(app, &ctx)) {
        mu_frame_begin(&ctx);

        int sw = mu_sdl_width(app);
        int sh = mu_sdl_height(app);
        mu_skia_resize(&rc, sw, sh);
        mu_node_set_bounds(root, (MuRect){0, 0, (float)sw, (float)sh});
        mu_node_set_bounds(demo.modal, root->bounds);

        demo_sync_controls(&demo);

        mu_layout_run(&ctx);
        mu_popups_sync(&ctx);

        if ((demo.modal->flags & MU_NODE_VISIBLE) && dlg)
            mu_layout_node(&ctx, dlg);

        if (demo.toast->flags & MU_NODE_VISIBLE) {
            mu_layout_node(&ctx, demo.toast);
            float tw = demo.toast->bounds.w > 0.f ? demo.toast->bounds.w : 260.f;
            float th = demo.toast->bounds.h > 0.f ? demo.toast->bounds.h : 44.f;
            const float taskbar_h = 52.f;
            const float margin = 10.f;
            mu_node_set_bounds(demo.toast,
                               (MuRect){(sw - tw) * 0.5f, (float)sh - taskbar_h - th - margin, tw, th});
        }

        if ((demo.modal->flags & MU_NODE_VISIBLE) && dlg)
            mu_node_set_bounds(dlg, (MuRect){(sw - 400) * 0.5f, (sh - 240) * 0.5f, 400, 240});

        mu_sdl_frame(&ctx, app);

        MuColor clear = ctx.style ? ctx.style->page_bg : (MuColor){12, 14, 18, 255};
        mu_skia_begin_frame(&rc, clear);
        mu_paint_all(&ctx, &rc);
        mu_skia_end_frame(&rc);
        mu_sdl_present(app, &rc);

        mu_frame_end(&ctx);
    }

    mu_skia_render_shutdown(&rc);
    mu_style_shutdown(&ctx);
    mu_context_shutdown(&ctx);
    mu_sdl_destroy(app);
    return 0;
}
