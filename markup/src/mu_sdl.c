#include "../include/markup/mu_sdl.h"
#include "../include/markup/mu_input.h"
#include "../include/markup/mu_popup.h"
#include "../include/markup/mu_widgets_basic.h"
#include "mu_input_keymap.h"

#include <SDL3/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct MuSdlApp {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    int width;
    int height;
    bool mouse_down;
    float mouse_x;
    float mouse_y;
    bool text_input_active;
    float wheel_x;
    float wheel_y;
    float wheel_mouse_x;
    float wheel_mouse_y;
    bool wheel_pending;
    bool btn_down[3];
    bool btn_was[3];
};

static int sdl_button_index(Uint8 sdl_btn) {
    if (sdl_btn == SDL_BUTTON_LEFT) return MU_BUTTON_LEFT;
    if (sdl_btn == SDL_BUTTON_MIDDLE) return MU_BUTTON_MIDDLE;
    if (sdl_btn == SDL_BUTTON_RIGHT) return MU_BUTTON_RIGHT;
    return MU_BUTTON_LEFT;
}

static uint32_t sdl_modifiers(SDL_Keymod m) {
    uint32_t out = 0;
    if (m & SDL_KMOD_SHIFT) out |= MU_MOD_SHIFT;
    if (m & SDL_KMOD_CTRL) out |= MU_MOD_CTRL;
    if (m & SDL_KMOD_ALT) out |= MU_MOD_ALT;
    if (m & SDL_KMOD_GUI) out |= MU_MOD_META;
    return out;
}

MuSdlApp *mu_sdl_create(const char *title, int width, int height) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) return NULL;

    MuSdlApp *app = (MuSdlApp *)calloc(1, sizeof(MuSdlApp));
    if (!app) return NULL;

    app->width = width;
    app->height = height;
    app->window = SDL_CreateWindow(title ? title : "Markup", width, height, SDL_WINDOW_RESIZABLE);
    if (!app->window) {
        free(app);
        return NULL;
    }

    app->renderer = SDL_CreateRenderer(app->window, NULL);
    if (!app->renderer) {
        SDL_DestroyWindow(app->window);
        free(app);
        return NULL;
    }

    app->texture = SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                     width, height);
    if (!app->texture) {
        SDL_DestroyRenderer(app->renderer);
        SDL_DestroyWindow(app->window);
        free(app);
        return NULL;
    }

    return app;
}

void mu_sdl_destroy(MuSdlApp *app) {
    if (!app) return;
    if (app->texture) SDL_DestroyTexture(app->texture);
    if (app->renderer) SDL_DestroyRenderer(app->renderer);
    if (app->window) SDL_DestroyWindow(app->window);
    free(app);
    SDL_Quit();
}

int mu_sdl_width(const MuSdlApp *app) {
    return app ? app->width : 0;
}

int mu_sdl_height(const MuSdlApp *app) {
    return app ? app->height : 0;
}

static void refresh_size(MuSdlApp *app) {
    if (!app || !app->window) return;
    int w = 0, h = 0;
    if (!SDL_GetWindowSize(app->window, &w, &h)) return;
    if (w <= 0 || h <= 0) return;
    if (w == app->width && h == app->height) return;
    app->width = w;
    app->height = h;
    if (app->texture) SDL_DestroyTexture(app->texture);
    app->texture = SDL_CreateTexture(app->renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
}

static void dispatch_sdl_key(MuContext *ctx, const SDL_KeyboardEvent *key, bool down) {
    if (!ctx || !key) return;
    int mk = mu_key_from_sdl_sym((long)key->key);
    if (mk == MU_KEY_UNKNOWN) return;
    MuKeyEvent ke = {mk, down, !down, key->repeat != 0, sdl_modifiers(SDL_GetModState())};
    mu_input_dispatch_key(ctx, &ke);
}

bool mu_sdl_poll(MuSdlApp *app, MuContext *ctx) {
    if (!app) return false;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_EVENT_QUIT:
            return false;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_RESIZED:
            refresh_size(app);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            int b = sdl_button_index(ev.button.button);
            if (b >= 0 && b < 3) app->btn_down[b] = true;
            if (ev.button.button == SDL_BUTTON_LEFT) app->mouse_down = true;
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            int b = sdl_button_index(ev.button.button);
            if (b >= 0 && b < 3) app->btn_down[b] = false;
            if (ev.button.button == SDL_BUTTON_LEFT) app->mouse_down = false;
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            app->mouse_x = ev.motion.x;
            app->mouse_y = ev.motion.y;
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            app->wheel_x += ev.wheel.x;
            app->wheel_y += ev.wheel.y;
            app->wheel_mouse_x = ev.wheel.mouse_x;
            app->wheel_mouse_y = ev.wheel.mouse_y;
            app->wheel_pending = true;
            break;
        case SDL_EVENT_TEXT_INPUT:
            if (ctx) {
                MuNode *focused = mu_context_find_id(ctx, NULL, ctx->focused_id);
                if (focused && focused->role && strcmp(focused->role, "input") == 0)
                    mu_textinput_insert_utf8(ctx, focused, ev.text.text);
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            dispatch_sdl_key(ctx, &ev.key, true);
            break;
        case SDL_EVENT_KEY_UP:
            dispatch_sdl_key(ctx, &ev.key, false);
            break;
        default:
            break;
        }
    }
    refresh_size(app);
    float mx = 0.f, my = 0.f;
    (void)SDL_GetMouseState(&mx, &my);
    app->mouse_x = mx;
    app->mouse_y = my;
    return true;
}

