#include "../include/markup/mu_path.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct MuPathEdge {
    float x0, y0, x1, y1;
} MuPathEdge;

typedef struct MuPathEdgeList {
    MuPathEdge edges[MU_PATH_MAX * 2];
    int count;
} MuPathEdgeList;

typedef void (*MuPathSpanFn)(void *ctx, int y, int x0, int x1, MuColor col, uint8_t coverage);

static void path_edges_from(const MuPath *path, MuPathEdgeList *out) {
    if (!path || !out) return;
    out->count = 0;

    float sub_x = 0.f, sub_y = 0.f;
    float cur_x = 0.f, cur_y = 0.f;
    bool have_sub = false;
    bool have_pt = false;

    for (int i = 0; i < path->count; i++) {
        MuPathCmd cmd = path->cmds[i];
        if (cmd == MU_PATH_MOVE) {
            cur_x = path->xs[i];
            cur_y = path->ys[i];
            sub_x = cur_x;
            sub_y = cur_y;
            have_sub = true;
            have_pt = true;
            continue;
        }
        if (cmd == MU_PATH_LINE) {
            float nx = path->xs[i];
            float ny = path->ys[i];
            if (have_pt && out->count < (int)(sizeof(out->edges) / sizeof(out->edges[0]))) {
                out->edges[out->count++] = (MuPathEdge){cur_x, cur_y, nx, ny};
            }
            cur_x = nx;
            cur_y = ny;
            have_pt = true;
            continue;
        }
        if (cmd == MU_PATH_CLOSE && have_sub && have_pt) {
            if (out->count < (int)(sizeof(out->edges) / sizeof(out->edges[0])))
                out->edges[out->count++] = (MuPathEdge){cur_x, cur_y, sub_x, sub_y};
            cur_x = sub_x;
            cur_y = sub_y;
        }
    }
}

static int edge_cmp_x(const void *a, const void *b) {
    const float *fa = (const float *)a;
    const float *fb = (const float *)b;
    if (*fa < *fb) return -1;
    if (*fa > *fb) return 1;
    return 0;
}

void mu_path_raster_fill_even_odd(const MuPath *path, MuPathSpanFn span, void *ctx, MuColor fill) {
    if (!path || !span || fill.a == 0) return;

    MuPathEdgeList edges;
    path_edges_from(path, &edges);
    if (edges.count <= 0) return;

    float min_y = edges.edges[0].y0, max_y = edges.edges[0].y0;
    for (int i = 0; i < edges.count; i++) {
        MuPathEdge e = edges.edges[i];
        if (e.y0 < min_y) min_y = e.y0;
        if (e.y1 < min_y) min_y = e.y1;
        if (e.y0 > max_y) max_y = e.y0;
        if (e.y1 > max_y) max_y = e.y1;
    }

    int y0 = (int)floorf(min_y);
    int y1 = (int)ceilf(max_y);
    if (y1 <= y0) return;

    float xs[MU_PATH_MAX * 2];
    for (int y = y0; y < y1; y++) {
        float scan = (float)y + 0.5f;
        int n = 0;
        for (int i = 0; i < edges.count && n < (int)(sizeof(xs) / sizeof(xs[0])); i++) {
            MuPathEdge e = edges.edges[i];
            float ya = e.y0, yb = e.y1;
            if ((ya <= scan && yb > scan) || (yb <= scan && ya > scan)) {
                float t = (scan - ya) / (yb - ya);
                xs[n++] = e.x0 + t * (e.x1 - e.x0);
            }
        }
        if (n < 2) continue;
        qsort(xs, (size_t)n, sizeof(float), edge_cmp_x);
        for (int i = 0; i + 1 < n; i += 2) {
            int x0 = (int)floorf(xs[i]);
            int x1 = (int)ceilf(xs[i + 1]);
            if (x1 > x0) span(ctx, y, x0, x1, fill, 255);
        }
    }
}

static void stroke_quad(MuPathSpanFn span, void *ctx, float ax, float ay, float bx, float by, float cx, float cy,
                        float dx, float dy, MuColor col) {
    float pts_x[4] = {ax, bx, cx, dx};
    float pts_y[4] = {ay, by, cy, dy};
    MuPath p;
    mu_path_init(&p);
    mu_path_move_to(&p, pts_x[0], pts_y[0]);
    for (int i = 1; i < 4; i++) mu_path_line_to(&p, pts_x[i], pts_y[i]);
    mu_path_close(&p);
    mu_path_raster_fill_even_odd(&p, span, ctx, col);
}

void mu_path_raster_stroke(const MuPath *path, MuPathSpanFn span, void *ctx, MuColor stroke, float width) {
    if (!path || !span || stroke.a == 0 || width <= 0.f) return;

    MuPathEdgeList edges;
    path_edges_from(path, &edges);
    if (edges.count <= 0) return;

    float hw = width * 0.5f;
    for (int i = 0; i < edges.count; i++) {
        MuPathEdge e = edges.edges[i];
        float dx = e.x1 - e.x0;
        float dy = e.y1 - e.y0;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1e-4f) continue;
        float nx = -dy / len * hw;
        float ny = dx / len * hw;
        stroke_quad(span, ctx, e.x0 + nx, e.y0 + ny, e.x0 - nx, e.y0 - ny, e.x1 - nx, e.y1 - ny, e.x1 + nx,
                    e.y1 + ny, stroke);
    }
}
