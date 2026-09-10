#include "../include/markup/mu_style.h"
#include "../include/markup/mu_text.h"
#include <stdlib.h>
#include <string.h>

static void resolve_text_defaults(const MuStyleModule *t, MuTextStyle *out) {
    mu_text_style_init(out);
    if (!t) {
        out->size = 16.f;
        out->weight = MU_TEXT_WEIGHT_NORMAL;
        out->letter_spacing = 1.f;
        return;
    }
    out->size = t->default_font_size;
    out->weight = t->default_font_weight > 0 ? t->default_font_weight : MU_TEXT_WEIGHT_NORMAL;
    out->italic = 0;
    out->letter_spacing = t->default_letter_spacing >= 0.f ? t->default_letter_spacing : 1.f;
}

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
    /* Low alpha: the blurred backdrop is the effect; these only lift and cool it. */
    m->glass_bg = (MuColor){255, 255, 255, 90};
    m->glass_border = (MuColor){255, 255, 255, 140};
    m->default_font_size = 16;
    m->default_font_weight = MU_TEXT_WEIGHT_NORMAL;
    m->default_letter_spacing = 1.f;
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
        resolve_text_defaults(NULL, &out->text);
        return;
    }

    resolve_text_defaults(t, &out->text);
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
    } else if (strcmp(r, "glass") == 0) {
        /* Deliberately translucent: the node's backdrop blur shows through this. An
         * opaque background here hides the effect completely — which is exactly what
         * happens if this branch is missing and the role falls through to panel_bg. */
        out->background = t->glass_bg;
        out->foreground = t->label_fg;
        out->border = t->glass_border;
        out->border_width = 1.f;
    } else if (strcmp(r, "subtitle") == 0) {
        out->foreground = t->muted_fg;
        out->background = (MuColor){0, 0, 0, 0};
        float fs = t->default_font_size - 1.f;
        out->text.size = fs < 11.f ? 11.f : fs;
    } else if (strcmp(r, "heading") == 0) {
        out->foreground = t->label_fg;
        out->background = (MuColor){0, 0, 0, 0};
        out->text.size = t->default_font_size + 5.f;
        out->text.weight = MU_TEXT_WEIGHT_BOLD;
    } else if (strcmp(r, "group") == 0) {
        out->background = (MuColor){0, 0, 0, 0};
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
    } else if (strcmp(r, "image") == 0) {
        out->background = (MuColor){0, 0, 0, 0};
        out->border = t->panel_border;
    } else if (strcmp(r, "tile") == 0) {
        out->background = t->panel_bg;
        out->border = t->panel_border;
        if (node->flags & MU_NODE_HOVERED) {
            out->background.r = (unsigned char)(out->background.r + 12 > 255 ? 255 : out->background.r + 12);
            out->background.g = (unsigned char)(out->background.g + 12 > 255 ? 255 : out->background.g + 12);
            out->background.b = (unsigned char)(out->background.b + 12 > 255 ? 255 : out->background.b + 12);
        }
        if (node->flags & MU_NODE_PRESSED) {
            out->background.r = (unsigned char)(out->background.r > 16 ? out->background.r - 16 : 0);
            out->background.g = (unsigned char)(out->background.g > 16 ? out->background.g - 16 : 0);
            out->background.b = (unsigned char)(out->background.b > 16 ? out->background.b - 16 : 0);
        }
    } else if (strcmp(r, "scroll") == 0) {
        out->background = t->input_bg;
        out->border = t->panel_border;
        out->foreground = t->slider_thumb;
    } else if (strcmp(r, "tabs") == 0) {
        out->background = t->panel_bg;
        out->border = t->panel_border;
    } else if (strcmp(r, "tab") == 0) {
        out->background = (MuColor){0, 0, 0, 0};
        out->foreground = t->muted_fg;
        if (node->flags & MU_NODE_HOVERED) {
            out->foreground = t->label_fg;
            out->background = t->button_bg_hover;
            out->background.a = 48;
        }
    } else if (strcmp(r, "tab-selected") == 0) {
        out->background = t->input_bg;
        out->foreground = t->label_fg;
        if (node->flags & MU_NODE_HOVERED) {
            out->background = t->button_bg_hover;
            out->background.a = 72;
        }
    } else if (strcmp(r, "listitem") == 0) {
        out->background = (MuColor){0, 0, 0, 0};
        out->foreground = t->label_fg;
        if (node->flags & MU_NODE_HOVERED) {
            out->background = t->button_bg_hover;
            out->background.a = 40;
        }
    } else if (strcmp(r, "listitem-selected") == 0) {
        out->background = t->button_bg;
        out->background.a = 56;
        out->foreground = t->label_fg;
        if (node->flags & MU_NODE_HOVERED) {
            out->background = t->button_bg_hover;
            out->background.a = 72;
        }
    } else {
        out->background = t->panel_bg;
        out->foreground = t->label_fg;
    }

    mu_text_style_merge(&out->text, &node->text);
}
