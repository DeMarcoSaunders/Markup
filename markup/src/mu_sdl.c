#include "../include/markup/mu_sdl.h"
#include "../include/markup/mu_input.h"
#include "../include/markup/mu_popup.h"
#include "../include/markup/mu_widgets_basic.h"

#include <SDL3/SDL.h>
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
};

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
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (ev.button.button == SDL_BUTTON_LEFT) app->mouse_down = true;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (ev.button.button == SDL_BUTTON_LEFT) app->mouse_down = false;
            break;
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
            if (ctx) {
                MuNode *focused = mu_context_find_id(ctx, NULL, ctx->focused_id);
                if (focused && focused->role && strcmp(focused->role, "input") == 0) {
                    MuKeyEvent ke = {MU_KEY_UNKNOWN, true, ev.key.repeat != 0};
                    SDL_Keycode sym = ev.key.key;
                    if (sym == SDLK_BACKSPACE) ke.key = MU_KEY_BACKSPACE;
                    else if (sym == SDLK_DELETE) ke.key = MU_KEY_DELETE;
                    else if (sym == SDLK_LEFT) ke.key = MU_KEY_LEFT;
                    else if (sym == SDLK_RIGHT) ke.key = MU_KEY_RIGHT;
                    else if (sym == SDLK_HOME) ke.key = MU_KEY_HOME;
                    else if (sym == SDLK_END) ke.key = MU_KEY_END;
                    if (ke.key != MU_KEY_UNKNOWN) mu_input_dispatch_key(ctx, &ke);
                }
            }
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

void mu_sdl_present(MuSdlApp *app, MuRenderContext *rc) {
    if (!app || !rc || !app->texture) return;
    int w = 0, h = 0, stride = 0;
    const void *pixels = mu_skia_pixel_data(rc, &w, &h, &stride);
    if (!pixels || w <= 0 || h <= 0) return;

    if (w != app->width || h != app->height) refresh_size(app);

    SDL_UpdateTexture(app->texture, NULL, pixels, stride);
    SDL_SetRenderDrawColor(app->renderer, 0, 0, 0, 255);
    SDL_RenderClear(app->renderer);
    SDL_RenderTexture(app->renderer, app->texture, NULL, NULL);
    SDL_RenderPresent(app->renderer);
}

void mu_sdl_frame(MuContext *ctx, MuSdlApp *app) {
    if (!ctx || !app) return;

    MuVec2 mp = {app->mouse_x, app->mouse_y};
    mu_input_update_hover(ctx, mp);

    static bool prev_down = false;
    MuPointerEvent pe = {0};
    pe.position = mp;
    pe.button = 0;

    if (app->mouse_down && !prev_down) {
        pe.pressed = true;
        mu_popups_dispatch_pointer(ctx, &pe);
        mu_input_dispatch_pointer(ctx, &pe);
    } else if (!app->mouse_down && prev_down) {
        pe.released = true;
        mu_popups_dispatch_pointer(ctx, &pe);
        mu_input_dispatch_pointer(ctx, &pe);
    } else if (app->mouse_down && ctx->captured_pointer_id) {
        pe.drag = true;
        mu_input_dispatch_pointer(ctx, &pe);
    }
    prev_down = app->mouse_down;

    MuNode *focused = mu_context_find_id(ctx, NULL, ctx->focused_id);
    bool want_text = focused && focused->role && strcmp(focused->role, "input") == 0;
    if (want_text != app->text_input_active) {
        if (want_text) {
            SDL_StartTextInput(app->window);
            SDL_Rect area = {(int)focused->bounds.x, (int)focused->bounds.y, (int)focused->bounds.w,
                             (int)focused->bounds.h};
            if (area.w < 1) area.w = 1;
            if (area.h < 1) area.h = 1;
            SDL_SetTextInputArea(app->window, &area, 0);
        } else {
            SDL_StopTextInput(app->window);
        }
        app->text_input_active = want_text;
    } else if (want_text && focused) {
        SDL_Rect area = {(int)focused->bounds.x, (int)focused->bounds.y, (int)focused->bounds.w,
                         (int)focused->bounds.h};
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
        mu_widgets_dispatch_wheel(ctx, wheel_pt, app->wheel_x, app->wheel_y);
    }
    app->wheel_x = 0.f;
    app->wheel_y = 0.f;
    app->wheel_pending = false;
}
