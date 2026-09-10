#include "mu_soft_internal.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/*
 * Backdrop blur — the frosted glass primitive.
 *
 * Reading the backdrop is free here: the software backend owns its pixel buffer, so
 * there is no framebuffer copy or render-target dance. This is the one place the CPU
 * renderer has a structural advantage over a GPU one.
 *
 * The blur is three passes of a separable box blur, which converges close enough to a
 * Gaussian that the difference is invisible at UI scale. Each pass uses a sliding window
 * sum, so cost per pixel is constant regardless of radius — a 40px blur costs the same
 * as a 4px one.
 *
 * Source pixels are read from a region inflated by the blur reach and clamped to the
 * surface, with edge pixels extended. Sampling only the visible rect instead would pull
 * in nothing beyond its border and darken the edges, which reads as a vignette.
 *
 * Note this reads the surface *outside* the current scissor on purpose: clipping governs
 * what may be written, while the blur legitimately needs to see its surroundings. Writing
 * stays clipped.
 */

/* Three box passes approximate a Gaussian; this splits the requested radius across them. */
#define MU_BLUR_PASSES 3
#define MU_BLUR_MAX_RADIUS 128

/*
 * Above this radius the blur runs at half resolution: the backdrop is box-downsampled
 * 2x on the way in, blurred with half the radius, and bilinearly upsampled during the
 * composite. That is a quarter of the pixels through six passes, and it also skips the
 * full-resolution copy entirely.
 *
 * The result is not identical, but a blur is a low-pass filter — the detail lost to
 * downsampling is detail the blur was about to destroy. Below the threshold the sample
 * region is small enough that full resolution is cheap, and half-res artifacts would
 * start to be visible relative to a barely-blurred image, so small radii stay exact.
 */
#define MU_BLUR_HALFRES_MIN 4.f
#define MU_BLUR_QUARTERRES_MIN 12.f
#define MU_BLUR_MAX_SCALE 4

/**
 * Downsample factor for a given radius.
 *
 * The wider the blur, the more aggressively it can be downsampled: everything the
 * downsample discards is finer than what the blur is about to erase anyway. Below
 * MU_BLUR_HALFRES_MIN the image is barely blurred, so any resampling would be the most
 * visible thing in it and full resolution is used instead.
 *
 * Cost scales with the square of this, and it dominates: the blur passes are a serial
 * running-sum with little instruction-level parallelism, so reducing the pixel count is
 * the only large lever available short of SIMD.
 */
static int blur_scale_for(float radius) {
    if (radius >= MU_BLUR_QUARTERRES_MIN) return 4;
    if (radius >= MU_BLUR_HALFRES_MIN) return 2;
    return 1;
}

static int box_radius_for(float blur_radius) {
    int r = (int)(blur_radius / (float)MU_BLUR_PASSES + 0.5f);
    if (r < 1) r = 1;
    if (r > MU_BLUR_MAX_RADIUS) r = MU_BLUR_MAX_RADIUS;
    return r;
}

/** Grow the scratch pair to hold `pixels` each. Kept on the context to avoid per-frame malloc. */
static bool scratch_reserve(MuRenderContext *rc, int pixels) {
    if (pixels <= rc->blur_scratch_cap) return true;
    int cap = rc->blur_scratch_cap ? rc->blur_scratch_cap : 4096;
    while (cap < pixels) cap *= 2;
    uint32_t *next = (uint32_t *)realloc(rc->blur_scratch, (size_t)cap * 2u * sizeof(uint32_t));
    if (!next) return false;
    rc->blur_scratch = next;
    rc->blur_scratch_cap = cap;
    return true;
}

/*
 * Dividing by the window width per channel per pass costs 24 integer divisions per
 * pixel, which measured as the dominant cost by a wide margin. A precomputed 16.16
 * reciprocal turns each into a multiply and a shift.
 *
 * The product cannot overflow: the running sum is at most 255*window and the reciprocal
 * is about 65536/window, so their product stays near 255*65536 regardless of radius.
 * The +32768 rounds rather than truncates, which matters — truncation loses a count on
 * window sizes that do not divide 65536, and a uniform field would then decay slightly
 * every time it was blurred.
 *
 * Alpha is skipped entirely. The composite writes an opaque result, so blurring the
 * alpha channel is a quarter of the work thrown away.
 */
