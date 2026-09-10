#include "../include/markup/mu_compose.h"
#include <string.h>

void mu_icon_desc_init(MuIconDesc *desc) {
    if (!desc) return;
    memset(desc, 0, sizeof(*desc));
    desc->image_id = MU_IMAGE_INVALID;
    desc->fit = MU_IMAGE_FIT_CONTAIN;
    desc->tint = (MuColor){255, 255, 255, 255};
}

void mu_compose_decorative(MuNode *node) {
    mu_node_set_hit_transparent(node, true);
}

static void compose_icon_layout(MuNode *icon, const MuIconDesc *d) {
    if (!icon || !d) return;
    float w = 0.f, h = 0.f;
    if (d->size > 0.f) {
        w = h = d->size;
    } else if (d->width > 0.f || d->height > 0.f) {
        w = d->width > 0.f ? d->width : d->height;
        h = d->height > 0.f ? d->height : d->width;
    }
    if (w > 0.f && h > 0.f) {
        mu_layout_set_min_size(icon, w, h);
        mu_layout_set_max_size(icon, w, h);
    }
}

static MuNode *compose_apply_icon(MuContext *ctx, MuRenderContext *rc, const MuIconDesc *d) {
    if (!ctx || !d) return NULL;

    MuNode *icon = mu_make_image(ctx, d->image_id);
    if (!icon) return NULL;

    if (d->path && rc) mu_image_set_source(icon, rc, d->path);
    if (d->src.w > 0.f && d->src.h > 0.f) mu_image_set_src_rect(icon, d->src);
    mu_image_set_fit(icon, d->fit);
    mu_image_set_tint(icon, d->tint);
    if (d->radius > 0.f) mu_image_set_radius(icon, d->radius);
    compose_icon_layout(icon, d);
    return icon;
}

MuNode *mu_compose_icon_chip(MuContext *ctx, MuRenderContext *rc, const MuIconDesc *desc) {
    if (!ctx || !desc) return NULL;

    const bool icon_only = !desc->label || !desc->label[0];
    const float icon_sz = desc->size > 0.f ? desc->size : 24.f;
    const float pad = icon_only ? 6.f : 10.f;

    MuNode *chip = mu_make_panel(ctx, icon_only ? false : true);
    chip->role = icon_only ? "group" : "tile";
    mu_layout_set_align_items(chip, MU_ALIGN_CENTER);
    mu_layout_set_gap(chip, icon_only ? 0.f : 6.f);
    if (icon_only) {
        mu_layout_set_flex_direction(chip, MU_FLEX_ROW);
        mu_layout_set_justify(chip, MU_JUSTIFY_CENTER);
        mu_layout_set_padding_all(chip, pad);
        const float box = icon_sz + pad * 2.f;
        mu_layout_set_min_size(chip, box, box);
        mu_layout_set_max_size(chip, box, box);
    } else {
        mu_layout_set_padding(chip, 8.f, pad, pad, pad);
    }
    if (desc->on_click) mu_panel_set_on_click(chip, desc->on_click, desc->user);

    MuNode *icon = compose_apply_icon(ctx, rc, desc);
    if (!icon) {
        mu_node_destroy_recursive(ctx, chip);
        return NULL;
    }
    mu_compose_decorative(icon);
    mu_node_add_child(ctx, chip, icon);

    if (desc->label && desc->label[0]) {
        MuNode *lbl = mu_make_label(ctx, desc->label);
        lbl->role = "subtitle";
        mu_compose_decorative(lbl);
        mu_node_add_child(ctx, chip, lbl);
    }
    return chip;
}

MuNode *mu_compose_icon_strip(MuContext *ctx, MuRenderContext *rc, const MuIconDesc *items, int count, float gap) {
    if (!ctx || !items || count <= 0) return NULL;

    MuNode *row = mu_make_panel(ctx, false);
    row->role = "group";
    mu_layout_set_flex_direction(row, MU_FLEX_ROW);
    mu_layout_set_gap(row, gap > 0.f ? gap : 8.f);
    mu_layout_set_align_items(row, MU_ALIGN_CENTER);
    mu_layout_set_padding_all(row, 0.f);

    for (int i = 0; i < count; i++) {
        const MuIconDesc *it = &items[i];
        MuNode *child = NULL;
        if (it->on_click || (it->label && it->label[0]))
            child = mu_compose_icon_chip(ctx, rc, it);
        else {
            child = compose_apply_icon(ctx, rc, it);
            if (child) mu_compose_decorative(child);
        }
        if (child) {
            child->layout.align_self = MU_ALIGN_SELF_CENTER;
            mu_node_add_child(ctx, row, child);
        }
    }
    return row;
}

MuNode *mu_compose_icon_carousel(MuContext *ctx, MuRenderContext *rc, const MuIconDesc *items, int count,
                                 float item_size, float viewport_w) {
    if (!ctx || !items || count <= 0) return NULL;

    float icon_sz = item_size > 0.f ? item_size : 48.f;
    bool labels = false;
    for (int i = 0; i < count; i++) {
        if (items[i].label && items[i].label[0]) {
            labels = true;
            break;
        }
    }
    float viewport_h = labels ? icon_sz + 36.f : icon_sz + 8.f;

    MuNode *scroll = mu_make_scroll(ctx, MU_SCROLL_HORIZONTAL);
    if (!scroll) return NULL;
    mu_layout_set_min_size(scroll, viewport_w > 0.f ? viewport_w : 0.f, viewport_h);
    mu_layout_set_max_size(scroll, viewport_w > 0.f ? viewport_w : 0.f, viewport_h);
    mu_layout_set_flex(scroll, 0.f, 0.f, viewport_h);

    MuNode *content = mu_scroll_content(scroll);
    if (!content) return scroll;
    mu_layout_set_flex_direction(content, MU_FLEX_ROW);
    mu_layout_set_gap(content, 12.f);
    mu_layout_set_align_items(content, MU_ALIGN_START);
    mu_layout_set_padding_all(content, 0.f);

    for (int i = 0; i < count; i++) {
        MuIconDesc item = items[i];
        if (item.size <= 0.f && item.width <= 0.f && item.height <= 0.f) item.size = icon_sz;
        MuNode *chip = mu_compose_icon_chip(ctx, rc, &item);
        if (chip) {
            chip->layout.flex_shrink = 0.f;
            mu_node_add_child(ctx, content, chip);
        }
    }
    return scroll;
}

MuNode *mu_compose_flow(MuContext *ctx, float gap) {
    if (!ctx) return NULL;
    MuNode *flow = mu_make_panel(ctx, false);
    flow->role = "group";
    mu_layout_set_flex_direction(flow, MU_FLEX_ROW);
    mu_layout_set_flex_wrap(flow, true);
    mu_layout_set_gap(flow, gap > 0.f ? gap : 12.f);
    mu_layout_set_align_items(flow, MU_ALIGN_START);
    mu_layout_set_padding_all(flow, 0.f);
    return flow;
}

MuNode *mu_compose_list(MuContext *ctx, float min_height) {
    if (!ctx) return NULL;
    return mu_make_list(ctx, min_height > 0.f ? min_height : 160.f);
}

MuNode *mu_compose_tabs(MuContext *ctx) {
    if (!ctx) return NULL;
    MuNode *tabs = mu_make_tabs(ctx);
    if (!tabs) return NULL;
    mu_layout_set_flex(tabs, 1.f, 1.f, 0.f);
    mu_layout_set_min_size(tabs, 0.f, 240.f);
    return tabs;
}
