/*
 * demo_soft — the software rasterizer in a window.
 *
 * Renders with markup_soft: no GPU, no OpenGL, no Skia. Every pixel is produced by
 * mu_soft.c on the CPU and blitted to an SDL texture, which is the same path a
 * bare-metal framebuffer target will take.
 *
 * Text does not render yet (Phase 3 adds stb_truetype), so this is deliberately a
 * *chrome* gallery: panels, buttons, checkboxes, sliders and scroll areas at a spread
 * of corner radii and border widths, which is exactly what Phase 2 needs to show off.
 * Labels and button captions occupy correct space but paint nothing.
 *
 * Press ESC or close the window to quit.
 *
 * Run with `--shot <file.bmp>` to render a single frame, write it out, and exit. The
 * software backend has no GPU state, so that snapshot is exactly what the window shows —
 * which makes it usable for visual review and for CI without a display.
 */

#include "markup/mu_anim.h"
#include "markup/mu_compose.h"
#include "markup/mu_core.h"
#include "markup/mu_image.h"
#include "markup/mu_input.h"
#include "markup/mu_layout_flex.h"
#include "markup/mu_popup.h"
#include "markup/mu_render.h"
#include "markup/mu_sdl.h"
#include "markup/mu_soft.h"
#include "markup/mu_style.h"
#include "markup/mu_widgets_basic.h"

#include "markup_demo_font_config.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Write the current software surface out as a BMP. Returns false on failure. */
static bool save_shot(MuRenderContext *rc, const char *path) {
    int w = 0, h = 0, pitch = 0;
    const void *pixels = mu_present_pixel_data(rc, &w, &h, &pitch);
    if (!pixels || w <= 0 || h <= 0) return false;

    SDL_Surface *surf =
        SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ARGB8888, (void *)pixels, pitch);
    if (!surf) return false;
    bool ok = SDL_SaveBMP(surf, path);
    SDL_DestroySurface(surf);
    return ok;
}

typedef struct DemoState {
    MuContext *ctx;
    int clicks;
    bool dark;
    MuAnimator *animator;
    MuNode *card;
    int card_target;
    MuNode *btn_primary;
    MuNode *btn_secondary;
    MuRect btn_primary_rest;
    MuRect btn_secondary_rest;
    MuNode *scroll;
    MuSpring scroll_bounce;
} DemoState;

static void on_click(void *user) {
    DemoState *d = (DemoState *)user;
    if (d) d->clicks++;
}

static void on_toggle(void *user, bool on) {
    (void)user;
    (void)on;
}

/* Fixed corners the card cycles through, clear of the toolbar and the glass panel so the
 * two effects stay easy to tell apart. */
#define CARD_W 160.f
#define CARD_H 100.f
static const MuRect CARD_TARGETS[] = {
    {50.f, 70.f, CARD_W, CARD_H},
    {890.f, 70.f, CARD_W, CARD_H},
    {890.f, 530.f, CARD_W, CARD_H},
    {50.f, 530.f, CARD_W, CARD_H},
};
#define CARD_TARGET_COUNT (int)(sizeof(CARD_TARGETS) / sizeof(CARD_TARGETS[0]))

/* Springing rather than jumping is the entire demo, so this retargets an existing
 * animation instead of teleporting: mu_anim_to keeps the current velocity, which is what
 * makes clicking again mid-flight redirect smoothly instead of restarting. */
static void on_card_click(void *user) {
    DemoState *d = (DemoState *)user;
    if (!d || !d->card) return;
    d->card_target = (d->card_target + 1) % CARD_TARGET_COUNT;
    mu_anim_to(d->animator, d->card, CARD_TARGETS[d->card_target], MU_SPRING_RESPONSE,
               MU_SPRING_BOUNCY);
}

/*
 * Hover lift — an ordinary flex-managed button springs a few pixels up while hovered.
 *
 * Unlike the card, this node's resting position genuinely belongs to layout — but layout
 * in this codebase is dirty-flag-gated (mu_layout_node bails out unless
 * MU_NODE_LAYOUT_DIRTY is set), not recomputed unconditionally every frame. Once the
 * button is first laid out, mu_layout_run never touches it again on its own, and
 * mu_anim_advance writes node->bounds directly rather than through mu_node_set_bounds, so
 * nothing ever re-dirties it either. That means node->bounds stops being a trustworthy
 * "resting position" the moment this node has animated even once: reading it back as the
 * base for the next target would read the *previous frame's already-lifted* value, lift it
 * again, and again every frame after — a runaway that never settles rather than a hover
 * effect. (Caught exactly that way: the shot capture never reached rest.)
 *
 * The fix is to capture the true resting rect once — before this node has ever animated —
 * and always compute the target from that stable copy, never from live node->bounds.
 *
 * Call once per liftable node per frame, after mu_sdl_frame (so the hover flag is current)
 * and before mu_anim_advance.
 */
