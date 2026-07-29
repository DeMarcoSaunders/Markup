#include "../include/markup/mu_image.h"
#include <math.h>

void mu_draw_image_opts_init(MuDrawImageOpts *opts) {
    if (!opts) return;
    opts->fit = MU_IMAGE_FIT_CONTAIN;
    opts->tint = (MuColor){255, 255, 255, 255};
    opts->radius = 0.f;
    opts->src = (MuRect){0.f, 0.f, 0.f, 0.f};
}

bool mu_image_resolve_src(const MuRect *src, float tex_w, float tex_h, MuRect *out) {
    if (!out || tex_w < 1.f || tex_h < 1.f) return false;
    if (!src || src->w <= 0.f || src->h <= 0.f) {
        *out = (MuRect){0.f, 0.f, tex_w, tex_h};
        return true;
    }
    *out = *src;
    return out->w >= 1.f && out->h >= 1.f;
}

void mu_image_sheet_init(MuImageSheet *sheet) {
    if (!sheet) return;
    sheet->image_id = MU_IMAGE_INVALID;
    sheet->cell_w = 0.f;
    sheet->cell_h = 0.f;
    sheet->cols = 0;
}

bool mu_image_sheet_load(MuImageSheet *sheet, MuRenderContext *rc, const char *path, float cell_w, float cell_h,
                         int cols) {
    if (!sheet || !rc || !path || cell_w < 1.f || cell_h < 1.f || cols < 1) return false;
    uint32_t id = mu_image_load_file(rc, path);
    if (id == MU_IMAGE_INVALID) return false;
    sheet->image_id = id;
    sheet->cell_w = cell_w;
    sheet->cell_h = cell_h;
    sheet->cols = cols;
    return true;
}

MuRect mu_image_sheet_cell(const MuImageSheet *sheet, int index) {
    if (!sheet || sheet->image_id == MU_IMAGE_INVALID || sheet->cols < 1 || index < 0)
        return (MuRect){0.f, 0.f, 0.f, 0.f};
    int col = index % sheet->cols;
    int row = index / sheet->cols;
    return (MuRect){col * sheet->cell_w, row * sheet->cell_h, sheet->cell_w, sheet->cell_h};
}

MuRect mu_image_fit_dst(float src_w, float src_h, MuRect bounds, MuImageFit fit) {
    MuRect out = bounds;
    if (src_w < 1.f || src_h < 1.f || bounds.w < 1.f || bounds.h < 1.f)
        return out;

    if (fit == MU_IMAGE_FIT_FILL)
        return bounds;

    float scale;
    if (fit == MU_IMAGE_FIT_COVER)
        scale = (bounds.w / src_w > bounds.h / src_h) ? (bounds.w / src_w) : (bounds.h / src_h);
    else
        scale = (bounds.w / src_w < bounds.h / src_h) ? (bounds.w / src_w) : (bounds.h / src_h);

    float dw = src_w * scale;
    float dh = src_h * scale;
    out.w = dw;
    out.h = dh;
    out.x = bounds.x + (bounds.w - dw) * 0.5f;
    out.y = bounds.y + (bounds.h - dh) * 0.5f;
    return out;
}
