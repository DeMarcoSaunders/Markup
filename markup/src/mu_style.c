#include "../include/markup/mu_style.h"
#include <stdlib.h>
#include <string.h>

static void default_theme(MuStyleModule *m) {
    memset(m, 0, sizeof(*m));
    m->page_bg = (MuColor){248, 250, 252, 255};
    m->panel_bg = (MuColor){255, 255, 255, 255};
    m->panel_border = (MuColor){229, 231, 235, 255};
    m->toolbar_bg = (MuColor){243, 244, 246, 255};
    m->toolbar_border = (MuColor){209, 213, 219, 255};
    m->label_fg = (MuColor){31, 41, 55, 255};
    m->muted_fg = (MuColor){107, 114, 128, 255};
    m->button_bg = (MuColor){59, 130, 246, 255};
    m->button_fg = (MuColor){255, 255, 255, 255};
    m->button_border = (MuColor){37, 99, 235, 255};
    m->button_bg_hover = (MuColor){96, 165, 250, 255};
    m->button_fg_hover = (MuColor){255, 255, 255, 255};
    m->button_bg_pressed = (MuColor){37, 99, 235, 255};
    m->input_bg = (MuColor){255, 255, 255, 255};
    m->input_fg = (MuColor){31, 41, 55, 255};
    m->input_border = (MuColor){209, 213, 219, 255};
    m->slider_track = (MuColor){229, 231, 235, 255};
    m->slider_thumb = (MuColor){59, 130, 246, 255};
    m->checkbox_border = (MuColor){107, 114, 128, 255};
    m->checkbox_fill = (MuColor){59, 130, 246, 255};
    m->modal_overlay = (MuColor){0, 0, 0, 160};
    m->modal_bg = (MuColor){255, 255, 255, 255};
    m->default_font_size = 16;
    m->default_radius = 6;
}

void mu_style_init(MuContext *ctx, const MuStyleModule *initial) {
    if (!ctx) return;
    mu_style_shutdown(ctx);
    MuStyleModule *m = (MuStyleModule *)malloc(sizeof(MuStyleModule));
    if (!m) return;
    if (initial)
        *m = *initial;
    else
        default_theme(m);
    ctx->style = m;
}

void mu_style_shutdown(MuContext *ctx) {
    if (!ctx) return;
    free(ctx->style);
    ctx->style = NULL;
}

static const char *role_of(MuNode *node) {
    if (node && node->role) return node->role;
    return "";
}

void mu_style_resolve(MuContext *ctx, MuNode *node, MuStyleSnapshot *out) {
    memset(out, 0, sizeof(*out));
    if (!ctx || !node) return;
    const MuStyleModule *t = ctx->style;
    if (!t) {
        out->background = (MuColor){255, 255, 255, 255};
        out->foreground = (MuColor){0, 0, 0, 255};
        out->font_size = 16;
        return;
    }

    out->font_size = t->default_font_size;
    out->radius_tl = out->radius_tr = out->radius_br = out->radius_bl = t->default_radius;
    out->border_width = 1;

    const char *r = role_of(node);
    if (strcmp(r, "page") == 0) {
        out->background = t->page_bg;
    } else if (strcmp(r, "toolbar") == 0) {
        out->background = t->toolbar_bg;
        out->foreground = t->label_fg;
        out->border = t->toolbar_border;
        out->radius_tl = out->radius_tr = out->radius_br = out->radius_bl = 0.f;
        out->border_width = 1.f;
    } else if (strcmp(r, "subtitle") == 0) {
        out->foreground = t->muted_fg;
        out->background = (MuColor){0, 0, 0, 0};
        float fs = t->default_font_size - 1.f;
        out->font_size = fs < 11.f ? 11.f : fs;
    } else if (strcmp(r, "panel") == 0) {
        out->background = t->panel_bg;
        out->border = t->panel_border;
    } else if (strcmp(r, "label") == 0) {
        out->foreground = t->label_fg;
        out->background = (MuColor){0, 0, 0, 0};
    } else     if (strcmp(r, "button") == 0) {
        out->background = t->button_bg;
        out->foreground = t->button_fg;
        out->border = t->button_border;
        if (node->flags & MU_NODE_HOVERED)
            out->background = t->button_bg_hover;
        if (node->flags & MU_NODE_PRESSED)
            out->background = t->button_bg_pressed;
        if (node->flags & MU_NODE_DISABLED) {
            out->background.a = 120;
            out->foreground.a = 180;
        }
    } else if (strcmp(r, "input") == 0) {
        out->background = t->input_bg;
        out->foreground = t->input_fg;
        out->border = t->input_border;
    } else if (strcmp(r, "slider") == 0) {
        out->background = t->slider_track;
        out->foreground = t->slider_thumb;
    } else if (strcmp(r, "checkbox") == 0) {
        out->background = t->input_bg;
        out->border = t->checkbox_border;
        out->foreground = t->checkbox_fill;
    } else if (strcmp(r, "dropdown") == 0) {
        out->background = t->input_bg;
        out->foreground = t->input_fg;
        out->border = t->input_border;
    } else if (strcmp(r, "modal") == 0) {
        out->background = t->modal_bg;
        out->border = t->panel_border;
    } else if (strcmp(r, "toast") == 0) {
        out->background = (MuColor){31, 41, 55, 240};
        out->foreground = (MuColor){255, 255, 255, 255};
    } else {
        out->background = t->panel_bg;
        out->foreground = t->label_fg;
    }
}
