#include "mu_soft_internal.h"
#include "../include/markup/mu_text.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Pixel helpers
 * ------------------------------------------------------------------------ */

/* mu_soft_pack / mu_soft_unpack are inline in mu_soft.h, and mu_soft_mul255 /
 * mu_soft_blend_over in mu_soft_internal.h — they are inner-loop primitives and a
 * cross-translation-unit call cost more than the work they did. */

/* ---------------------------------------------------------------------------
 * Clipping
 * ------------------------------------------------------------------------ */

MuSoftClip mu_soft_clip_now(const MuRenderContext *rc) {
    if (rc->scissor_depth > 0) return rc->clip[rc->scissor_depth - 1];
    MuSoftClip full = {0, 0, rc->width, rc->height};
    return full;
}

void mu_push_scissor(MuRenderContext *rc, MuRect r) {
    if (!rc) return;
    if (rc->scissor_depth >= MU_SOFT_CLIP_MAX) {
        rc->scissor_overflow++; /* keep pop balanced rather than silently narrowing */
        return;
    }

    MuSoftClip cur = mu_soft_clip_now(rc);
    int x0 = (int)floorf(r.x);
    int y0 = (int)floorf(r.y);
    int x1 = (int)ceilf(r.x + r.w);
    int y1 = (int)ceilf(r.y + r.h);

    MuSoftClip n;
    n.x0 = x0 > cur.x0 ? x0 : cur.x0;
    n.y0 = y0 > cur.y0 ? y0 : cur.y0;
    n.x1 = x1 < cur.x1 ? x1 : cur.x1;
    n.y1 = y1 < cur.y1 ? y1 : cur.y1;
    if (n.x1 < n.x0) n.x1 = n.x0;
    if (n.y1 < n.y0) n.y1 = n.y0;

    rc->clip[rc->scissor_depth++] = n;
}

void mu_pop_scissor(MuRenderContext *rc) {
    if (!rc) return;
    if (rc->scissor_overflow > 0) {
        rc->scissor_overflow--;
        return;
    }
    if (rc->scissor_depth > 0) rc->scissor_depth--;
}

/* ---------------------------------------------------------------------------
 * Spans and rects
 * ------------------------------------------------------------------------ */