static uint32_t window_reciprocal(int window) {
    return (uint32_t)((65536 + window / 2) / window);
}

#define BOX_AVG(sum, recip) ((((sum) * (recip) + 32768u) >> 16) & 0xFFu)

/** One horizontal box pass with a sliding window; edges clamp to the first/last pixel. */
static void box_blur_h(const uint32_t *src, uint32_t *dst, int w, int h, int radius) {
    const int window = radius * 2 + 1;
    const uint32_t recip = window_reciprocal(window);

    for (int y = 0; y < h; y++) {
        const uint32_t *srow = src + (size_t)y * (size_t)w;
        uint32_t *drow = dst + (size_t)y * (size_t)w;

        uint32_t sr = 0, sg = 0, sb = 0;
        /* Prime the window: everything left of 0 clamps to pixel 0. */
        for (int i = -radius; i <= radius; i++) {
            int x = i < 0 ? 0 : (i >= w ? w - 1 : i);
            uint32_t p = srow[x];
            sr += (p >> 16) & 0xFFu;
            sg += (p >> 8) & 0xFFu;
            sb += p & 0xFFu;
        }

        for (int x = 0; x < w; x++) {
            drow[x] = (BOX_AVG(sr, recip) << 16) | (BOX_AVG(sg, recip) << 8) | BOX_AVG(sb, recip);

            int add = x + radius + 1;
            int sub = x - radius;
            add = add >= w ? w - 1 : add;
            sub = sub < 0 ? 0 : sub;
            uint32_t pa = srow[add], ps = srow[sub];
            sr += ((pa >> 16) & 0xFFu) - ((ps >> 16) & 0xFFu);
            sg += ((pa >> 8) & 0xFFu) - ((ps >> 8) & 0xFFu);
            sb += (pa & 0xFFu) - (ps & 0xFFu);
        }
    }
}

/*
 * Vertical counterpart, over strips of columns.
 *
 * The obvious form — one column at a time, walking down — reads with a stride of `w` and
 * misses cache on essentially every access; it measured as roughly three quarters of the
 * total blur cost. Carrying a running sum for MU_BLUR_VSTRIP adjacent columns at once
 * turns every read back into a contiguous run, at the price of a handful of sums in
 * registers.
 */
#define MU_BLUR_VSTRIP 16

static void box_blur_v(const uint32_t *src, uint32_t *dst, int w, int h, int radius) {
    const int window = radius * 2 + 1;
    const uint32_t recip = window_reciprocal(window);
    uint32_t sr[MU_BLUR_VSTRIP], sg[MU_BLUR_VSTRIP], sb[MU_BLUR_VSTRIP];

    for (int x0 = 0; x0 < w; x0 += MU_BLUR_VSTRIP) {
        int n = w - x0;
        if (n > MU_BLUR_VSTRIP) n = MU_BLUR_VSTRIP;

        for (int i = 0; i < n; i++) sr[i] = sg[i] = sb[i] = 0;

        for (int k = -radius; k <= radius; k++) {
            int y = k < 0 ? 0 : (k >= h ? h - 1 : k);
            const uint32_t *row = src + (size_t)y * (size_t)w + (size_t)x0;
            for (int i = 0; i < n; i++) {
                uint32_t p = row[i];
                sr[i] += (p >> 16) & 0xFFu;
                sg[i] += (p >> 8) & 0xFFu;
                sb[i] += p & 0xFFu;
            }
        }

        for (int y = 0; y < h; y++) {
            uint32_t *drow = dst + (size_t)y * (size_t)w + (size_t)x0;
            for (int i = 0; i < n; i++)
                drow[i] = (BOX_AVG(sr[i], recip) << 16) | (BOX_AVG(sg[i], recip) << 8) |
                          BOX_AVG(sb[i], recip);

            int add = y + radius + 1;
            int sub = y - radius;
            add = add >= h ? h - 1 : add;
            sub = sub < 0 ? 0 : sub;
            const uint32_t *arow = src + (size_t)add * (size_t)w + (size_t)x0;
            const uint32_t *brow = src + (size_t)sub * (size_t)w + (size_t)x0;
            for (int i = 0; i < n; i++) {
                uint32_t pa = arow[i], ps = brow[i];
                sr[i] += ((pa >> 16) & 0xFFu) - ((ps >> 16) & 0xFFu);
                sg[i] += ((pa >> 8) & 0xFFu) - ((ps >> 8) & 0xFFu);
                sb[i] += (pa & 0xFFu) - (ps & 0xFFu);
            }
        }
    }
}