#define HOVER_LIFT_PX 4.f
#define HOVER_LIFT_RESPONSE 0.15f

static void hover_lift(MuAnimator *anim, MuNode *node, MuRect rest) {
    if (!node) return;
    MuRect target = rest;
    if (node->flags & MU_NODE_HOVERED) target.y -= HOVER_LIFT_PX;
    mu_anim_to(anim, node, target, HOVER_LIFT_RESPONSE, MU_SPRING_SNAPPY);
}

/*
 * Scroll rubber-banding — wheel or thumb-drag input that overruns the content springs
 * back instead of clamping dead.
 *
 * This is a plain MuSpring, not MuAnimator: the thing being animated is one scalar (a
 * visual y-offset layered on top of the clamped scroll position), not a node's bounds.
 * mu_scroll.c has no idea any of this is happening — mu_scroll_get_excess only reports how
 * much of the last offset change its own clamp absorbed, which is ordinary bookkeeping the
 * clamp already had to do, not an animation hook. Everything about the spring — the
 * resistance curve, the cap, the bounce back to rest — lives here.
 *
 * Content's bounds are only trustworthy as "the clamped resting position" for one instant:
 * right after mu_scroll_set_offset runs, since nothing re-derives them afterward (the same
 * dirty-gating lesson hover_lift hit). Calling mu_scroll_set_offset with the scroll's own
 * unchanged offset re-triggers that positioning on demand, which is what makes it safe to
 * lay the bounce on top every single frame rather than only on frames something moved. The
 * nudge has to be followed by a real re-layout of the content's children, not just a write
 * to content->bounds — bounds in this codebase are absolute, not parent-relative, so a
 * child positioned before the nudge stays exactly where it was unless flex runs again.
 */
#define SCROLL_BOUNCE_RESISTANCE 0.5f
#define SCROLL_BOUNCE_MAX 40.f
#define SCROLL_BOUNCE_RESPONSE 0.3f

static void scroll_bounce_advance(MuContext *ctx, MuNode *scroll, MuSpring *bounce, float dt) {
    if (!scroll) return;

    float excess_x = 0.f, excess_y = 0.f;
    mu_scroll_get_excess(scroll, &excess_x, &excess_y);
    if (excess_y != 0.f) {
        float v = bounce->value + excess_y * SCROLL_BOUNCE_RESISTANCE;
        if (v > SCROLL_BOUNCE_MAX) v = SCROLL_BOUNCE_MAX;
        if (v < -SCROLL_BOUNCE_MAX) v = -SCROLL_BOUNCE_MAX;
        bounce->value = v;
    }
    mu_spring_step(bounce, dt);

    float sx, sy;
    mu_scroll_get_offset(scroll, &sx, &sy);
    mu_scroll_set_offset(ctx, scroll, sx, sy);

    MuNode *content = mu_scroll_content(scroll);
    if (content && bounce->value != 0.f) {
        content->bounds.y -= bounce->value;
        mu_layout_flex_run(ctx, content);
    }
}

/*
 * Radius swatch — a custom node kind.
 *
 * The style system resolves corner radius from a node's role, so stock widgets cannot
 * vary it per instance. Rather than fake it, this registers a node kind that owns its
 * own radius and border width and paints itself, which is also a compact demonstration
 * of the plugin registry: no core file knows this type exists.
 */

typedef struct SwatchState {
    float radius;
    float border_w;
} SwatchState;

static uint32_t k_swatch;

static void swatch_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void swatch_measure(MuContext *ctx, MuNode *node, MuVec2 avail, MuVec2 *out) {
    (void)ctx;
    (void)node;
    (void)avail;
    out->x = 96.f;
    out->y = 96.f;
}

static void swatch_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    SwatchState *s = (SwatchState *)node->state;
    if (!s) return;
    MuStyleSnapshot st;
    mu_style_resolve(ctx, node, &st);
    mu_draw_rect(rc, node->bounds, st.background, st.border, s->border_w, s->radius);
}