/** Fill [x0,x1) on row y with `col` at `coverage`/255. Clipped; no-op when empty. */
static void fill_span(MuRenderContext *rc, int y, int x0, int x1, MuColor col, uint32_t coverage) {
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

static void fill_rect_i(MuRenderContext *rc, int x0, int y0, int x1, int y1, MuColor col) {
    MuSoftClip c = mu_soft_clip_now(rc);
    if (y0 < c.y0) y0 = c.y0;
    if (y1 > c.y1) y1 = c.y1;
    for (int y = y0; y < y1; y++) fill_span(rc, y, x0, x1, col, 255u);
}

static void fill_rect_f(MuRenderContext *rc, float x, float y, float w, float h, MuColor col) {
    if (w <= 0.f || h <= 0.f) return;
    int x0 = (int)lroundf(x);
    int y0 = (int)lroundf(y);
    int x1 = (int)lroundf(x + w);
    int y1 = (int)lroundf(y + h);
    if (x1 <= x0 || y1 <= y0) return;
    fill_rect_i(rc, x0, y0, x1, y1, col);
}

/* ---------------------------------------------------------------------------
 * Rounded rectangles
 *
 * Coverage comes from a signed distance field rather than analytic scanline spans:
 * one distance function handles any radius, and the border falls out as the difference
 * between the outer and inner shapes' coverage, so fill and stroke share a code path.
 *
 * The SDF is only evaluated where the shape is actually curved or has a soft edge.
 * Rows in the straight vertical band get a solid span with a narrow antialiased fringe
 * at each end; only the corner bands are evaluated per pixel. For a 1200x800 panel at
 * radius 8 that is ~19k distance evaluations instead of ~960k.
 * ------------------------------------------------------------------------ */

/** Width of the antialiased fringe, in pixels, at a straight edge. */
#define MU_SOFT_EDGE_FRINGE 2

typedef struct MuRoundRect {
    float cx, cy;    /* centre */
    float hw, hh;    /* half extents */
    float r;         /* corner radius */
} MuRoundRect;

/** Signed distance from (px,py) to a rounded box centred at the origin. Negative inside. */
static float sdf_round_box(float px, float py, float hw, float hh, float r) {
    float qx = fabsf(px) - hw + r;
    float qy = fabsf(py) - hh + r;
    float ax = qx > 0.f ? qx : 0.f;
    float ay = qy > 0.f ? qy : 0.f;
    float m = qx > qy ? qx : qy;
    return sqrtf(ax * ax + ay * ay) + (m < 0.f ? m : 0.f) - r;
}

/** Pixel coverage in [0,1] from a signed distance, giving a 1px antialiased edge. */
static float rr_coverage(const MuRoundRect *s, float px, float py) {
    float d = sdf_round_box(px - s->cx, py - s->cy, s->hw, s->hh, s->r);
    float c = 0.5f - d;
    if (c <= 0.f) return 0.f;
    if (c >= 1.f) return 1.f;
    return c;
}

/**
 * Blend [x0,x1) on row y using SDF coverage. When `inner` is non-NULL the shape is a
 * ring: coverage is outer minus inner, which is what makes a rounded border a single
 * pass rather than four stroked edges.
 */
static void sdf_span(MuRenderContext *rc, int y, int x0, int x1, const MuRoundRect *outer,
                     const MuRoundRect *inner, MuColor col) {
    MuSoftClip c = mu_soft_clip_now(rc);
    if (y < c.y0 || y >= c.y1) return;
    if (x0 < c.x0) x0 = c.x0;
    if (x1 > c.x1) x1 = c.x1;
    if (x1 <= x0) return;

    uint32_t *row = rc->pixels + (size_t)y * (size_t)rc->stride;
    float py = (float)y + 0.5f;

    for (int x = x0; x < x1; x++) {
        float px = (float)x + 0.5f;
        float cov = rr_coverage(outer, px, py);
        if (inner) {
            cov -= rr_coverage(inner, px, py);
            if (cov <= 0.f) continue;
        } else if (cov <= 0.f) {
            continue;
        }
        if (cov > 1.f) cov = 1.f;

        uint32_t alpha = mu_soft_mul255(col.a, (uint32_t)(cov * 255.f + 0.5f));
        if (alpha == 0u) continue;
        row[x] = (alpha == 255u) ? mu_soft_pack(col) : mu_soft_blend_over(row[x], col, alpha);
    }
}

/**
 * Fill a rounded rect, or stroke its border when border_w > 0.
 * Caller guarantees box.w/h > 0 and radius > 0.
 */
static void draw_rounded(MuRenderContext *rc, MuRect box, float radius, float border_w, MuColor col) {
    float hw = box.w * 0.5f;
    float hh = box.h * 0.5f;
    float limit = hw < hh ? hw : hh;
    if (radius > limit) radius = limit;

    MuRoundRect outer = {box.x + hw, box.y + hh, hw, hh, radius};

    const bool ring = border_w > 0.f;
    float bw = border_w;
    if (bw > limit) bw = limit;

    MuRoundRect inner;
    if (ring) {
        float ir = radius - bw;
        if (ir < 0.f) ir = 0.f;
        inner = (MuRoundRect){outer.cx, outer.cy, hw - bw, hh - bw, ir};
    }

    MuSoftClip c = mu_soft_clip_now(rc);
    int y_start = (int)floorf(box.y);
    int y_end = (int)ceilf(box.y + box.h);
    int x_start = (int)floorf(box.x);
    int x_end = (int)ceilf(box.x + box.w);
    if (y_start < c.y0) y_start = c.y0;
    if (y_end > c.y1) y_end = c.y1;
    if (x_start < c.x0) x_start = c.x0;
    if (x_end > c.x1) x_end = c.x1;
    if (x_end <= x_start || y_end <= y_start) return;

    /* Rows where both outer and inner edges are vertical, so no corner curve is crossed. */
    float straight = radius > bw ? radius : bw;
    int band_y0 = (int)ceilf(box.y + straight);
    int band_y1 = (int)floorf(box.y + box.h - straight);

    /* Columns where both edges are horizontal — between the two corner curves. */
    int flat_x0 = (int)ceilf(box.x + straight);
    int flat_x1 = (int)floorf(box.x + box.w - straight);
    if (flat_x0 < x_start) flat_x0 = x_start;
    if (flat_x1 > x_end) flat_x1 = x_end;

    for (int y = y_start; y < y_end; y++) {
        if (y < band_y0 || y >= band_y1) {
            /* Corner band. Only the two corner columns curve; between them the edge is
             * horizontal, so coverage does not vary with x and one evaluation at the
             * centre serves the whole span. Without this, a wide rect pays for a full
             * row of distance evaluations to render a straight edge. */
            if (flat_x1 <= flat_x0) {
                sdf_span(rc, y, x_start, x_end, &outer, ring ? &inner : NULL, col);
                continue;
            }

            sdf_span(rc, y, x_start, flat_x0, &outer, ring ? &inner : NULL, col);

            float py = (float)y + 0.5f;
            float cov = rr_coverage(&outer, outer.cx, py);
            if (ring) cov -= rr_coverage(&inner, outer.cx, py);
            if (cov > 0.f) {
                if (cov > 1.f) cov = 1.f;
                fill_span(rc, y, flat_x0, flat_x1, col, (uint32_t)(cov * 255.f + 0.5f));
            }

            sdf_span(rc, y, flat_x1, x_end, &outer, ring ? &inner : NULL, col);
            continue;
        }

        if (ring) {
            /* Two vertical strips; the span between them lies inside `inner`. */
            int left_end = (int)ceilf(box.x + bw) + MU_SOFT_EDGE_FRINGE;
            int right_start = (int)floorf(box.x + box.w - bw) - MU_SOFT_EDGE_FRINGE;
            if (left_end > x_end) left_end = x_end;
            if (right_start < x_start) right_start = x_start;
            if (right_start <= left_end) {
                sdf_span(rc, y, x_start, x_end, &outer, &inner, col);
            } else {
                sdf_span(rc, y, x_start, left_end, &outer, &inner, col);
                sdf_span(rc, y, right_start, x_end, &outer, &inner, col);
            }
            continue;
        }

        /* Solid interior with an antialiased fringe at each end. */
        int mid0 = x_start + MU_SOFT_EDGE_FRINGE;
        int mid1 = x_end - MU_SOFT_EDGE_FRINGE;
        if (mid1 <= mid0) {
            sdf_span(rc, y, x_start, x_end, &outer, NULL, col);
            continue;
        }
        sdf_span(rc, y, x_start, mid0, &outer, NULL, col);
        fill_span(rc, y, mid0, mid1, col, 255u);
        sdf_span(rc, y, mid1, x_end, &outer, NULL, col);
    }
}

/* ---------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------ */

static MuRenderContext *g_measure_rc;

static MuRenderContext *measure_rc(MuRenderContext *rc) {
    return rc ? rc : g_measure_rc;
}

void mu_render_bind_measure(MuRenderContext *rc) {
    g_measure_rc = rc;
    mu_soft_text_bind(rc);
}

void mu_soft_render_init(MuRenderContext *rc, int width, int height) {
    if (!rc) return;
    memset(rc, 0, sizeof(*rc));
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    rc->pixels = (uint32_t *)calloc((size_t)width * (size_t)height, sizeof(uint32_t));
    if (!rc->pixels) return;
    rc->width = width;
    rc->height = height;
    rc->stride = width;
    rc->owns_pixels = true;
    rc->image_count = 1; /* slot 0 is MU_IMAGE_INVALID */
    mu_soft_text_init(rc);
}

void mu_soft_render_init_borrowed(MuRenderContext *rc, uint32_t *pixels, int width, int height, int stride) {
    if (!rc) return;
    memset(rc, 0, sizeof(*rc));
    if (!pixels || width < 1 || height < 1) return;
    rc->pixels = pixels;
    rc->width = width;
    rc->height = height;
    rc->stride = stride > 0 ? stride : width;
    rc->owns_pixels = false;
    rc->image_count = 1;
    mu_soft_text_init(rc);
}

void mu_soft_render_shutdown(MuRenderContext *rc) {
    if (!rc) return;
    mu_soft_text_shutdown(rc);
    for (int i = 0; i < rc->image_count; i++) {
        free(rc->images[i].pixels);
        rc->images[i].pixels = NULL;
    }
    rc->image_count = 0;
    free(rc->blur_scratch);
    rc->blur_scratch = NULL;
    rc->blur_scratch_cap = 0;
    mu_soft_backdrop_cache_free_all(rc);
    if (rc->owns_pixels) free(rc->pixels);
    rc->pixels = NULL;
    rc->width = rc->height = rc->stride = 0;
    rc->owns_pixels = false;
    rc->scissor_depth = 0;
    rc->scissor_overflow = 0;
    if (g_measure_rc == rc) g_measure_rc = NULL;
}

void mu_soft_resize(MuRenderContext *rc, int width, int height) {
    if (!rc || !rc->owns_pixels) return;
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    if (width == rc->width && height == rc->height) return;

    uint32_t *next = (uint32_t *)calloc((size_t)width * (size_t)height, sizeof(uint32_t));
    if (!next) return;
    free(rc->pixels);
    rc->pixels = next;
    rc->width = width;
    rc->height = height;
    rc->stride = width;
    rc->scissor_depth = 0;
    rc->scissor_overflow = 0;
    /* Every cached blur names a rectangle on the surface that just went away. */
    mu_soft_backdrop_cache_free_all(rc);
}

void mu_soft_begin_frame_rect(MuRenderContext *rc, MuColor clear, MuRect area) {
    if (!rc || !rc->pixels) return;
    rc->scissor_depth = 0;
    rc->scissor_overflow = 0;

    int x0 = (int)floorf(area.x), y0 = (int)floorf(area.y);
    int x1 = (int)ceilf(area.x + area.w), y1 = (int)ceilf(area.y + area.h);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > rc->width) x1 = rc->width;
    if (y1 > rc->height) y1 = rc->height;
    if (x1 <= x0 || y1 <= y0) return;

    uint32_t packed = mu_soft_pack(clear);
    for (int y = y0; y < y1; y++) {
        uint32_t *row = rc->pixels + (size_t)y * (size_t)rc->stride;
        for (int x = x0; x < x1; x++) row[x] = packed;
    }
}