/** Signed distance to a rounded box, mirroring the shape used by mu_draw_rect. */
static float blur_sdf_round_box(float px, float py, float hw, float hh, float r) {
    float qx = fabsf(px) - hw + r;
    float qy = fabsf(py) - hh + r;
    float ax = qx > 0.f ? qx : 0.f;
    float ay = qy > 0.f ? qy : 0.f;
    float m = qx > qy ? qx : qy;
    return sqrtf(ax * ax + ay * ay) + (m < 0.f ? m : 0.f) - r;
}

/**
 * The pixels this request is allowed to write: the area, rounded out, clipped to the
 * scissor. Shared with the cache so a stored snapshot and a replay of it can only ever
 * describe the same rectangle.
 */
static bool blur_write_rect(const MuRenderContext *rc, MuRect area, int *ox0, int *oy0, int *ox1,
                            int *oy1) {
    MuSoftClip clip = mu_soft_clip_now(rc);
    int x0 = (int)floorf(area.x), y0 = (int)floorf(area.y);
    int x1 = (int)ceilf(area.x + area.w), y1 = (int)ceilf(area.y + area.h);
    if (x0 < clip.x0) x0 = clip.x0;
    if (y0 < clip.y0) y0 = clip.y0;
    if (x1 > clip.x1) x1 = clip.x1;
    if (y1 > clip.y1) y1 = clip.y1;
    if (x1 <= x0 || y1 <= y0) return false;
    *ox0 = x0;
    *oy0 = y0;
    *ox1 = x1;
    *oy1 = y1;
    return true;
}

