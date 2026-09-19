#include "mu_soft_internal.h"
#include "mu_path_raster.h"
#include "../include/markup/mu_path.h"

static void soft_path_span(void *ctx, int y, int x0, int x1, MuColor col, uint8_t coverage) {
    MuRenderContext *rc = (MuRenderContext *)ctx;
    if (!rc || !rc->pixels) return;
    MuSoftClip c = mu_soft_clip_now(rc);
    if (y < c.y0 || y >= c.y1) return;
    if (x0 < c.x0) x0 = c.x0;
    if (x1 > c.x1) x1 = c.x1;
    if (x1 <= x0) return;

    uint32_t *row = rc->pixels + (size_t)y * (size_t)rc->stride;
    if (coverage >= 255) {
        for (int x = x0; x < x1; x++) row[x] = mu_soft_blend_over(row[x], col, col.a);
    } else {
        for (int x = x0; x < x1; x++) row[x] = mu_soft_blend_over(row[x], col, coverage);
    }
}

void mu_draw_path(MuRenderContext *rc, const MuPath *path, const MuDrawPathOpts *opts) {
    if (!rc || !path) return;

    MuDrawPathOpts defaults;
    if (!opts) {
        mu_draw_path_opts_init(&defaults);
        opts = &defaults;
    }

    float stroke_w = opts->stroke_width;
    if (stroke_w < 0.f) stroke_w = 0.f;

    float s = mu_soft_scale(rc);
    if (s != 1.f) {
        /* Scale path into pixel space for this surface. */
        MuPath scaled;
        mu_path_init(&scaled);
        for (int i = 0; i < path->count; i++) {
            float x = path->xs[i] * s;
            float y = path->ys[i] * s;
            switch (path->cmds[i]) {
            case MU_PATH_MOVE:
                mu_path_move_to(&scaled, x, y);
                break;
            case MU_PATH_LINE:
                mu_path_line_to(&scaled, x, y);
                break;
            case MU_PATH_CLOSE:
                mu_path_close(&scaled);
                break;
            }
        }
        if (opts->fill.a > 0) mu_path_raster_fill_even_odd(&scaled, soft_path_span, rc, opts->fill);
        if (opts->stroke.a > 0 && stroke_w > 0.f)
            mu_path_raster_stroke(&scaled, soft_path_span, rc, opts->stroke, stroke_w * s);
        return;
    }

    if (opts->fill.a > 0) mu_path_raster_fill_even_odd(path, soft_path_span, rc, opts->fill);
    if (opts->stroke.a > 0 && stroke_w > 0.f)
        mu_path_raster_stroke(path, soft_path_span, rc, opts->stroke, stroke_w);
}