void mu_soft_begin_frame(MuRenderContext *rc, MuColor clear) {
    if (!rc) return;
    mu_soft_begin_frame_rect(rc, clear, (MuRect){0.f, 0.f, (float)rc->width, (float)rc->height});
}

const void *mu_soft_pixel_data(MuRenderContext *rc, int *out_w, int *out_h, int *out_row_bytes) {
    if (!rc || !rc->pixels) return NULL;
    if (out_w) *out_w = rc->width;
    if (out_h) *out_h = rc->height;
    if (out_row_bytes) *out_row_bytes = rc->stride * (int)sizeof(uint32_t);
    return rc->pixels;
}

const void *mu_present_pixel_data(MuRenderContext *rc, int *out_w, int *out_h, int *out_row_bytes) {
    return mu_soft_pixel_data(rc, out_w, out_h, out_row_bytes);
}

MuColor mu_soft_get_pixel(const MuRenderContext *rc, int x, int y) {
    MuColor none = {0, 0, 0, 0};
    if (!rc || !rc->pixels) return none;
    if (x < 0 || y < 0 || x >= rc->width || y >= rc->height) return none;
    return mu_soft_unpack(rc->pixels[(size_t)y * (size_t)rc->stride + (size_t)x]);
}

/* ---------------------------------------------------------------------------
 * Drawing
 * ------------------------------------------------------------------------ */

