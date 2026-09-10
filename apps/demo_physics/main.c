/*
 * demo_physics — rigid bodies driving the node tree.
 *
 * A container of falling balls, rendered by the software rasterizer. The point is not the
 * simulation (mu_physics.c does that, and is tested without any of this) but the join:
 * each ball is an ordinary MuNode, physics writes its bounds, and everything downstream —
 * damage tracking, clipping, painting — carries on as if a layout pass had moved it.
 *
 * Nothing here marks anything dirty. Damage is discovered by comparing bounds against the
 * previous frame, so a hundred falling balls repaint correctly with no bookkeeping, and
 * once the pile settles the frames go quiet on their own.
 *
 * Click inside the container to drop a ball where you clicked.
 * Press ESC or close the window to quit.
 *
 * Run with `--shot <file.bmp>` to drop a fixed set of balls, simulate a few seconds and
 * write the result out without needing a display.
 */

#include "markup/mu_core.h"
#include "markup/mu_input.h"
#include "markup/mu_layout_flex.h"
#include "markup/mu_physics_node.h"
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

#define MAX_BALLS 140
#define BALL_MIN 9.f
#define BALL_MAX 17.f
#define WALL 14.f

/* ---------------------------------------------------------------------------
 * A shape node kind: a solid colour with a corner radius given as a fraction of
 * its width, so the same kind draws both the balls (0.5 -> a circle) and the
 * static walls (0 -> a plain slab).
 * ------------------------------------------------------------------------ */

typedef struct ShapeState {
    MuColor color;
    float radius_frac;
} ShapeState;

static uint32_t shape_kind;

static void shape_destroy(MuContext *ctx, MuNode *node) {
    (void)ctx;
    free(node->state);
    node->state = NULL;
}

static void shape_paint(MuContext *ctx, MuNode *node, MuRenderContext *rc) {
    (void)ctx;
    ShapeState *s = (ShapeState *)node->state;
    if (!s) return;
    float side = node->bounds.w < node->bounds.h ? node->bounds.w : node->bounds.h;
    mu_draw_rect(rc, node->bounds, s->color, (MuColor){0, 0, 0, 0}, 0.f, side * s->radius_frac);
}

static const MuNodeOps shape_ops = {
    .destroy_state = shape_destroy,
    .paint = shape_paint,
};

static MuNode *make_shape(MuContext *ctx, MuColor color, float radius_frac) {
    ShapeState *s = (ShapeState *)calloc(1, sizeof(ShapeState));
    if (!s) return NULL;
    s->color = color;
    s->radius_frac = radius_frac;
    MuNode *n = mu_node_create(ctx, shape_kind, s);
    if (!n) free(s);
    return n;
}

/* ------------------------------------------------------------------------ */

typedef struct Ball {
    MuBodyId body;
    MuNode *node;
} Ball;

typedef struct Demo {
    MuContext *ctx;
    MuNode *world;
    MuNode *count_label;

    Ball balls[MAX_BALLS];
    int ball_count;
    int oldest; /* ring index, so the cap evicts in drop order */

    MuBodyId walls[3];
    MuNode *wall_nodes[3];

    unsigned rng;
    char count_text[48];
} Demo;

/* Deterministic, so --shot produces the same picture every run. */
static unsigned rnd(Demo *d) {
    d->rng = d->rng * 1664525u + 1013904223u;
    return d->rng >> 8;
}

static float rnd_range(Demo *d, float lo, float hi) {
    return lo + (float)(rnd(d) % 10000u) / 10000.f * (hi - lo);
}

static MuColor ball_color(Demo *d) {
    static const MuColor palette[6] = {
        {96, 165, 250, 255}, {52, 211, 153, 255}, {251, 191, 36, 255},
        {248, 113, 113, 255}, {167, 139, 250, 255}, {45, 212, 191, 255},
    };
    return palette[rnd(d) % 6u];
}

static void update_count(Demo *d) {
    snprintf(d->count_text, sizeof(d->count_text), "%d bodies", d->ball_count);
    if (d->count_label) {
        mu_label_set_text(d->count_label, d->count_text);
        mu_node_mark_paint_dirty(d->count_label);
    }
}

/** Remove one ball entirely: body first, then the node it was driving. */
static void drop_oldest(Demo *d) {
    Ball *b = &d->balls[d->oldest];
    if (!b->node) return;
    /* Order matters. Destroying the node first would leave the world holding a pointer
     * into freed memory until the next advance walked over it. */
    mu_physics_remove_body(d->world, b->body);
    mu_node_remove_child(d->ctx, d->world, b->node);
    mu_node_destroy_recursive(d->ctx, b->node);
    b->node = NULL;
    b->body = MU_BODY_NONE;
    d->ball_count--;
}

