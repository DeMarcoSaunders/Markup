#include "mu_soft_internal.h"
#include "../include/markup/mu_path.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * Software path rasterizer.
 *
 * A fill pixel stores the fraction of its area inside the polygon (even-odd).
 * Interior runs are one opaque span; only the pixels an edge crosses are partial.
 * An edge that sits on a pixel boundary covers 0 or 1, so an integer rect stays
 * crisp — the same rule a square from mu_draw_rect follows.
 *
 * A stroke pixel stores clamp(half_width + 0.5 - distance), the same 1px filter
 * rounded rects use. Distance to a finite segment is a round cap, and the maximum
 * across the segments that meet at a vertex is a round join. That mask is blended
 * once, so a pixel shared by two segments takes the maximum instead of compositing
 * twice and going dark.
 *
 * Coverage is linear in the 8-bit value, matching the rounded-rect fringe, so the
 * two kinds of edge composite the same way.
 */

typedef struct SoftEdge {
    float x0, y0, x1, y1;
} SoftEdge;

static int soft_path_edges(const MuPath *path, SoftEdge *out, int cap) {
    if (!path || !out || cap <= 0) return 0;

    int n = 0;
    float sub_x = 0.f, sub_y = 0.f;
    float cur_x = 0.f, cur_y = 0.f;
    int have_sub = 0;
    int have_pt = 0;

    for (int i = 0; i < path->count; i++) {
        MuPathCmd cmd = path->cmds[i];
        if (cmd == MU_PATH_MOVE) {
            cur_x = path->xs[i];
            cur_y = path->ys[i];
            sub_x = cur_x;
            sub_y = cur_y;
            have_sub = 1;
            have_pt = 1;
            continue;
        }
        if (cmd == MU_PATH_LINE) {
            float nx = path->xs[i];
            float ny = path->ys[i];
            if (have_pt && n < cap) out[n++] = (SoftEdge){cur_x, cur_y, nx, ny};
            cur_x = nx;
            cur_y = ny;
            have_pt = 1;
            continue;
        }
        if (cmd == MU_PATH_CLOSE && have_sub && have_pt) {
            if (n < cap) out[n++] = (SoftEdge){cur_x, cur_y, sub_x, sub_y};
            cur_x = sub_x;
            cur_y = sub_y;
        }
    }
    return n;
}

/** Blend [x0, x1) at coverage/255 of `col`. Source alpha and coverage both apply. */
static void soft_blend_span(MuRenderContext *rc, int y, int x0, int x1, MuColor col, uint8_t coverage) {
    if (!rc || !rc->pixels || coverage == 0) return;

    MuSoftClip c = mu_soft_clip_now(rc);
    if (y < c.y0 || y >= c.y1) return;
    if (x0 < c.x0) x0 = c.x0;
    if (x1 > c.x1) x1 = c.x1;
    if (x1 <= x0) return;

    uint32_t alpha = mu_soft_mul255(col.a, coverage);
    if (alpha == 0u) return;

    uint32_t *row = rc->pixels + (size_t)y * (size_t)rc->stride;
    if (alpha == 255u) {
        uint32_t packed = mu_soft_pack(col);
        for (int x = x0; x < x1; x++) row[x] = packed;
        return;
    }
    for (int x = x0; x < x1; x++) row[x] = mu_soft_blend_over(row[x], col, alpha);
}

/**
 * Even-odd coverage from a signed pixel area.
 * Winding 0 and 2 are empty, winding 1 is solid, and a fractional winding is the
 * covered fraction of the pixel. Values that land a hair off an integer (a vertical
 * edge that should be exact) snap back so a pixel-aligned edge stays 0 or 255.
 */
static int coverage_byte(float area) {
    float a = fabsf(area);
    float nearest = roundf(a);
    if (fabsf(a - nearest) < 1e-4f) a = nearest;
    a = fmodf(a, 2.f);
    if (a < 0.f) a += 2.f;
    if (a > 1.f) a = 2.f - a;
    if (a <= 0.f) return 0;
    if (a >= 1.f) return 255;
    return (int)(a * 255.f + 0.5f);
}

/**
 * Area of x in [L, R), y in [y0, y1), that lies to the right of the segment.
 * The segment is y-monotonic with y0 < y1. x linear in y makes the midpoint of
 * each clipped piece an exact average, so this is the integral, not a sample.
 */