void mu_draw_rect(MuRenderContext *rc, MuRect r, MuColor fill, MuColor border, float border_w, float radius) {
    if (!rc || !rc->pixels) return;
    if (r.w <= 0.f || r.h <= 0.f) return;
    if (radius < 0.f) radius = 0.f;

    /* Square corners stay on the snapped integer path: UI chrome (separators, overlays,
     * toolbars) reads as crisp rather than softened, and it avoids the SDF entirely. */
    if (radius <= 0.5f) {
        if (fill.a > 0) fill_rect_f(rc, r.x, r.y, r.w, r.h, fill);

        if (border_w > 0.f && border.a > 0) {
            float bw = border_w;
            if (bw > r.w * 0.5f) bw = r.w * 0.5f;
            if (bw > r.h * 0.5f) bw = r.h * 0.5f;
            float inner_h = r.h - 2.f * bw;
            fill_rect_f(rc, r.x, r.y, r.w, bw, border);
            fill_rect_f(rc, r.x, r.y + r.h - bw, r.w, bw, border);
            if (inner_h > 0.f) {
                fill_rect_f(rc, r.x, r.y + bw, bw, inner_h, border);
                fill_rect_f(rc, r.x + r.w - bw, r.y + bw, bw, inner_h, border);
            }
        }
        return;
    }

    if (fill.a > 0) draw_rounded(rc, r, radius, 0.f, fill);
    if (border_w > 0.f && border.a > 0) draw_rounded(rc, r, radius, border_w, border);
}

