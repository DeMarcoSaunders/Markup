#include "../include/markup/mu_raylib.h"
#include "../include/markup/mu_image.h"
#include "../include/markup/mu_input.h"
#include "../include/markup/mu_popup.h"
#include "../include/markup/mu_widgets_basic.h"
#include "mu_path_raster.h"
#include "mu_input_keymap.h"
#include "../include/markup/mu_text.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Context bound for metric queries made during layout, where no rc is threaded through
 * (see mu_render_bind_measure). */
static MuRenderContext *g_mu_measure_rc;

void mu_render_bind_measure(MuRenderContext *rc) {
    g_mu_measure_rc = rc;
}

static MuRenderContext *measure_rc(MuRenderContext *rc) {
    return rc ? rc : g_mu_measure_rc;
}

static void mu_apply_font_filter(Font f) {
    if (f.texture.id != 0)
        SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
}

static void raylib_set_slot(MuRenderContext *rc, int slot, Font font, bool owned, const char *family) {
    if (!rc || slot < 0 || slot >= MU_FONT_MAX) return;
    if (rc->fonts[slot].owned && rc->fonts[slot].font.texture.id != 0)
        UnloadFont(rc->fonts[slot].font);
    rc->fonts[slot].font = font;
    rc->fonts[slot].owned = owned;
    if (family)
        snprintf(rc->fonts[slot].family, sizeof(rc->fonts[slot].family), "%s", family);
    else
        rc->fonts[slot].family[0] = '\0';
    mu_apply_font_filter(font);
    if (slot == 0) rc->font = font;
    if (slot >= rc->font_count) rc->font_count = slot + 1;
}

Color mu_to_ray(MuColor c) {
    return (Color){c.r, c.g, c.b, c.a};
}

void mu_render_init(MuRenderContext *rc) {
    memset(rc, 0, sizeof(*rc));
    Font def = GetFontDefault();
    raylib_set_slot(rc, 0, def, false, "default");
    rc->font_loaded = false;
    rc->image_count = 1;
}

void mu_render_shutdown(MuRenderContext *rc) {
    if (!rc)
        return;
    for (int i = 0; i < rc->font_count; i++) {
        if (rc->fonts[i].owned && rc->fonts[i].font.texture.id != 0)
            UnloadFont(rc->fonts[i].font);
    }
    for (int i = 0; i < rc->image_count; i++) {
        if (rc->images[i].owned && rc->images[i].texture.id != 0)
            UnloadTexture(rc->images[i].texture);
    }
    rc->font_count = 0;
    rc->image_count = 0;
    rc->font_loaded = false;
    rc->font = GetFontDefault();
}

void mu_render_set_font(MuRenderContext *rc, Font font, bool take_ownership) {
    if (!rc)
        return;
    if (rc->font_loaded)
        UnloadFont(rc->font);
    raylib_set_slot(rc, 0, font, take_ownership, NULL);
    rc->font_loaded = take_ownership;
}

uint32_t mu_font_load_file(MuRenderContext *rc, const char *path, const char *family_name) {
    if (!rc || !path || !path[0] || rc->font_count >= MU_FONT_MAX)
        return MU_FONT_DEFAULT;
    Font f = LoadFont(path);
    if (f.texture.id == 0)
        return MU_FONT_DEFAULT;
    int id = rc->font_count;
    raylib_set_slot(rc, id, f, true, family_name ? family_name : path);
    return (uint32_t)id;
}

static Texture2D *raylib_resolve_image(MuRenderContext *rc, uint32_t image_id) {
    if (!rc || image_id == MU_IMAGE_INVALID || image_id >= (uint32_t)rc->image_count)
        return NULL;
    if (rc->images[image_id].texture.id == 0)
        return NULL;
    return &rc->images[image_id].texture;
}

uint32_t mu_image_load_file(MuRenderContext *rc, const char *path) {
    if (!rc || !path || !path[0] || rc->image_count >= MU_IMAGE_MAX)
        return MU_IMAGE_INVALID;
    Texture2D tex = LoadTexture(path);
    if (tex.id == 0) {
        MuImageRgba rgba = {0};
        if (mu_image_decode_rgba_file(path, &rgba)) {
            Image img = {
                .data = rgba.pixels,
                .width = rgba.width,
                .height = rgba.height,
                .mipmaps = 1,
                .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
            };
            tex = LoadTextureFromImage(img);
            mu_image_rgba_free(&rgba);
        }
    }
    if (tex.id == 0)
        return MU_IMAGE_INVALID;
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    int id = rc->image_count++;
    rc->images[id].texture = tex;
    rc->images[id].owned = true;
    rc->images[id].width = (float)tex.width;
    rc->images[id].height = (float)tex.height;
    return (uint32_t)id;
}