static float area_right_of_segment(float x0, float y0, float x1, float y1, float L, float R) {
    float h = y1 - y0;
    if (h <= 0.f || R <= L) return 0.f;

    float dx = x1 - x0;
    if (fabsf(dx) <= 1e-5f) {
        float x = 0.5f * (x0 + x1);
        if (x >= R) return 0.f;
        if (x <= L) return (R - L) * h;
        return (R - x) * h;
    }

    float y_l = y0 + (L - x0) * h / dx;
    float y_r = y0 + (R - x0) * h / dx;
    float splits[4];
    int n = 0;
    splits[n++] = y0;
    if (y_l > y0 && y_l < y1) splits[n++] = y_l;
    if (y_r > y0 && y_r < y1) {
        int dup = 0;
        for (int k = 0; k < n; k++) {
            if (fabsf(splits[k] - y_r) <= 1e-6f) dup = 1;
        }
        if (!dup) splits[n++] = y_r;
    }
    for (int i = 1; i < n; i++) {
        float v = splits[i];
        int j = i;
        while (j > 0 && splits[j - 1] > v) {
            splits[j] = splits[j - 1];
            j--;
        }
        splits[j] = v;
    }
    splits[n++] = y1;

    float area = 0.f;
    float inv_h = 1.f / h;
    for (int i = 0; i + 1 < n; i++) {
        float a = splits[i];
        float b = splits[i + 1];
        float dy = b - a;
        if (dy <= 0.f) continue;
        float xm = x0 + dx * ((0.5f * (a + b) - y0) * inv_h);
        if (xm >= R) continue;
        if (xm <= L) area += (R - L) * dy;
        else area += (R - xm) * dy;
    }
    return area;
}

/**
 * Add one scanline-clipped segment. `right[i]` is the signed area contributed to
 * pixel i and every pixel to its right; `partial[i]` is the extra area inside
 * pixel i when the segment crosses it. A later prefix sum turns that into coverage.
 */
static void accum_segment(float *partial, float *right, int origin, int count, float x0, float y0, float x1,
                          float y1, float dir) {
    float h = y1 - y0;
    if (h <= 1e-8f || count <= 0) return;

    float x_min = x0 < x1 ? x0 : x1;
    float x_max = x0 > x1 ? x0 : x1;
    int i0 = (int)floorf(x_min);
    int i1 = (int)ceilf(x_max); /* first pixel fully to the right of the segment */
    int end = origin + count;

    if (i1 <= origin) {
        right[0] += dir * h;
        return;
    }
    if (i0 >= end) return;

    int a = i0 > origin ? i0 : origin;
    int b = i1 < end ? i1 : end;
    for (int i = a; i < b; i++) {
        partial[i - origin] += dir * area_right_of_segment(x0, y0, x1, y1, (float)i, (float)(i + 1));
    }
    if (i1 >= origin && i1 < end) right[i1 - origin] += dir * h;
}

static void accum_edge_row(float *partial, float *right, int origin, int count, const SoftEdge *e, float y_top,
                           float y_bot) {
    float ax = e->x0, ay = e->y0, bx = e->x1, by = e->y1;
    float dir = 1.f;
    if (ay == by) return;
    if (ay > by) {
        float tx = ax, ty = ay;
        ax = bx;
        ay = by;
        bx = tx;
        by = ty;
        dir = -1.f;
    }
    if (by <= y_top || ay >= y_bot) return;
    if (ay < y_top) {
        float t = (y_top - ay) / (by - ay);
        ax = ax + t * (bx - ax);
        ay = y_top;
    }
    if (by > y_bot) {
        float t = (y_bot - ay) / (by - ay);
        bx = ax + t * (bx - ax);
        by = y_bot;
    }
    accum_segment(partial, right, origin, count, ax, ay, bx, by, dir);
}

static void emit_row(MuRenderContext *rc, int y, int origin, const float *partial, const float *right, int count,
                     MuColor col) {
    float sum = 0.f;
    int run_x = origin;
    int run_c = -1;
    for (int i = 0; i < count; i++) {
        sum += right[i];
        int c = coverage_byte(partial[i] + sum);
        if (c == run_c) continue;
        if (run_c > 0) soft_blend_span(rc, y, run_x, origin + i, col, (uint8_t)run_c);
        run_x = origin + i;
        run_c = c;
    }
    if (run_c > 0) soft_blend_span(rc, y, run_x, origin + count, col, (uint8_t)run_c);
}