/* mu_draw_text, mu_text_measure and mu_font_load_file live in mu_soft_text.c. */

/* ---------------------------------------------------------------------------
 * Images
 * ------------------------------------------------------------------------ */

uint32_t mu_image_load_file(MuRenderContext *rc, const char *path) {
    if (!rc || !path || !path[0]) return MU_IMAGE_INVALID;

    MuImageRgba src = {0};
    if (!mu_image_decode_rgba_file(path, &src)) return MU_IMAGE_INVALID;

    /* mu_image_rgba_free zeroes width/height, so store before releasing the decode. */
    uint32_t id = mu_soft_image_from_rgba(rc, src.pixels, src.width, src.height, src.stride);
    mu_image_rgba_free(&src);
    return id;
}

uint32_t mu_soft_image_from_rgba(MuRenderContext *rc, const uint8_t *rgba, int width, int height,
                                 int stride) {
    if (!rc || !rgba || width < 1 || height < 1) return MU_IMAGE_INVALID;
    if (rc->image_count >= (int)MU_IMAGE_MAX) return MU_IMAGE_INVALID;
    if (stride <= 0) stride = width * 4;

    uint32_t *argb = (uint32_t *)malloc((size_t)width * (size_t)height * sizeof(uint32_t));
    if (!argb) return MU_IMAGE_INVALID;

    /* Stored PREMULTIPLIED. Filtering straight alpha blends the colour of fully
     * transparent texels into neighbours and halos every soft edge, and premultiplying
     * here rather than per sample keeps the inner blit loop to weighted adds. */
    for (int y = 0; y < height; y++) {
        const uint8_t *row = rgba + (size_t)y * (size_t)stride;
        uint32_t *dst = argb + (size_t)y * (size_t)width;
        for (int x = 0; x < width; x++) {
            const uint8_t *p = row + (size_t)x * 4u;
            uint32_t a = p[3];
            dst[x] = (a << 24) | (mu_soft_mul255(p[0], a) << 16) | (mu_soft_mul255(p[1], a) << 8) |
                     mu_soft_mul255(p[2], a);
        }
    }

    int id = rc->image_count++;
    rc->images[id].pixels = argb;
    rc->images[id].width = width;
    rc->images[id].height = height;
    return (uint32_t)id;
}

bool mu_image_get_size(MuRenderContext *rc, uint32_t image_id, float *out_w, float *out_h) {
    rc = measure_rc(rc);
    if (!rc || image_id == MU_IMAGE_INVALID || image_id >= (uint32_t)rc->image_count) return false;
    if (!rc->images[image_id].pixels) return false;
    if (out_w) *out_w = (float)rc->images[image_id].width;
    if (out_h) *out_h = (float)rc->images[image_id].height;
    return true;
}

static MuSoftImageSlot *soft_resolve_image(MuRenderContext *rc, uint32_t image_id) {
    if (!rc || image_id == MU_IMAGE_INVALID || image_id >= (uint32_t)rc->image_count) return NULL;
    if (!rc->images[image_id].pixels) return NULL;
    return &rc->images[image_id];
}

/** Tap bounds for a source rect, clamped to the image. Hoisted out of the blit loop. */
typedef struct MuSampleBounds {
    int min_x, min_y, max_x, max_y;
} MuSampleBounds;

