#include "../include/markup/mu_raylib.h"
#include "../include/markup/mu_input.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static Font g_mu_ui_font;

static void mu_apply_font_filter(Font f) {
    if (f.texture.id != 0)
        SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
}

Color mu_to_ray(MuColor c) {
    return (Color){c.r, c.g, c.b, c.a};
}

void mu_render_init(MuRenderContext *rc) {
    memset(rc, 0, sizeof(*rc));
    rc->font = GetFontDefault();
    rc->font_loaded = false;
    g_mu_ui_font = rc->font;
    mu_apply_font_filter(rc->font);
}

void mu_render_shutdown(MuRenderContext *rc) {
    if (!rc)
        return;
    if (rc->font_loaded)
        UnloadFont(rc->font);
    rc->font_loaded = false;
    rc->font = GetFontDefault();
    g_mu_ui_font = rc->font;
}

Font mu_raylib_ui_font(void) {
    return g_mu_ui_font;
}

void mu_render_set_font(MuRenderContext *rc, Font font, bool take_ownership) {
    if (!rc)
        return;
    if (rc->font_loaded)
        UnloadFont(rc->font);
    rc->font = font;
    rc->font_loaded = take_ownership;
    g_mu_ui_font = font;
    mu_apply_font_filter(font);
}

void mu_render_begin(MuRenderContext *rc) {
    (void)rc;
}

void mu_render_end(MuRenderContext *rc) {
    (void)rc;
    while (rc->scissor_depth > 0) mu_pop_scissor(rc);
}

void mu_push_scissor(MuRenderContext *rc, MuRect r) {
    if (rc->scissor_depth >= 32) return;
    BeginScissorMode((int)r.x, (int)r.y, (int)r.w, (int)r.h);
    rc->scissor_depth++;
}

void mu_pop_scissor(MuRenderContext *rc) {
    if (rc->scissor_depth <= 0) return;
    EndScissorMode();
    rc->scissor_depth--;
}

static void draw_rounded_rect(float x, float y, float w, float h, float radius_px, Color col) {
    if (radius_px <= 0.5f) {
        DrawRectangle((int)x, (int)y, (int)w, (int)h, col);
        return;
    }
    if (w < 1.f || h < 1.f)
        return;
    /* raylib expects roundness in (0..1]; corner r = min(w,h)*roundness/2 */
    float m = (w < h) ? w : h;
    float roundness = (2.f * radius_px) / m;
    if (roundness > 1.f)
        roundness = 1.f;
    DrawRectangleRounded((Rectangle){x, y, w, h}, roundness, 18, col);
}

void mu_draw_rect(MuRenderContext *rc, MuRect r, MuColor fill, MuColor border, float border_w, float radius) {
    (void)rc;
    if (fill.a > 0)
        draw_rounded_rect(r.x, r.y, r.w, r.h, radius, mu_to_ray(fill));
    if (border_w > 0 && border.a > 0)
        DrawRectangleLinesEx((Rectangle){r.x, r.y, r.w, r.h}, border_w, mu_to_ray(border));
}

void mu_draw_text(MuRenderContext *rc, const char *text, float x, float y, float font_size, MuColor fg) {
    if (!text) return;
    DrawTextEx(rc->font, text, (Vector2){x, y}, font_size, 1.0f, mu_to_ray(fg));
}

void mu_paint_tree(MuContext *ctx, MuRenderContext *rc, MuNode *node) {
    if (!node || !(node->flags & MU_NODE_VISIBLE)) return;
    if (node->flags & MU_NODE_CLIP_CHILDREN) mu_push_scissor(rc, node->bounds);

    const MuNodeOps *ops = mu_get_node_ops(ctx, node->kind);
    if (ops && ops->paint) ops->paint(ctx, node, rc);

    for (int i = 0; i < node->child_count; i++) mu_paint_tree(ctx, rc, node->children[i]);

    if (node->flags & MU_NODE_CLIP_CHILDREN) mu_pop_scissor(rc);
}

void mu_paint_all(MuContext *ctx, MuRenderContext *rc) {
    if (ctx->root) mu_paint_tree(ctx, rc, ctx->root);
    if (ctx->modal_layer && (ctx->modal_layer->flags & MU_NODE_VISIBLE)) mu_paint_tree(ctx, rc, ctx->modal_layer);
}

static void raylib_dispatch_input(MuContext *ctx) {
    MuVec2 mp = {GetMousePosition().x, GetMousePosition().y};
    mu_input_update_hover(ctx, mp);

    MuPointerEvent ev = {0};
    ev.position = mp;
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ev.button = 0;
        ev.pressed = true;
        mu_input_dispatch_pointer(ctx, &ev);
    } else if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        ev.button = 0;
        ev.released = true;
        mu_input_dispatch_pointer(ctx, &ev);
    }

    if (IsKeyPressed(KEY_TAB)) {
        mu_focus_advance_tab(ctx);
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        MuKeyEvent ke = {KEY_BACKSPACE, true, false};
        mu_input_dispatch_key(ctx, &ke);
    }

    MuNode *focused = mu_context_find_id(ctx, NULL, ctx->focused_id);
    if (focused) {
        int ch;
        while ((ch = GetCharPressed()) > 0)
            mu_input_dispatch_char(ctx, (unsigned int)ch);
    }

    if (ctx->captured_pointer_id && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        MuPointerEvent ev2 = {0};
        ev2.position = mp;
        ev2.drag = true;
        mu_input_dispatch_pointer(ctx, &ev2);
    }
}

void mu_raylib_frame(MuContext *ctx, MuRenderContext *rc) {
    (void)rc;
    raylib_dispatch_input(ctx);
}