void mu_draw_backdrop_blur(MuRenderContext *rc, MuRect area, float blur_radius, MuColor tint,
                           float corner_radius) {
    if (!rc || !rc->pixels) return;
    if (area.w <= 0.f || area.h <= 0.f) return;
    if (blur_radius < 0.f) blur_radius = 0.f;

    /* Where we are allowed to write. */
    int wx0, wy0, wx1, wy1;
    if (!blur_write_rect(rc, area, &wx0, &wy0, &wx1, &wy1)) return;

    /* Blur radius is applied in working-buffer space, so the reach has to be scaled back
     * up to decide how much full-resolution backdrop to read. */
    const int scale = blur_scale_for(blur_radius);
    const int box_r = box_radius_for(blur_radius / (float)scale);
    const int reach = (box_r * MU_BLUR_PASSES + 1) * scale;

    /* Where we read from: the write area grown by the blur reach, clamped to the surface
     * but deliberately NOT to the scissor. */
    int sx0 = wx0 - reach, sy0 = wy0 - reach;
    int sx1 = wx1 + reach, sy1 = wy1 + reach;
    if (sx0 < 0) sx0 = 0;
    if (sy0 < 0) sy0 = 0;
    if (sx1 > rc->width) sx1 = rc->width;
    if (sy1 > rc->height) sy1 = rc->height;

    const int sw = sx1 - sx0, sh = sy1 - sy0;
    if (sw <= 0 || sh <= 0) return;

    /* Working buffer dimensions, at whatever resolution we are blurring at. */
    const int bw = (sw + scale - 1) / scale;
    const int bh = (sh + scale - 1) / scale;
    if (bw <= 0 || bh <= 0) return;
    if (!scratch_reserve(rc, bw * bh)) return;

    uint32_t *buf_a = rc->blur_scratch;
    uint32_t *buf_b = rc->blur_scratch + rc->blur_scratch_cap;

    if (scale == 1) {
        for (int y = 0; y < sh; y++) {
            const uint32_t *srow = rc->pixels + (size_t)(sy0 + y) * (size_t)rc->stride + (size_t)sx0;
            memcpy(buf_a + (size_t)y * (size_t)sw, srow, (size_t)sw * sizeof(uint32_t));
        }
    } else {
        /* Box-average each scale x scale block straight out of the surface, which also
         * avoids ever materialising a full-resolution copy. Blocks are clamped at the
         * right and bottom edges when the region is odd-sized. */
        /* scale is a power of two, so a complete block divides by a shift. */
        const uint32_t full_block = (uint32_t)(scale * scale);
        const int block_shift = (scale == 2) ? 2 : 4;
        for (int y = 0; y < bh; y++) {
            uint32_t *drow = buf_a + (size_t)y * (size_t)bw;
            for (int x = 0; x < bw; x++) {
                uint32_t sr = 0, sg = 0, sb = 0, n = 0;
                for (int dy = 0; dy < scale; dy++) {
                    int syy = y * scale + dy;
                    if (syy >= sh) break;
                    const uint32_t *srow = rc->pixels + (size_t)(sy0 + syy) * (size_t)rc->stride;
                    for (int dx = 0; dx < scale; dx++) {
                        int sxx = x * scale + dx;
                        if (sxx >= sw) break;
                        uint32_t p = srow[sx0 + sxx];
                        sr += (p >> 16) & 0xFFu;
                        sg += (p >> 8) & 0xFFu;
                        sb += p & 0xFFu;
                        n++;
                    }
                }
                /* A full block is the overwhelmingly common case, and dividing by it
                 * costs three integer divisions per texel — the same cost that dominated
                 * the blur passes before they used a reciprocal. Partial blocks only
                 * occur on an odd-sized right or bottom edge. */
                if (n == full_block) {
                    drow[x] = ((sr >> block_shift) << 16) | ((sg >> block_shift) << 8) |
                              (sb >> block_shift);
                } else {
                    if (n == 0) n = 1;
                    drow[x] = ((sr / n) << 16) | ((sg / n) << 8) | (sb / n);
                }
            }
        }
    }

    if (blur_radius > 0.f) {
        for (int pass = 0; pass < MU_BLUR_PASSES; pass++) {
            box_blur_h(buf_a, buf_b, bw, bh, box_r);
            box_blur_v(buf_b, buf_a, bw, bh, box_r);
        }
    }

    /* Composite: mix toward the tint, mask with the rounded rect, write back. */
    float hw = area.w * 0.5f, hh = area.h * 0.5f;
    float limit = hw < hh ? hw : hh;
    float radius = corner_radius > limit ? limit : (corner_radius < 0.f ? 0.f : corner_radius);
    float cx = area.x + hw, cy = area.y + hh;
    const uint32_t mix = tint.a;
    /* Upsample maths: 2*scale is a power of two, so floor-divide is an arithmetic shift
     * and the masked remainder scales straight to an 8-bit fraction. */
    const int up_shift = (scale == 2) ? 2 : 3;
    const int up_mask = (1 << up_shift) - 1;
    const int up_frac = 256 >> up_shift;

    const uint32_t inv_mix = 255u - mix;
    const uint32_t tint_pr = mu_soft_mul255(tint.r, mix);
    const uint32_t tint_pg = mu_soft_mul255(tint.g, mix);
    const uint32_t tint_pb = mu_soft_mul255(tint.b, mix);

    /*
     * The tint as a lookup rather than as arithmetic.
     *
     * Each channel goes through mul255(channel, inv_mix) + tint_p*, and everything but the
     * channel itself is constant for the whole call — so there are only 256 possible
     * answers per channel. Tabulating them turns five operations per channel per pixel
     * into one byte load, and the stored values are exactly what the arithmetic produced,
     * so the output does not change.
     *
     * Building costs 256 iterations rather than 768: mul255(i, inv_mix) advances linearly
     * in i, so the multiply gives way to a running sum, and the three channels share that
     * work, differing only by the constant added at the end. That is on the order of a
     * microsecond — free against any panel worth blurring, and still negligible in
     * absolute terms against a small one.
     *
     * The sum cannot exceed 255: mul255 divides exactly by 255 and inv_mix + mix = 255, so
     * mul255(c, inv_mix) + mul255(tint.c, mix) <= inv_mix + mix.
     */
    unsigned char tlut_r[256], tlut_g[256], tlut_b[256];
    if (mix > 0u) {
        uint32_t t = 0x80u; /* mul255's rounding term; grows by inv_mix each step */
        for (int i = 0; i < 256; i++) {
            uint32_t v = (t + (t >> 8)) >> 8;
            tlut_r[i] = (unsigned char)(v + tint_pr);
            tlut_g[i] = (unsigned char)(v + tint_pg);
            tlut_b[i] = (unsigned char)(v + tint_pb);
            t += inv_mix;
        }
    }

    for (int y = wy0; y < wy1; y++) {
        uint32_t *drow = rc->pixels + (size_t)y * (size_t)rc->stride;
        float py = (float)y + 0.5f;

        /* Row setup for the upsample. A working texel at index i covers `scale`
         * full-resolution pixels starting at i*scale, so its centre sits half a block in;
         * the -0.5 puts the sample on texel centres rather than corners. */
        const uint32_t *brow = NULL;
        const uint32_t *up_row0 = NULL, *up_row1 = NULL;
        uint32_t hx_tab[MU_BLUR_MAX_SCALE][2]; /* {ix, wx} per horizontal phase */
        uint32_t wy = 0u, iy = 0u;
        if (scale == 1) {
            brow = buf_a + (size_t)(y - sy0) * (size_t)bw;
        } else {
            /* fy = (dy + 0.5)/scale - 0.5, rearranged as (2*dy + 1 - scale) / (2*scale).
             * scale is a power of two, so the divide is a shift and the remainder gives
             * the sub-texel fraction directly — no float conversion in the inner loop. */
            int numy = 2 * (y - sy0) + 1 - scale;
            int ty0 = numy >> up_shift;
            int ty1 = ty0 + 1;
            wy = (uint32_t)((numy & up_mask) * up_frac);
            iy = 256u - wy;
            if (ty0 < 0) ty0 = 0;
            if (ty0 > bh - 1) ty0 = bh - 1;
            if (ty1 < 0) ty1 = 0;
            if (ty1 > bh - 1) ty1 = bh - 1;

            up_row0 = buf_a + (size_t)ty0 * (size_t)bw;
            up_row1 = buf_a + (size_t)ty1 * (size_t)bw;

            /* The sample grid is uniform and scale is a power of two, so the horizontal
             * weights repeat with period `scale`; one pair per phase. */
            for (int p = 0; p < scale; p++) {
                int nx = 2 * p + 1 - scale;
                uint32_t wxp = (uint32_t)((nx & up_mask) * up_frac);
                hx_tab[p][0] = 256u - wxp;
                hx_tab[p][1] = wxp;
            }
        }

        /*
         * Bilinear is taken in two separable halves rather than as four weighted taps.
         *
         * The four-tap form p00*ix*iy + p10*wx*iy + p01*ix*wy + p11*wx*wy factors exactly
         * into (p00*iy + p01*wy)*ix + (p10*iy + p11*wy)*wx — the same products summed in a
         * different order, with the one rounding still at the end, so this is bit for bit
         * what the four-tap form produced.
         *
         * What it buys: each bracket depends only on a texel column and the row's vertical
         * weight, and one texel column feeds `scale` consecutive output pixels. So the
         * vertical half is computed once per column and reused across the block, leaving
         * two multiplies per channel per pixel instead of four, and one pair of texel loads
         * per `scale` pixels instead of four loads each.
         *
         * Keyed on the *unclamped* column index. At the left edge two different unclamped
         * indices can clamp to the same tx0 while producing different tx1, so keying on the
         * clamped value would reuse a column pair that does not match.
         */
        int col_key = INT_MIN;
        uint32_t v0r = 0, v0g = 0, v0b = 0, v1r = 0, v1g = 0, v1b = 0;

        /*
         * Span on this row that is fully covered, so the distance field can be skipped
         * across it. Only pixels near an edge or a corner arc actually need it, and the
         * SDF carries a sqrtf — the same reason mu_draw_rect confines its evaluation to
         * the corner boxes.
         *
         * Rows clear of the arcs (dy >= radius) are bounded left and right by vertical
         * edges, so all but a half-pixel fringe is solid. Rows crossing an arc are solid
         * only between the two arcs. Rows within half a pixel of the top or bottom edge
         * are antialiased across their whole width and get no solid span at all.
         */
        int solid_x0 = wx1, solid_x1 = wx1; /* empty span by default */
        if (radius <= 0.5f) {
            solid_x0 = wx0;
        } else {
            float dy_top = py - area.y;
            float dy_bot = (area.y + area.h) - py;
            float dy = dy_top < dy_bot ? dy_top : dy_bot;
            if (dy >= 0.5f) {
                float inset = (dy >= radius) ? 0.5f : radius;
                int s0 = (int)ceilf(area.x + inset);
                int s1 = (int)floorf(area.x + area.w - inset);
                if (s0 < wx0) s0 = wx0;
                if (s1 > wx1) s1 = wx1;
                if (s1 > s0) {
                    solid_x0 = s0;
                    solid_x1 = s1;
                }
            }
        }

        for (int x = wx0; x < wx1; x++) {
            float cov = 1.f;
            if (x < solid_x0 || x >= solid_x1) {
                float d = blur_sdf_round_box((float)x + 0.5f - cx, py - cy, hw, hh, radius);
                cov = 0.5f - d;
                if (cov <= 0.f) continue;
                if (cov > 1.f) cov = 1.f;
            }

            uint32_t r, g, b;
            if (scale == 1) {
                uint32_t s = brow[x - sx0];
                r = (s >> 16) & 0xFFu;
                g = (s >> 8) & 0xFFu;
                b = s & 0xFFu;
            } else {
                int numx = 2 * (x - sx0) + 1 - scale;
                int tx_raw = numx >> up_shift;
                if (tx_raw != col_key) {
                    col_key = tx_raw;
                    int t0 = tx_raw, t1 = tx_raw + 1;
                    if (t0 < 0) t0 = 0;
                    if (t0 > bw - 1) t0 = bw - 1;
                    if (t1 < 0) t1 = 0;
                    if (t1 > bw - 1) t1 = bw - 1;
                    uint32_t a0 = up_row0[t0], a1 = up_row1[t0];
                    uint32_t b0 = up_row0[t1], b1 = up_row1[t1];
                    v0r = ((a0 >> 16) & 0xFFu) * iy + ((a1 >> 16) & 0xFFu) * wy;
                    v0g = ((a0 >> 8) & 0xFFu) * iy + ((a1 >> 8) & 0xFFu) * wy;
                    v0b = (a0 & 0xFFu) * iy + (a1 & 0xFFu) * wy;
                    v1r = ((b0 >> 16) & 0xFFu) * iy + ((b1 >> 16) & 0xFFu) * wy;
                    v1g = ((b0 >> 8) & 0xFFu) * iy + ((b1 >> 8) & 0xFFu) * wy;
                    v1b = (b0 & 0xFFu) * iy + (b1 & 0xFFu) * wy;
                }

                /* Both halves are 8-bit weights, so the product stays under 2^24. */
                const uint32_t *h = hx_tab[(x - sx0) & (scale - 1)];
                r = (v0r * h[0] + v1r * h[1] + 32768u) >> 16;
                g = (v0g * h[0] + v1g * h[1] + 32768u) >> 16;
                b = (v0b * h[0] + v1b * h[1] + 32768u) >> 16;
            }

            if (mix > 0u) {
                r = tlut_r[r];
                g = tlut_g[g];
                b = tlut_b[b];
            }

            MuColor out = {(unsigned char)r, (unsigned char)g, (unsigned char)b, 255};
            uint32_t a = (uint32_t)(cov * 255.f + 0.5f);
            drow[x] = (a == 255u) ? mu_soft_pack(out) : mu_soft_blend_over(drow[x], out, a);
        }
    }
}