static void add_ball(Demo *d, MuVec2 local) {
    MuPhysWorld *w = mu_physics_world(d->world);
    if (!w) return;

    if (d->ball_count >= MAX_BALLS) drop_oldest(d);

    float r = rnd_range(d, BALL_MIN, BALL_MAX);
    MuBodyId body = mu_phys_add_circle(w, MU_BODY_DYNAMIC, local, r, 1.f);
    if (!body) return;
    mu_phys_set_material(w, body, 0.15f, 0.4f);
    mu_phys_set_damping(w, body, 0.05f, 0.4f);

    MuNode *node = make_shape(d->ctx, ball_color(d), 0.5f);
    if (!node || !mu_physics_attach(d->ctx, d->world, node, body)) {
        mu_phys_remove(w, body);
        if (node) mu_node_destroy_recursive(d->ctx, node);
        return;
    }

    d->balls[d->oldest].body = body;
    d->balls[d->oldest].node = node;
    d->oldest = (d->oldest + 1) % MAX_BALLS;
    d->ball_count++;
    update_count(d);
}

/**
 * Static walls sized to the container. Shapes cannot be resized in place, so a resize
 * rebuilds them — which is also the simplest demonstration of removing a bound body.
 */
static void rebuild_walls(Demo *d) {
    MuPhysWorld *w = mu_physics_world(d->world);
    if (!w) return;

    for (int i = 0; i < 3; i++) {
        if (d->walls[i]) mu_physics_remove_body(d->world, d->walls[i]);
        if (d->wall_nodes[i]) {
            mu_node_remove_child(d->ctx, d->world, d->wall_nodes[i]);
            mu_node_destroy_recursive(d->ctx, d->wall_nodes[i]);
        }
        d->walls[i] = MU_BODY_NONE;
        d->wall_nodes[i] = NULL;
    }

    float cw = d->world->bounds.w, ch = d->world->bounds.h;
    const MuColor wall_col = {71, 85, 105, 255};

    /* floor, left, right — all in coordinates local to the container */
    const MuVec2 pos[3] = {{cw * 0.5f, ch - WALL * 0.5f},
                           {WALL * 0.5f, ch * 0.5f},
                           {cw - WALL * 0.5f, ch * 0.5f}};
    const MuVec2 half[3] = {{cw * 0.5f, WALL * 0.5f},
                            {WALL * 0.5f, ch * 0.5f},
                            {WALL * 0.5f, ch * 0.5f}};

    for (int i = 0; i < 3; i++) {
        d->walls[i] = mu_phys_add_box(w, MU_BODY_STATIC, pos[i], half[i].x, half[i].y, 0.f);
        mu_phys_set_material(w, d->walls[i], 0.f, 0.6f);
        d->wall_nodes[i] = make_shape(d->ctx, wall_col, 0.f);
        mu_physics_attach(d->ctx, d->world, d->wall_nodes[i], d->walls[i]);
    }
}

static void on_world_click(void *user, MuVec2 local) {
    add_ball((Demo *)user, local);
}

static void on_drop_many(void *user) {
    Demo *d = (Demo *)user;
    float cw = d->world->bounds.w;
    for (int i = 0; i < 20; i++)
        add_ball(d, (MuVec2){rnd_range(d, WALL + BALL_MAX, cw - WALL - BALL_MAX),
                             rnd_range(d, 30.f, 90.f)});
}

static void on_reset(void *user) {
    Demo *d = (Demo *)user;
    for (int i = 0; i < MAX_BALLS; i++)
        if (d->balls[i].node) {
            mu_physics_remove_body(d->world, d->balls[i].body);
            mu_node_remove_child(d->ctx, d->world, d->balls[i].node);
            mu_node_destroy_recursive(d->ctx, d->balls[i].node);
            d->balls[i].node = NULL;
            d->balls[i].body = MU_BODY_NONE;
        }
    d->ball_count = 0;
    d->oldest = 0;
    update_count(d);
}

static bool save_shot(MuRenderContext *rc, const char *path) {
    int w = 0, h = 0, pitch = 0;
    const void *pixels = mu_present_pixel_data(rc, &w, &h, &pitch);
    if (!pixels || w <= 0 || h <= 0) return false;
    SDL_Surface *surf = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ARGB8888, (void *)pixels, pitch);
    if (!surf) return false;
    bool ok = SDL_SaveBMP(surf, path);
    SDL_DestroySurface(surf);
    return ok;
}