static const MuNodeOps swatch_ops = {
    .destroy_state = swatch_destroy,
    .measure = swatch_measure,
    .layout_children = NULL,
    .paint = swatch_paint,
    .hit_test = NULL,
    .on_pointer = NULL,
    .on_key = NULL,
    .on_char = NULL,
};

static MuNode *make_swatch(MuContext *ctx, float radius, float border_w) {
    SwatchState *s = (SwatchState *)calloc(1, sizeof(SwatchState));
    if (!s) return NULL;
    s->radius = radius;
    s->border_w = border_w;
    MuNode *n = mu_node_create(ctx, k_swatch, s);
    if (!n) {
        free(s);
        return NULL;
    }
    n->role = "tile";
    mu_layout_set_min_size(n, 96.f, 96.f);
    mu_layout_set_max_size(n, 96.f, 96.f);
    return n;
}

/*
 * A procedurally-built texture, so the demo needs no PNG decoder — which is also how a
 * bare-metal target would supply images. Checkerboard for interpolation, plus a radial
 * alpha falloff so blending against the background is visible.
 */
static uint32_t make_demo_texture(MuRenderContext *rc) {
    enum { N = 16 };
    static uint8_t px[N * N * 4];
    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            uint8_t *p = &px[(y * N + x) * 4];
            bool light = ((x / 2) + (y / 2)) % 2 == 0;
            p[0] = (uint8_t)(light ? 240 : 40 + x * 12);
            p[1] = (uint8_t)(light ? 90 + y * 8 : 120);
            p[2] = (uint8_t)(light ? 60 : 200);

            /* Falloff kept out past the inscribed circle so the edges stay opaque and
             * fit modes and corner rounding remain readable; only the corners fade. */
            float dx = ((float)x - (N - 1) * 0.5f) / (N * 0.5f);
            float dy = ((float)y - (N - 1) * 0.5f) / (N * 0.5f);
            float d = dx * dx + dy * dy;
            float a = 1.f - (d - 1.15f) * 1.8f;
            if (a > 1.f) a = 1.f;
            if (a < 0.f) a = 0.f;
            p[3] = (uint8_t)(a * 255.f + 0.5f);
        }
    }
    return mu_soft_image_from_rgba(rc, px, N, N, 0);
}

/** One image node, sized to a fixed box. */
static MuNode *image_cell(MuContext *ctx, uint32_t id, MuImageFit fit, MuColor tint, float radius,
                          float w, float h) {
    MuNode *n = mu_make_image(ctx, id);
    if (!n) return NULL;
    mu_image_set_fit(n, fit);
    mu_image_set_tint(n, tint);
    mu_image_set_radius(n, radius);
    mu_layout_set_min_size(n, w, h);
    mu_layout_set_max_size(n, w, h);
    return n;
}

/** Fit modes, tint and corner rounding side by side. */
static MuNode *image_row(MuContext *ctx, uint32_t id) {
    const MuColor plain = {255, 255, 255, 255};

    MuNode *row = mu_make_panel(ctx, false);
    row->role = "group";
    mu_layout_set_flex_direction(row, MU_FLEX_ROW);
    mu_layout_set_gap(row, 14.f);
    mu_layout_set_padding_all(row, 0.f);
    mu_layout_set_align_items(row, MU_ALIGN_CENTER);

    mu_node_add_child(ctx, row, image_cell(ctx, id, MU_IMAGE_FIT_FILL, plain, 0.f, 96.f, 96.f));
    mu_node_add_child(ctx, row, image_cell(ctx, id, MU_IMAGE_FIT_CONTAIN, plain, 0.f, 150.f, 96.f));
    mu_node_add_child(ctx, row, image_cell(ctx, id, MU_IMAGE_FIT_COVER, plain, 0.f, 150.f, 96.f));
    mu_node_add_child(ctx, row,
                      image_cell(ctx, id, MU_IMAGE_FIT_FILL, (MuColor){120, 180, 255, 255}, 0.f, 96.f, 96.f));
    mu_node_add_child(ctx, row, image_cell(ctx, id, MU_IMAGE_FIT_FILL, plain, 24.f, 96.f, 96.f));
    return row;
}