/*
 * Blur cache.
 *
 * Stores the surface contents immediately after a blur composited, so a later frame can
 * replay them instead of recomputing. Correctness rests entirely on the caller having
 * established that the pixels the blur *reads* are unchanged (mu_node_backdrop_unchanged);
 * everything checked here is only that the cached rectangle answers the same request.
 *
 * The whole rectangle is replayed, corners included. Pixels outside the rounded mask were
 * never written by the blur, so what the snapshot holds there is backdrop — and by the
 * caller's precondition that backdrop is identical, which makes copying it a no-op rather
 * than a corruption. Storing a coverage mask to skip them would cost more than it saves.
 *
 * Entries are matched on node pointer *and* node id. Ids are never reused within a
 * context, so a destroyed node whose address is later handed to a new one cannot be given
 * the dead node's pixels — which a bare pointer comparison would eventually do.
 */

static uint32_t tint_key(MuColor c) {
    return ((uint32_t)c.a << 24) | ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | (uint32_t)c.b;
}

static MuSoftBackdropSlot *backdrop_find(MuRenderContext *rc, const MuNode *node) {
    for (int i = 0; i < MU_BACKDROP_CACHE_MAX; i++) {
        MuSoftBackdropSlot *s = &rc->backdrop_cache[i];
        if (s->node == node && s->node_id == node->id && s->pixels) return s;
    }
    return NULL;
}