bool mu_image_get_size(MuRenderContext *rc, uint32_t image_id, float *out_w, float *out_h) {
    rc = measure_rc(rc);
    if (!rc || image_id == MU_IMAGE_INVALID || image_id >= (uint32_t)rc->image_count)
        return false;
    if (rc->images[image_id].texture.id == 0)
        return false;
    if (out_w) *out_w = rc->images[image_id].width;
    if (out_h) *out_h = rc->images[image_id].height;
    return true;
}

void mu_draw_image(MuRenderContext *rc, uint32_t image_id, MuRect dst, const MuDrawImageOpts *opts) {
    Texture2D *tex = raylib_resolve_image(rc, image_id);
    if (!tex || dst.w < 1.f || dst.h < 1.f)
        return;

    MuDrawImageOpts defaults;
    if (!opts) {
        mu_draw_image_opts_init(&defaults);
        opts = &defaults;
    }

    float tex_w = (float)tex->width;
    float tex_h = (float)tex->height;
    MuRect src_px;
    if (!mu_image_resolve_src(&opts->src, tex_w, tex_h, &src_px)) return;

    MuRect draw = mu_image_fit_dst(src_px.w, src_px.h, dst, opts->fit);
    Rectangle src = {src_px.x, src_px.y, src_px.w, src_px.h};
    Rectangle dest = {draw.x, draw.y, draw.w, draw.h};
    Color tint = mu_to_ray(opts->tint);
    DrawTexturePro(*tex, src, dest, (Vector2){0, 0}, 0.f, tint);
}

static Font raylib_resolve_font(MuRenderContext *rc, const MuTextStyle *style) {
    Font fallback = rc ? rc->fonts[0].font : GetFontDefault();
    if (!rc || !style || style->font_id == MU_FONT_DEFAULT)
        return fallback;
    if (style->font_id >= (uint32_t)rc->font_count)
        return fallback;
    Font f = rc->fonts[style->font_id].font;
    return f.texture.id != 0 ? f : fallback;
}

static void raylib_draw_text_ex(Font f, const char *text, Vector2 pos, float size, float spacing, Color col,
                                int weight, int italic) {
    DrawTextEx(f, text, pos, size, spacing, col);
    if (weight >= 600)
        DrawTextEx(f, text, (Vector2){pos.x + 1.f, pos.y}, size, spacing, col);
    if (italic > 0)
        DrawTextEx(f, text, (Vector2){pos.x + 2.f, pos.y - 0.5f}, size, spacing, col);
}

void mu_text_measure(MuRenderContext *rc, const char *text, const MuTextStyle *style, MuTextMetrics *out) {
    rc = measure_rc(rc);
    if (!out) return;
    out->width = 0.f;
    out->height = 0.f;
    if (!text) return;

    MuTextStyle defaults;
    if (!style) {
        mu_text_style_init(&defaults);
        defaults.size = 16.f;
        defaults.letter_spacing = 1.f;
        style = &defaults;
    }

    Font f = raylib_resolve_font(rc, style);
    float size = style->size > 0.f ? style->size : 16.f;
    float spacing = style->letter_spacing >= 0.f ? style->letter_spacing : 1.f;
    Vector2 sz = MeasureTextEx(f, text, size, spacing);
    if (style->weight >= 600) sz.x += 1.f;
    if (style->italic > 0) sz.x += 2.f;
    out->width = sz.x;
    out->height = sz.y;
}

void mu_draw_backdrop_blur(MuRenderContext *rc, MuRect area, float blur_radius, MuColor tint,
                           float corner_radius) {
    /* Degraded: a flat tint, no blur.
     *
     * Real backdrop blur needs to read what has already been drawn. Under raylib that
     * means LoadImageFromScreen (a full pipeline stall every frame) or restructuring the
     * app around a RenderTexture2D and a blur shader. Neither belongs behind a call that
     * looks this cheap, so this path deliberately approximates rather than pretending.
     * The software backend does the real thing. */
    (void)blur_radius;
    if (!rc) return;
    if (tint.a == 0) return;
    mu_draw_rect(rc, area, tint, (MuColor){0, 0, 0, 0}, 0.f, corner_radius);
}

/* No cache: the blur above is one flat rect, so replaying it would cost more than drawing
 * it, and there is no host-readable surface to snapshot in any case. Reporting a
 * permanent miss keeps callers on the single correct path. */
bool mu_backdrop_cache_try(MuRenderContext *rc, const MuNode *node, MuRect area, float blur_radius,
                           MuColor tint, float corner_radius) {
    (void)rc;
    (void)node;
    (void)area;
    (void)blur_radius;
    (void)tint;
    (void)corner_radius;
    return false;
}