void mu_sdl_present_rect(MuSdlApp *app, MuRenderContext *rc, MuRect area) {
    if (!app || !rc || !app->texture) return;
    int w = 0, h = 0, stride = 0;
    const void *pixels = mu_present_pixel_data(rc, &w, &h, &stride);
    if (!pixels || w <= 0 || h <= 0) return;

    if (w != app->width || h != app->height) refresh_size(app);

    int x0 = (int)floorf(area.x), y0 = (int)floorf(area.y);
    int x1 = (int)ceilf(area.x + area.w), y1 = (int)ceilf(area.y + area.h);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > w) x1 = w;
    if (y1 > h) y1 = h;

    if (x1 > x0 && y1 > y0) {
        SDL_Rect r = {x0, y0, x1 - x0, y1 - y0};
        const unsigned char *base = (const unsigned char *)pixels;
        const void *sub = base + (size_t)y0 * (size_t)stride + (size_t)x0 * 4u;
        SDL_UpdateTexture(app->texture, &r, sub, stride);
    }

    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderClear(app->renderer);
    SDL_RenderTexture(app->renderer, app->texture, NULL, NULL);
    SDL_RenderPresent(app->renderer);
}

void mu_sdl_present(MuSdlApp *app, MuRenderContext *rc) {
    if (!app) return;
    mu_sdl_present_rect(app, rc, (MuRect){0.f, 0.f, (float)app->width, (float)app->height});
}

void mu_sdl_frame(MuContext *ctx, MuSdlApp *app) {
    if (!ctx || !app) return;

    mu_input_set_modifiers(ctx, sdl_modifiers(SDL_GetModState()));

    MuVec2 mp = {app->mouse_x, app->mouse_y};
    mu_input_update_hover(ctx, mp);

    for (int b = 0; b < 3; b++) {
        if (app->btn_down[b] == app->btn_was[b]) continue;
        MuPointerEvent pe = {0};
        pe.position = mp;
        pe.button = b;
        pe.modifiers = ctx->seat_modifiers;
        if (app->btn_down[b]) {
            pe.pressed = true;
            mu_popups_dispatch_pointer(ctx, &pe);
            mu_input_dispatch_pointer(ctx, &pe);
        } else {
            pe.released = true;
            mu_popups_dispatch_pointer(ctx, &pe);
            mu_input_dispatch_pointer(ctx, &pe);
        }
        app->btn_was[b] = app->btn_down[b];
    }

    if (app->mouse_down && ctx->captured_pointer_id) {
        MuPointerEvent pe = {0};
        pe.position = mp;
        pe.button = MU_BUTTON_LEFT;
        pe.drag = true;
        pe.modifiers = ctx->seat_modifiers;
        mu_input_dispatch_pointer(ctx, &pe);
    }

    MuNode *focused = mu_context_find_id(ctx, NULL, ctx->focused_id);
    bool want_text = focused && focused->role && strcmp(focused->role, "input") == 0;

    MuRect ime_area = {0};
    bool have_ime_area = mu_input_get_ime_caret_rect(ctx, &ime_area);
    if (!have_ime_area && want_text && focused) {
        ime_area = focused->bounds;
        have_ime_area = ime_area.w > 0.f && ime_area.h > 0.f;
    }

    if (have_ime_area != app->text_input_active) {
        if (have_ime_area) {
            SDL_StartTextInput(app->window);
        } else {
            SDL_StopTextInput(app->window);
        }
        app->text_input_active = have_ime_area;
    }
    if (app->text_input_active) {
        SDL_Rect area = {(int)ime_area.x, (int)ime_area.y, (int)ime_area.w, (int)ime_area.h};
        if (area.w < 1) area.w = 1;
        if (area.h < 1) area.h = 1;
        SDL_SetTextInputArea(app->window, &area, 0);
    }

    const bool *keys = SDL_GetKeyboardState(NULL);
    static bool tab_was = false;
    if (keys && keys[SDL_SCANCODE_TAB]) {
        if (!tab_was) mu_focus_advance_tab(ctx);
        tab_was = true;
    } else {
        tab_was = false;
    }

    if (app->wheel_x != 0.f || app->wheel_y != 0.f) {
        MuVec2 wheel_pt = app->wheel_pending ? (MuVec2){app->wheel_mouse_x, app->wheel_mouse_y} : mp;
        MuWheelEvent we = {wheel_pt, app->wheel_x, app->wheel_y, ctx->seat_modifiers};
        mu_input_dispatch_wheel(ctx, &we);
    }
    app->wheel_x = 0.f;
    app->wheel_y = 0.f;
    app->wheel_pending = false;
}