/**
 * Bilinear sample at integer texel (x0,y0) with 8-bit sub-texel offsets (tx,ty).
 *
 * Texels are already premultiplied (see mu_soft_image_from_rgba), so this is four
 * weighted adds per channel with no per-tap alpha work. The four weight products sum to
 * exactly 65536.
 *
 * A 16.16 fixed-point stepper was tried here to avoid floorf and the float-to-int
 * conversion. It measured no better than this — the difference sat inside run-to-run
 * noise — so the simpler float form was kept. floorf compiles to a single roundss on
 * x86, which is likely why there was nothing to win.
 *
 * Taps clamp to the *source rect*, not the whole image, so sprite-sheet cells cannot
 * bleed into their neighbours.
 *
 * Output is indexed by ARGB byte position: [0]=b, [1]=g, [2]=r, [3]=a.
 */
static void sample_bilinear_premul(const MuSoftImageSlot *img, const MuSampleBounds *b, float u, float v,
                                   uint32_t out[4]) {
    float fx = u - 0.5f, fy = v - 0.5f;
    int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
    uint32_t tx = (uint32_t)((fx - (float)x0) * 256.f);
    uint32_t ty = (uint32_t)((fy - (float)y0) * 256.f);
    if (tx > 256u) tx = 256u;
    if (ty > 256u) ty = 256u;

    int x1 = x0 + 1, y1 = y0 + 1;
    if (x0 < b->min_x) x0 = b->min_x; else if (x0 > b->max_x) x0 = b->max_x;
    if (x1 < b->min_x) x1 = b->min_x; else if (x1 > b->max_x) x1 = b->max_x;
    if (y0 < b->min_y) y0 = b->min_y; else if (y0 > b->max_y) y0 = b->max_y;
    if (y1 < b->min_y) y1 = b->min_y; else if (y1 > b->max_y) y1 = b->max_y;

    const uint32_t *row0 = img->pixels + (size_t)y0 * (size_t)img->width;
    const uint32_t *row1 = img->pixels + (size_t)y1 * (size_t)img->width;
    uint32_t p00 = row0[x0], p10 = row0[x1], p01 = row1[x0], p11 = row1[x1];

    uint32_t ix = 256u - tx, iy = 256u - ty;
    uint32_t w00 = ix * iy, w10 = tx * iy, w01 = ix * ty, w11 = tx * ty;

    for (int ch = 0; ch < 4; ch++) {
        int sh = ch * 8;
        uint32_t s = ((p00 >> sh) & 0xFFu) * w00 + ((p10 >> sh) & 0xFFu) * w10 +
                     ((p01 >> sh) & 0xFFu) * w01 + ((p11 >> sh) & 0xFFu) * w11;
        out[ch] = (s + 32768u) >> 16;
    }
}