static void soft_fill_even_odd(MuRenderContext *rc, const SoftEdge *edges, int edge_count, MuColor col) {
    if (!rc || !edges || edge_count <= 0 || col.a == 0) return;

    float min_x = edges[0].x0, max_x = edges[0].x0;
    float min_y = edges[0].y0, max_y = edges[0].y0;
    for (int i = 0; i < edge_count; i++) {
        SoftEdge e = edges[i];
        if (e.x0 < min_x) min_x = e.x0;
        if (e.x1 < min_x) min_x = e.x1;
        if (e.x0 > max_x) max_x = e.x0;
        if (e.x1 > max_x) max_x = e.x1;
        if (e.y0 < min_y) min_y = e.y0;
        if (e.y1 < min_y) min_y = e.y1;
        if (e.y0 > max_y) max_y = e.y0;
        if (e.y1 > max_y) max_y = e.y1;
    }

    MuSoftClip clip = mu_soft_clip_now(rc);
    int x0 = (int)floorf(min_x);
    int x1 = (int)ceilf(max_x);
    int y0 = (int)floorf(min_y);
    int y1 = (int)ceilf(max_y);
    if (x0 < clip.x0) x0 = clip.x0;
    if (y0 < clip.y0) y0 = clip.y0;
    if (x1 > clip.x1) x1 = clip.x1;
    if (y1 > clip.y1) y1 = clip.y1;
    int n = x1 - x0;
    if (n <= 0 || y1 <= y0) return;

    /* One row of partial coverage plus the "everything to the right" spill.
     * Edges that miss this box still contribute: a left edge outside the clip
     * lands in right[0], which is what keeps a scissored interior filled. */
    float stack[1024];
    float *buf = stack;
    if (n > 512) {
        buf = (float *)malloc(sizeof(float) * (size_t)n * 2u);
        if (!buf) return;
    }
    float *partial = buf;
    float *right = buf + n;

    for (int y = y0; y < y1; y++) {
        memset(buf, 0, sizeof(float) * (size_t)n * 2u);
        float top = (float)y;
        float bot = top + 1.f;
        for (int i = 0; i < edge_count; i++)
            accum_edge_row(partial, right, x0, n, &edges[i], top, bot);
        emit_row(rc, y, x0, partial, right, n, col);
    }

    if (buf != stack) free(buf);
}

static float segment_len2(const SoftEdge *e) {
    float dx = e->x1 - e->x0;
    float dy = e->y1 - e->y0;
    return dx * dx + dy * dy;
}

/** Coverage of one pixel center against one segment. 0 outside the 1px fringe. */
static int stroke_pixel_byte(float px, float py, float ax, float ay, float bx, float by, float ab2, float hw) {
    float t = 0.f;
    if (ab2 > 0.f) {
        t = ((px - ax) * (bx - ax) + (py - ay) * (by - ay)) / ab2;
        if (t < 0.f) t = 0.f;
        else if (t > 1.f) t = 1.f;
    }
    float dx = px - (ax + t * (bx - ax));
    float dy = py - (ay + t * (by - ay));
    float c = hw + 0.5f - sqrtf(dx * dx + dy * dy);
    if (c <= 0.f) return 0;
    if (c >= 1.f) return 255;
    return (int)(c * 255.f + 0.5f);
}

static void stamp_segment(uint8_t *mask, int ox, int oy, int sw, int sh, const SoftEdge *e, float hw) {
    float reach = hw + 0.5f;
    float ab2 = segment_len2(e);
    int x0 = (int)floorf((e->x0 < e->x1 ? e->x0 : e->x1) - reach);
    int y0 = (int)floorf((e->y0 < e->y1 ? e->y0 : e->y1) - reach);
    int x1 = (int)ceilf((e->x0 > e->x1 ? e->x0 : e->x1) + reach);
    int y1 = (int)ceilf((e->y0 > e->y1 ? e->y0 : e->y1) + reach);
    if (x0 < ox) x0 = ox;
    if (y0 < oy) y0 = oy;
    if (x1 > ox + sw) x1 = ox + sw;
    if (y1 > oy + sh) y1 = oy + sh;

    for (int y = y0; y < y1; y++) {
        float py = (float)y + 0.5f;
        uint8_t *row = mask + (size_t)(y - oy) * (size_t)sw;
        for (int x = x0; x < x1; x++) {
            int byte = stroke_pixel_byte((float)x + 0.5f, py, e->x0, e->y0, e->x1, e->y1, ab2, hw);
            if (byte > row[x - ox]) row[x - ox] = (uint8_t)byte;
        }
    }
}