/** Sweeps corner radius and border width so the SDF path is visible at a glance. */
static MuNode *radius_sweep(MuContext *ctx) {
    MuNode *row = mu_make_panel(ctx, false);
    row->role = "group";
    mu_layout_set_flex_direction(row, MU_FLEX_ROW);
    mu_layout_set_gap(row, 14.f);
    mu_layout_set_padding_all(row, 0.f);
    mu_layout_set_align_items(row, MU_ALIGN_CENTER);

    static const float radii[] = {0.f, 3.f, 8.f, 16.f, 32.f, 48.f};
    static const float borders[] = {1.f, 1.f, 2.f, 3.f, 5.f, 8.f};
    for (int i = 0; i < (int)(sizeof(radii) / sizeof(radii[0])); i++) {
        MuNode *sw = make_swatch(ctx, radii[i], borders[i]);
        if (sw) mu_node_add_child(ctx, row, sw);
    }
    return row;
}

static MuNode *controls_column(MuContext *ctx, DemoState *d) {
    MuNode *col = mu_make_panel(ctx, true);
    col->role = "panel";
    mu_layout_set_gap(col, 12.f);
    mu_layout_set_flex(col, 1.f, 1.f, 0.f);

    mu_node_add_child(ctx, col, mu_make_label(ctx, "Controls"));
    d->btn_primary = mu_make_button(ctx, "Primary", on_click, d);
    d->btn_secondary = mu_make_button(ctx, "Secondary", on_click, d);
    mu_node_add_child(ctx, col, d->btn_primary);
    mu_node_add_child(ctx, col, d->btn_secondary);
    /* Checkboxes are square: without this the column's ALIGN_STRETCH would smear them
     * across the full width. */
    MuNode *cb_on = mu_make_checkbox(ctx, true, on_toggle, d);
    MuNode *cb_off = mu_make_checkbox(ctx, false, on_toggle, d);
    mu_layout_set_align_self(cb_on, MU_ALIGN_SELF_START);
    mu_layout_set_align_self(cb_off, MU_ALIGN_SELF_START);
    mu_node_add_child(ctx, col, cb_on);
    mu_node_add_child(ctx, col, cb_off);
    mu_node_add_child(ctx, col, mu_make_slider(ctx, 0.f, 100.f, 42.f));
    mu_node_add_child(ctx, col, mu_make_slider(ctx, 0.f, 100.f, 78.f));

    MuNode *input = mu_make_textinput(ctx, "editable");
    mu_node_add_child(ctx, col, input);
    return col;
}

static MuNode *scroll_column(MuContext *ctx, DemoState *d) {
    MuNode *panel = mu_make_panel(ctx, true);
    panel->role = "panel";
    mu_layout_set_gap(panel, 8.f);
    mu_layout_set_flex(panel, 1.f, 1.f, 0.f);
    mu_node_add_child(ctx, panel, mu_make_label(ctx, "Scroll"));

    MuNode *scroll = mu_make_scroll(ctx, MU_SCROLL_VERTICAL);
    d->scroll = scroll;
    mu_layout_set_flex(scroll, 1.f, 1.f, 0.f);
    MuNode *content = mu_scroll_content(scroll);
    if (content) {
        mu_layout_set_gap(content, 6.f);
        for (int i = 0; i < 14; i++) {
            MuNode *item = mu_make_panel(ctx, false);
            item->role = "tile";
            mu_layout_set_min_size(item, 0.f, 34.f);
            mu_layout_set_padding_all(item, 0.f);
            mu_node_add_child(ctx, content, item);
        }
    }
    mu_node_add_child(ctx, panel, scroll);
    return panel;
}