int main(int argc, char **argv) {
    const int W = 900, H = 640;

    const char *shot_path = NULL;
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], "--shot") == 0) shot_path = argv[i + 1];

    MuSdlApp *app = mu_sdl_create("Markup — physics", W, H);
    if (!app) {
        fprintf(stderr, "demo_physics: failed to create SDL window\n");
        return 1;
    }

    MuContext ctx;
    mu_context_init(&ctx, 1 << 20);
    mu_style_init(&ctx, NULL);

    MuRenderContext rc;
    mu_soft_render_init(&rc, W, H);
    mu_render_bind_measure(&rc);
    if (!mu_soft_set_font_file(&rc, "assets/InterVariable.ttf"))
        mu_soft_set_font_file(&rc, MARKUP_DEMO_FONT_FILE_ABS);

    mu_widgets_basic_register(&ctx);
    mu_physics_register(&ctx);
    shape_kind = mu_register_node_kind(&ctx, "phys_shape", &shape_ops);

    Demo demo;
    memset(&demo, 0, sizeof(demo));
    demo.ctx = &ctx;
    demo.rng = 12345u;

    MuNode *root = mu_make_panel(&ctx, true);
    root->role = "page";
    mu_layout_set_padding_all(root, 16.f);
    mu_layout_set_gap(root, 12.f);
    mu_node_set_bounds(root, (MuRect){0, 0, W, H});
    mu_context_set_root(&ctx, root);

    MuNode *bar = mu_make_panel(&ctx, false);
    bar->role = "toolbar";
    mu_layout_set_padding_all(bar, 10.f);
    mu_layout_set_gap(bar, 10.f);
    mu_node_add_child(&ctx, root, bar);
    mu_node_add_child(&ctx, bar, mu_make_button(&ctx, "Drop 20", on_drop_many, &demo));
    mu_node_add_child(&ctx, bar, mu_make_button(&ctx, "Reset", on_reset, &demo));
    demo.count_label = mu_make_label(&ctx, "0 bodies");
    mu_node_add_child(&ctx, bar, demo.count_label);

    /*
     * The container goes on the modal layer. mu_layout_run only walks the root, so its
     * bounds are the demo's to set — which is what a free-positioned simulation region
     * wants. Put it inside the flex tree instead and flex would size it for you; bodies
     * are in local coordinates either way.
     */
    MuNode *layer = mu_make_panel(&ctx, true);
    layer->role = "group";
    mu_modal_bind_layer(&ctx, layer);

    demo.world = mu_make_physics_world(&ctx, NULL);
    mu_node_add_child(&ctx, layer, demo.world);
    mu_physics_set_on_click(demo.world, on_world_click, &demo);

    int last_w = 0, last_h = 0;
    uint64_t prev_ticks = SDL_GetTicksNS();

    while (mu_sdl_poll(app, &ctx)) {
        mu_frame_begin(&ctx);

        int sw = mu_sdl_width(app), sh = mu_sdl_height(app);
        if (sw != last_w || sh != last_h) {
            mu_soft_resize(&rc, sw, sh);
            mu_node_set_bounds(root, (MuRect){0, 0, (float)sw, (float)sh});
            mu_node_set_bounds(layer, (MuRect){0, 0, (float)sw, (float)sh});
            mu_damage_all(&ctx);
            last_w = sw;
            last_h = sh;
        }

        mu_layout_run(&ctx);

        /* Below the toolbar, inset from the window edges. */
        float top = bar->bounds.y + bar->bounds.h + 12.f;
        MuRect region = {16.f, top, (float)sw - 32.f, (float)sh - top - 16.f};
        bool resized = region.w != demo.world->bounds.w || region.h != demo.world->bounds.h;
        mu_node_set_bounds(demo.world, region);
        if (resized) rebuild_walls(&demo);

        uint64_t now = SDL_GetTicksNS();
        float dt = (float)((double)(now - prev_ticks) / 1e9);
        prev_ticks = now;
        if (shot_path) dt = 1.f / 60.f; /* deterministic when capturing */

        mu_sdl_frame(&ctx, app);
        mu_physics_advance(demo.world, dt);

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
            /* Drop a batch on the first frame, let it settle, then capture and exit. */
            static int shot_frame = 0;
            if (shot_frame == 0) {
                on_drop_many(&demo);
                on_drop_many(&demo);
                on_drop_many(&demo);
            }
            if (++shot_frame > 240) {
                bool ok = save_shot(&rc, shot_path);
                printf("demo_physics: %s %s (%d bodies)\n", ok ? "wrote" : "FAILED to write",
                       shot_path, demo.ball_count);
                break;
            }
        }
    }

    mu_soft_render_shutdown(&rc);
    mu_style_shutdown(&ctx);
    mu_context_shutdown(&ctx);
    mu_sdl_destroy(app);
    return 0;
}