static void blit_mask(MuRenderContext *rc, const uint8_t *mask, int ox, int oy, int sw, int sh, MuColor col) {
    for (int y = 0; y < sh; y++) {
        const uint8_t *row = mask + (size_t)y * (size_t)sw;
        int run_x = 0;
        int run_c = -1;
        for (int x = 0; x < sw; x++) {
            int c = row[x];
            if (c == run_c) continue;
            if (run_c > 0) soft_blend_span(rc, oy + y, ox + run_x, ox + x, col, (uint8_t)run_c);
            run_x = x;
            run_c = c;
        }
        if (run_c > 0) soft_blend_span(rc, oy + y, ox + run_x, ox + sw, col, (uint8_t)run_c);
    }
}

static void soft_stroke(MuRenderContext *rc, const SoftEdge *edges, int edge_count, MuColor col, float width) {
    if (!rc || !edges || edge_count <= 0 || col.a == 0 || width <= 0.f) return;

    float hw = width * 0.5f;
    float reach = hw + 0.5f;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    int have = 0;
    for (int i = 0; i < edge_count; i++) {
        if (segment_len2(&edges[i]) < 1e-8f) continue;
        float minx = edges[i].x0 < edges[i].x1 ? edges[i].x0 : edges[i].x1;
        float maxx = edges[i].x0 > edges[i].x1 ? edges[i].x0 : edges[i].x1;
        float miny = edges[i].y0 < edges[i].y1 ? edges[i].y0 : edges[i].y1;
        float maxy = edges[i].y0 > edges[i].y1 ? edges[i].y0 : edges[i].y1;
        int ex0 = (int)floorf(minx - reach);
        int ey0 = (int)floorf(miny - reach);
        int ex1 = (int)ceilf(maxx + reach);
        int ey1 = (int)ceilf(maxy + reach);
        if (!have) {
            x0 = ex0;
            y0 = ey0;
            x1 = ex1;
            y1 = ey1;
            have = 1;
        } else {
            if (ex0 < x0) x0 = ex0;
            if (ey0 < y0) y0 = ey0;
            if (ex1 > x1) x1 = ex1;
            if (ey1 > y1) y1 = ey1;
        }
    }
    if (!have) return;

    MuSoftClip clip = mu_soft_clip_now(rc);
    if (x0 < clip.x0) x0 = clip.x0;
    if (y0 < clip.y0) y0 = clip.y0;
    if (x1 > clip.x1) x1 = clip.x1;
    if (y1 > clip.y1) y1 = clip.y1;
    int sw = x1 - x0;
    int sh = y1 - y0;
    if (sw <= 0 || sh <= 0) return;
    if ((size_t)sw > (size_t)-1 / (size_t)sh) return;

    size_t cells = (size_t)sw * (size_t)sh;
    uint8_t stack[4096];
    uint8_t *mask = stack;
    if (cells > sizeof(stack)) {
        mask = (uint8_t *)malloc(cells);
        if (!mask) return;
    }
    memset(mask, 0, cells);

    for (int i = 0; i < edge_count; i++) {
        if (segment_len2(&edges[i]) < 1e-8f) continue;
        stamp_segment(mask, x0, y0, sw, sh, &edges[i], hw);
    }
    blit_mask(rc, mask, x0, y0, sw, sh, col);

    if (mask != stack) free(mask);
}

static void soft_draw_path_pixels(MuRenderContext *rc, const MuPath *path, MuColor fill, MuColor stroke,
                                  float stroke_w) {
    SoftEdge edges[MU_PATH_MAX];
    int n = soft_path_edges(path, edges, MU_PATH_MAX);
    if (n <= 0) return;
    if (fill.a > 0) soft_fill_even_odd(rc, edges, n, fill);
    if (stroke.a > 0 && stroke_w > 0.f) soft_stroke(rc, edges, n, stroke, stroke_w);
}

void mu_draw_path(MuRenderContext *rc, const MuPath *path, const MuDrawPathOpts *opts) {
    if (!rc || !path || !rc->pixels) return;

    MuDrawPathOpts defaults;
    if (!opts) {
        mu_draw_path_opts_init(&defaults);
        opts = &defaults;
    }

    float stroke_w = opts->stroke_width;
    if (stroke_w < 0.f) stroke_w = 0.f;

    float s = mu_soft_scale(rc);
    if (s == 1.f) {
        soft_draw_path_pixels(rc, path, opts->fill, opts->stroke, stroke_w);
        return;
    }

    /* Coverage is in device pixels. Scale first so the fringe is one physical pixel. */
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
    soft_draw_path_pixels(rc, &scaled, opts->fill, opts->stroke, stroke_w * s);
}