int main(int argc, char **argv) {
    const int W = 1100, H = 700;

    const char *shot_path = NULL;
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "--shot") == 0) shot_path = argv[i + 1];
    }

    MuSdlApp *app = mu_sdl_create("Markup — software rasterizer", W, H);
    if (!app) {
        fprintf(stderr, "demo_soft: failed to create SDL window\n");
        return 1;
    }

    MuContext ctx;
    mu_context_init(&ctx, 1 << 20);
    mu_style_init(&ctx, NULL);

    MuRenderContext rc;
    mu_soft_render_init(&rc, W, H);
    mu_render_bind_measure(&rc);

    /* Assets are copied next to the executable; fall back to the source tree when run
     * from elsewhere. Without a font, layout still works but nothing paints. */
    if (!mu_soft_set_font_file(&rc, "assets/InterVariable.ttf"))
        mu_soft_set_font_file(&rc, MARKUP_DEMO_FONT_FILE_ABS);
    if (!mu_soft_has_font(&rc))
        fprintf(stderr, "demo_soft: no font loaded — text will not render\n");

    mu_widgets_basic_register(&ctx);
    k_swatch = mu_register_node_kind(&ctx, "swatch", &swatch_ops);
    MuNode *popup_layer = mu_make_popup_layer(&ctx);
    mu_popup_bind_layer(&ctx, popup_layer);

    DemoState demo = {0};
    demo.ctx = &ctx;
    demo.animator = mu_anim_create();
    mu_spring_init(&demo.scroll_bounce, 0.f, SCROLL_BOUNCE_RESPONSE, MU_SPRING_SNAPPY);

    MuNode *root = mu_make_panel(&ctx, true);
    root->role = "page";
    mu_layout_set_padding_all(root, 0.f);
    mu_layout_set_gap(root, 0.f);
    mu_context_set_root(&ctx, root);

    MuNode *toolbar = mu_make_panel(&ctx, false);
    toolbar->role = "toolbar";
    mu_layout_set_flex(toolbar, 0.f, 0.f, 56.f);
    mu_layout_set_padding(toolbar, 0.f, 20.f, 0.f, 20.f);
    mu_layout_set_align_items(toolbar, MU_ALIGN_CENTER);
    mu_layout_set_gap(toolbar, 10.f);
    mu_node_add_child(&ctx, toolbar, mu_make_label(&ctx, "demo_soft"));
    mu_node_add_child(&ctx, root, toolbar);

    MuNode *body = mu_make_panel(&ctx, true);
    body->role = "page";
    mu_layout_set_flex(body, 1.f, 1.f, 0.f);
    mu_layout_set_padding_all(body, 20.f);
    mu_layout_set_gap(body, 16.f);

    mu_node_add_child(&ctx, body, radius_sweep(&ctx));
    mu_node_add_child(&ctx, body, image_row(&ctx, make_demo_texture(&rc)));

    MuNode *cols = mu_make_panel(&ctx, false);
    cols->role = "group";
    mu_layout_set_flex_direction(cols, MU_FLEX_ROW);
    mu_layout_set_flex(cols, 1.f, 1.f, 0.f);
    mu_layout_set_gap(cols, 16.f);
    mu_layout_set_padding_all(cols, 0.f);
    mu_node_add_child(&ctx, cols, controls_column(&ctx, &demo));
    mu_node_add_child(&ctx, cols, scroll_column(&ctx, &demo));
    mu_node_add_child(&ctx, body, cols);

    mu_node_add_child(&ctx, root, body);

    /*
     * Frosted glass overlay.
     *
     * Parked on the modal layer rather than in the flex tree: that layer is painted last
     * (so everything it blurs has already been drawn) and is not laid out by flex, so the
     * panel can be positioned freely over the content below.
     */
    MuNode *overlay = mu_make_panel(&ctx, true);
    overlay->role = "group";
    mu_layout_set_padding_all(overlay, 0.f);
    mu_modal_bind_layer(&ctx, overlay);

    MuNode *glass = mu_make_panel(&ctx, true);
    glass->role = "glass";
    mu_node_set_backdrop_blur(glass, 16.f);
    mu_layout_set_padding_all(glass, 18.f);
    mu_layout_set_gap(glass, 6.f);
    mu_node_add_child(&ctx, overlay, glass);
    mu_node_add_child(&ctx, glass, mu_make_label(&ctx, "Frosted glass"));
    mu_node_add_child(&ctx, glass, mu_make_label(&ctx, "backdrop blur, 16px"));

    /*
     * Spring-animated card, added after glass so it paints on top and stays crisp rather
     * than feeding the blur. It lives on the same modal layer for the same reason glass
     * does: mu_layout_run never reaches this layer, so its bounds are entirely the
     * animator's to write, with nothing to fight it for control.
     */
    demo.card = mu_make_button(&ctx, "Spring me", on_card_click, &demo);
    mu_layout_set_min_size(demo.card, CARD_W, CARD_H);
    mu_layout_set_max_size(demo.card, CARD_W, CARD_H);
    mu_node_set_bounds(demo.card, CARD_TARGETS[0]);
    mu_node_add_child(&ctx, overlay, demo.card);

    int last_w = 0, last_h = 0;
    uint64_t prev_ticks = SDL_GetTicksNS();
    /* True whenever the buttons' flex-resting bounds need (re)capturing: initially, and
     * after any resize reflows the column. See hover_lift's comment for why this cannot
     * just be read from node->bounds on demand instead. */
    bool capture_rest = true;
    while (mu_sdl_poll(app, &ctx)) {
        mu_frame_begin(&ctx);

        int sw = mu_sdl_width(app);
        int sh = mu_sdl_height(app);
        if (sw != last_w || sh != last_h) {
            /* Resizing throws away the surface contents, so nothing survives to reuse. */
            mu_soft_resize(&rc, sw, sh);
            mu_node_set_bounds(root, (MuRect){0.f, 0.f, (float)sw, (float)sh});
            mu_damage_all(&ctx);
            last_w = sw;
            last_h = sh;
            capture_rest = true;
        }

        mu_layout_run(&ctx);

        if (capture_rest) {
            demo.btn_primary_rest = demo.btn_primary->bounds;
            demo.btn_secondary_rest = demo.btn_secondary->bounds;
            capture_rest = false;
        }

        /* The overlay layer is outside the flex tree, so position it by hand and lay out
         * its contents. Straddling the swatch and image rows makes the blur obvious. */
        mu_node_set_bounds(overlay, (MuRect){0.f, 0.f, (float)sw, (float)sh});
        mu_node_set_bounds(glass, (MuRect){360.f, 140.f, 330.f, 150.f});
        mu_layout_node(&ctx, glass);

        uint64_t now = SDL_GetTicksNS();
        float dt = (float)((double)(now - prev_ticks) / 1e9);
        prev_ticks = now;
        if (shot_path) dt = 1.f / 60.f; /* deterministic when capturing */

        mu_popups_sync(&ctx);
        mu_sdl_frame(&ctx, app);

        /* Headless capture has no real cursor, so force the hover it wants to demonstrate
         * rather than leaving the lift untested. mu_sdl_frame just cleared and recomputed
         * both of these from the real mouse; this overrides that for the shot only. Same
         * idea for the scroll: a real wheel event past the bottom, fired once. */
        if (shot_path) {
            demo.btn_primary->flags |= MU_NODE_HOVERED;
            ctx.hovered_id = demo.btn_primary->id;

            static bool scroll_kicked = false;
            if (!scroll_kicked) {
                mu_scroll_by(&ctx, demo.scroll, 0.f, -9999.f);
                scroll_kicked = true;
            }
        }
        hover_lift(demo.animator, demo.btn_primary, demo.btn_primary_rest);
        hover_lift(demo.animator, demo.btn_secondary, demo.btn_secondary_rest);

        mu_anim_advance(&ctx, demo.animator, dt);
        scroll_bounce_advance(&ctx, demo.scroll, &demo.scroll_bounce, dt);

        /* mu_paint_all collects damage; ask for it afterwards, then clear and upload
         * only that region. An idle frame damages nothing and does no pixel work. */
        MuColor clear = ctx.style ? ctx.style->page_bg : (MuColor){12, 14, 18, 255};
        mu_damage_collect(&ctx);
        if (!mu_damage_empty(&ctx)) {
            MuRect area = mu_damage_rect(&ctx);
            mu_soft_begin_frame_rect(&rc, clear, area);
            mu_paint_damaged(&ctx, &rc);
            mu_sdl_present_rect(app, &rc, area);
        }
        mu_damage_reset(&ctx);

        mu_frame_end(&ctx);

        if (shot_path) {
            /* Click and overscroll once at the start, then keep stepping so the shot shows
             * every spring having actually gone somewhere rather than sitting at rest. */
            static int shot_frame = 0;
            if (shot_frame == 0) on_card_click(&demo);
            shot_frame++;
            bool settled = mu_anim_count(demo.animator) == 0 && mu_spring_at_rest(&demo.scroll_bounce);
            if (settled || shot_frame > 300) {
                bool ok = save_shot(&rc, shot_path);
                printf("demo_soft: %s %s (%dx%d, everything %s after %d frame%s)\n",
                       ok ? "wrote" : "FAILED to write", shot_path, sw, sh,
                       settled ? "settled" : "still moving", shot_frame, shot_frame == 1 ? "" : "s");
                if (!ok) {
                    mu_anim_destroy(demo.animator);
                    mu_soft_render_shutdown(&rc);
                    mu_style_shutdown(&ctx);
                    mu_context_shutdown(&ctx);
                    mu_sdl_destroy(app);
                    return 1;
                }
                break;
            }
        }
    }

    mu_anim_destroy(demo.animator);
    mu_soft_render_shutdown(&rc);
    mu_style_shutdown(&ctx);
    mu_context_shutdown(&ctx);
    mu_sdl_destroy(app);
    return 0;
}