bool mu_backdrop_cache_try(MuRenderContext *rc, const MuNode *node, MuRect area, float blur_radius,
                           MuColor tint, float corner_radius) {
    if (!rc || !rc->pixels || !node) return false;
    MuSoftBackdropSlot *s = backdrop_find(rc, node);
    if (!s) return false;

    int x0, y0, x1, y1;
    if (!blur_write_rect(rc, area, &x0, &y0, &x1, &y1)) return false;
    /* Geometry, clip and blur parameters must all match. These are compared as exact
     * values because they are copied verbatim from the request that produced them; any
     * difference at all means the cached pixels answer a different question. */
    if (s->x0 != x0 || s->y0 != y0 || s->w != x1 - x0 || s->h != y1 - y0) return false;
    if (s->blur_radius != blur_radius || s->corner_radius != corner_radius) return false;
    if (s->tint != tint_key(tint)) return false;

    for (int y = 0; y < s->h; y++)
        memcpy(rc->pixels + (size_t)(y0 + y) * (size_t)rc->stride + (size_t)x0,
               s->pixels + (size_t)y * (size_t)s->w, (size_t)s->w * sizeof(uint32_t));

    s->stamp = ++rc->backdrop_stamp;
    rc->backdrop_hits++;
    return true;
}

void mu_backdrop_cache_store(MuRenderContext *rc, const MuNode *node, MuRect area, float blur_radius,
                             MuColor tint, float corner_radius) {
    if (!rc || !rc->pixels || !node) return;

    int x0, y0, x1, y1;
    if (!blur_write_rect(rc, area, &x0, &y0, &x1, &y1)) return;
    const int w = x1 - x0, h = y1 - y0;
    /* Store is only reached when try declined, so this counts exactly the recomputes. */
    rc->backdrop_misses++;

    MuSoftBackdropSlot *s = backdrop_find(rc, node);
    if (!s) {
        /* An empty slot if there is one, otherwise the least recently used. */
        s = &rc->backdrop_cache[0];
        for (int i = 0; i < MU_BACKDROP_CACHE_MAX; i++) {
            if (!rc->backdrop_cache[i].pixels) {
                s = &rc->backdrop_cache[i];
                break;
            }
            if (rc->backdrop_cache[i].stamp < s->stamp) s = &rc->backdrop_cache[i];
        }
    }

    if (s->pixels && s->w * s->h != w * h) {
        free(s->pixels);
        s->pixels = NULL;
    }
    if (!s->pixels) {
        s->pixels = (uint32_t *)malloc((size_t)w * (size_t)h * sizeof(uint32_t));
        if (!s->pixels) {
            /* Out of memory is not an error here — the blur has already been drawn
             * correctly, so dropping the entry only costs the next frame a recompute. */
            s->node = NULL;
            return;
        }
    }

    for (int y = 0; y < h; y++)
        memcpy(s->pixels + (size_t)y * (size_t)w,
               rc->pixels + (size_t)(y0 + y) * (size_t)rc->stride + (size_t)x0,
               (size_t)w * sizeof(uint32_t));

    s->node = node;
    s->node_id = node->id;
    s->w = w;
    s->h = h;
    s->x0 = x0;
    s->y0 = y0;
    s->blur_radius = blur_radius;
    s->corner_radius = corner_radius;
    s->tint = tint_key(tint);
    s->stamp = ++rc->backdrop_stamp;
}

void mu_backdrop_cache_drop(MuRenderContext *rc, const MuNode *node) {
    if (!rc || !node) return;
    MuSoftBackdropSlot *s = backdrop_find(rc, node);
    if (!s) return;
    free(s->pixels);
    s->pixels = NULL;
    s->node = NULL;
}

void mu_soft_backdrop_cache_free_all(MuRenderContext *rc) {
    if (!rc) return;
    for (int i = 0; i < MU_BACKDROP_CACHE_MAX; i++) {
        free(rc->backdrop_cache[i].pixels);
        rc->backdrop_cache[i].pixels = NULL;
        rc->backdrop_cache[i].node = NULL;
    }
}