void mu_backdrop_cache_store(MuRenderContext *rc, const MuNode *node, MuRect area, float blur_radius,
                             MuColor tint, float corner_radius) {
    (void)rc;
    (void)node;
    (void)area;
    (void)blur_radius;
    (void)tint;
    (void)corner_radius;
}

void mu_backdrop_cache_drop(MuRenderContext *rc, const MuNode *node) {
    (void)rc;
    (void)node;
}

void mu_render_logical_size(MuRenderContext *rc, int *out_w, int *out_h) {
    /* No scaling on this backend: logical units are pixels.
     *
     * The size comes from raylib rather than from the context, which carries no surface
     * of its own -- this backend draws straight into the window raylib owns. Asking
     * raylib also means a resize needs no bookkeeping here. */
    (void)rc;
    if (out_w) *out_w = GetScreenWidth();
    if (out_h) *out_h = GetScreenHeight();
}

const void *mu_present_pixel_data(MuRenderContext *rc, int *out_w, int *out_h, int *out_row_bytes) {
    /* Raylib presents through the GPU; there is no host-readable frame buffer to blit. */
    (void)rc;
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    if (out_row_bytes) *out_row_bytes = 0;
    return NULL;
}

static void apply_scissor(MuRect r) {
    int x = (int)floorf(r.x), y = (int)floorf(r.y);
    int w = (int)ceilf(r.x + r.w) - x, h = (int)ceilf(r.y + r.h) - y;
    BeginScissorMode(x, y, w > 0 ? w : 0, h > 0 ? h : 0);
}

void mu_push_scissor(MuRenderContext *rc, MuRect r) {
    if (!rc) return;
    if (rc->scissor_depth >= 32) {
        rc->scissor_overflow++;
        BeginScissorMode(0,0,0,0);
        return;
    }
    if (rc->scissor_depth) {
        MuRect parent=rc->scissor_stack[rc->scissor_depth-1];
        float right=fminf(r.x+r.w,parent.x+parent.w), bottom=fminf(r.y+r.h,parent.y+parent.h);
        r.x=fmaxf(r.x,parent.x); r.y=fmaxf(r.y,parent.y);
        r.w=fmaxf(0,right-r.x); r.h=fmaxf(0,bottom-r.y);
    }
    rc->scissor_stack[rc->scissor_depth++]=r;
    apply_scissor(r);
}

void mu_pop_scissor(MuRenderContext *rc) {
    if (!rc) return;
    if (rc->scissor_overflow) {
        if (--rc->scissor_overflow) return;
    } else if (rc->scissor_depth) rc->scissor_depth--;
    else return;
    if (rc->scissor_depth) apply_scissor(rc->scissor_stack[rc->scissor_depth-1]);
    else EndScissorMode();
}

static void draw_rounded_rect(float x, float y, float w, float h, float radius_px, Color col) {
    if (radius_px <= 0.5f) {
        DrawRectangle((int)x, (int)y, (int)w, (int)h, col);
        return;
    }
    if (w < 1.f || h < 1.f)
        return;
    float m = (w < h) ? w : h;
    float roundness = (2.f * radius_px) / m;
    if (roundness > 1.f)
        roundness = 1.f;
    DrawRectangleRounded((Rectangle){x, y, w, h}, roundness, 18, col);
}

static void raylib_path_span(void *ctx, int y, int x0, int x1, MuColor col, uint8_t coverage) {
    (void)ctx;
    if (x1 <= x0) return;
    Color c = mu_to_ray(col);
    if (coverage < 255) c.a = (unsigned char)((c.a * coverage) / 255);
    DrawRectangle(x0, y, x1 - x0, 1, c);
}

void mu_draw_path(MuRenderContext *rc, const MuPath *path, const MuDrawPathOpts *opts) {
    (void)rc;
    if (!path) return;

    MuDrawPathOpts defaults;
    if (!opts) {
        mu_draw_path_opts_init(&defaults);
        opts = &defaults;
    }

    if (opts->fill.a > 0) mu_path_raster_fill_even_odd(path, raylib_path_span, NULL, opts->fill);
    if (opts->stroke.a > 0 && opts->stroke_width > 0.f)
        mu_path_raster_stroke(path, raylib_path_span, NULL, opts->stroke, opts->stroke_width);
}

/* Deprecated backend: like backdrop blur, this degrades to a flat shape. */
void mu_draw_shadow(MuRenderContext *rc, MuRect r, float radius, MuColor color,
                     float blur, MuVec2 offset) {
    (void)blur;
    r.x += offset.x;
    r.y += offset.y;
    mu_draw_rect(rc, r, color, (MuColor){0}, 0.f, radius);
}