void mu_draw_image(MuRenderContext *rc, uint32_t image_id, MuRect dst, const MuDrawImageOpts *opts) {
    if (!rc || !rc->pixels || dst.w < 1.f || dst.h < 1.f) return;
    MuSoftImageSlot *img = soft_resolve_image(rc, image_id);
    if (!img) return;

    MuDrawImageOpts defaults;
    if (!opts) {
        mu_draw_image_opts_init(&defaults);
        opts = &defaults;
    }

    MuRect src_px;
    if (!mu_image_resolve_src(&opts->src, (float)img->width, (float)img->height, &src_px)) return;
    MuRect draw = mu_image_fit_dst(src_px.w, src_px.h, dst, opts->fit);
    if (draw.w < 1.f || draw.h < 1.f) return;

    /* MU_IMAGE_FIT_COVER scales past `dst` on one axis, so confine the blit to the box
     * the caller asked for — nothing should paint outside its own bounds. (The Skia
     * backend does not do this and will overflow; that is a bug on that path.) */
    float bx0 = draw.x > dst.x ? draw.x : dst.x;
    float by0 = draw.y > dst.y ? draw.y : dst.y;
    float bx1 = (draw.x + draw.w) < (dst.x + dst.w) ? (draw.x + draw.w) : (dst.x + dst.w);
    float by1 = (draw.y + draw.h) < (dst.y + dst.h) ? (draw.y + draw.h) : (dst.y + dst.h);
    if (bx1 <= bx0 || by1 <= by0) return;

    /* Corner rounding applies to the visible box, so CONTAIN rounds the letterboxed image
     * while COVER rounds the destination it fills. */
    const bool round = opts->radius > 0.5f;
    float hw = (bx1 - bx0) * 0.5f, hh = (by1 - by0) * 0.5f;
    float limit = hw < hh ? hw : hh;
    float radius = opts->radius > limit ? limit : opts->radius;
    MuRoundRect rr = {bx0 + hw, by0 + hh, hw, hh, radius};

    MuSoftClip clip = mu_soft_clip_now(rc);
    int x0 = (int)floorf(bx0), x1 = (int)ceilf(bx1);
    int y0 = (int)floorf(by0), y1 = (int)ceilf(by1);
    if (x0 < clip.x0) x0 = clip.x0;
    if (y0 < clip.y0) y0 = clip.y0;
    if (x1 > clip.x1) x1 = clip.x1;
    if (y1 > clip.y1) y1 = clip.y1;
    if (x1 <= x0 || y1 <= y0) return;

    const MuColor tint = opts->tint;
    const bool plain_tint = (tint.r == 255 && tint.g == 255 && tint.b == 255 && tint.a == 255);
    const float u_scale = src_px.w / draw.w;
    const float v_scale = src_px.h / draw.h;

    MuSampleBounds sb;
    sb.min_x = (int)src_px.x;
    sb.min_y = (int)src_px.y;
    sb.max_x = (int)(src_px.x + src_px.w) - 1;
    sb.max_y = (int)(src_px.y + src_px.h) - 1;
    if (sb.min_x < 0) sb.min_x = 0;
    if (sb.min_y < 0) sb.min_y = 0;
    if (sb.max_x > img->width - 1) sb.max_x = img->width - 1;
    if (sb.max_y > img->height - 1) sb.max_y = img->height - 1;
    if (sb.max_x < sb.min_x || sb.max_y < sb.min_y) return;

    for (int y = y0; y < y1; y++) {
        float py = (float)y + 0.5f;
        float v = src_px.y + (py - draw.y) * v_scale;
        uint32_t *row = rc->pixels + (size_t)y * (size_t)rc->stride;

        for (int x = x0; x < x1; x++) {
            float px = (float)x + 0.5f;

            uint32_t cov = 255u;
            if (round) {
                float c = rr_coverage(&rr, px, py);
                if (c <= 0.f) continue;
                cov = (uint32_t)(c * 255.f + 0.5f);
            }

            uint32_t s[4]; /* premultiplied, ARGB byte order: b, g, r, a */
            sample_bilinear_premul(img, &sb, src_px.x + (px - draw.x) * u_scale, v, s);
            uint32_t a = s[3];
            if (a == 0u) continue;

            uint32_t r = s[2], g = s[1], b = s[0];
            if (!plain_tint) {
                /* Modulating premultiplied colour by tint.a as well keeps it premultiplied. */
                uint32_t ta = tint.a;
                r = mu_soft_mul255(mu_soft_mul255(r, tint.r), ta);
                g = mu_soft_mul255(mu_soft_mul255(g, tint.g), ta);
                b = mu_soft_mul255(mu_soft_mul255(b, tint.b), ta);
                a = mu_soft_mul255(a, ta);
            }
            if (cov != 255u) {
                r = mu_soft_mul255(r, cov);
                g = mu_soft_mul255(g, cov);
                b = mu_soft_mul255(b, cov);
                a = mu_soft_mul255(a, cov);
            }
            if (a == 0u) continue;

            uint32_t d = row[x];
            uint32_t inv = 255u - a;
            uint32_t dr = (d >> 16) & 0xFFu, dg = (d >> 8) & 0xFFu, db = d & 0xFFu;
            uint32_t da = (d >> 24) & 0xFFu;

            row[x] = ((a + mu_soft_mul255(da, inv)) << 24) | ((r + mu_soft_mul255(dr, inv)) << 16) |
                     ((g + mu_soft_mul255(dg, inv)) << 8) | (b + mu_soft_mul255(db, inv));
        }
    }
}