void mu_draw_rect(MuRenderContext *rc, MuRect r, MuColor fill, MuColor border, float border_w, float radius) {
    (void)rc;
    if (fill.a > 0)
        draw_rounded_rect(r.x, r.y, r.w, r.h, radius, mu_to_ray(fill));
    if (border_w > 0 && border.a > 0)
        DrawRectangleLinesEx((Rectangle){r.x, r.y, r.w, r.h}, border_w, mu_to_ray(border));
}

void mu_draw_text(MuRenderContext *rc, const char *text, float x, float y, const MuTextStyle *style, MuColor fg) {
    if (!text) return;

    MuTextStyle defaults;
    if (!style) {
        mu_text_style_init(&defaults);
        defaults.size = 16.f;
        defaults.letter_spacing = 1.f;
        style = &defaults;
    }

    Font f = raylib_resolve_font(rc, style);
    float size = style->size > 0.f ? style->size : 16.f;
    float spacing = style->letter_spacing >= 0.f ? style->letter_spacing : 1.f;
    int weight = style->weight > 0 ? style->weight : MU_TEXT_WEIGHT_NORMAL;
    int italic = style->italic > 0 ? 1 : 0;
    raylib_draw_text_ex(f, text, (Vector2){x, y}, size, spacing, mu_to_ray(fg), weight, italic);
}

static uint32_t raylib_modifiers(void) {
    uint32_t m = 0;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) m |= MU_MOD_SHIFT;
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) m |= MU_MOD_CTRL;
    if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) m |= MU_MOD_ALT;
    if (IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER)) m |= MU_MOD_META;
    return m;
}

static void raylib_emit_key(MuContext *ctx, int rk, bool down) {
    int mk = mu_key_from_raylib_key(rk);
    if (mk == MU_KEY_UNKNOWN) return;
    MuKeyEvent ke = {mk, down, !down, false, ctx->seat_modifiers};
    mu_input_dispatch_key(ctx, &ke);
}

static void raylib_pointer_btn(MuContext *ctx, MuVec2 mp, int btn, bool down) {
    MuPointerEvent ev = {0};
    ev.position = mp;
    ev.button = btn;
    ev.modifiers = ctx->seat_modifiers;
    if (down)
        ev.pressed = true;
    else
        ev.released = true;
    mu_popups_dispatch_pointer(ctx, &ev);
    mu_input_dispatch_pointer(ctx, &ev);
}

static void raylib_dispatch_input(MuContext *ctx) {
    MuVec2 mp = {GetMousePosition().x, GetMousePosition().y};
    mu_input_set_modifiers(ctx, raylib_modifiers());
    mu_input_update_hover(ctx, mp);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) raylib_pointer_btn(ctx, mp, MU_BUTTON_LEFT, true);
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) raylib_pointer_btn(ctx, mp, MU_BUTTON_LEFT, false);
    if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE)) raylib_pointer_btn(ctx, mp, MU_BUTTON_MIDDLE, true);
    if (IsMouseButtonReleased(MOUSE_BUTTON_MIDDLE)) raylib_pointer_btn(ctx, mp, MU_BUTTON_MIDDLE, false);
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) raylib_pointer_btn(ctx, mp, MU_BUTTON_RIGHT, true);
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) raylib_pointer_btn(ctx, mp, MU_BUTTON_RIGHT, false);

    if (IsKeyPressed(KEY_TAB)) mu_focus_advance_tab(ctx);

    for (int k = KEY_SPACE; k <= KEY_KB_MENU; k++) {
        if (IsKeyPressed(k)) raylib_emit_key(ctx, k, true);
        if (IsKeyReleased(k)) raylib_emit_key(ctx, k, false);
    }

    MuNode *focused = mu_context_find_id(ctx, NULL, ctx->focused_id);
    if (focused) {
        int ch;
        while ((ch = GetCharPressed()) > 0) mu_input_dispatch_char(ctx, (unsigned int)ch);
    }

    if (ctx->captured_pointer_id && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        MuPointerEvent ev2 = {0};
        ev2.position = mp;
        ev2.button = MU_BUTTON_LEFT;
        ev2.drag = true;
        ev2.modifiers = ctx->seat_modifiers;
        mu_input_dispatch_pointer(ctx, &ev2);
    }

    float wheel = GetMouseWheelMove();
    if (wheel != 0.f) {
        MuWheelEvent we = {mp, 0.f, wheel, ctx->seat_modifiers};
        mu_input_dispatch_wheel(ctx, &we);
    }
}

void mu_raylib_frame(MuContext *ctx, MuRenderContext *rc) {
    (void)rc;
    raylib_dispatch_input(ctx);
}

void mu_draw_image_rgba_buffer(MuRenderContext *rc, const uint32_t *pixels, int w, int h, int stride, MuRect dst,
                               const MuDrawImageOpts *opts) {
    (void)rc;
    (void)pixels;
    (void)w;
    (void)h;
    (void)stride;
    (void)dst;
    (void)opts;
}
