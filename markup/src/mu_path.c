#include "../include/markup/mu_path.h"
#include "../include/markup/mu_render.h"
#include <string.h>

void mu_path_init(MuPath *path) {
    if (!path) return;
    memset(path, 0, sizeof(*path));
}

static bool path_push(MuPath *path, MuPathCmd cmd, float x, float y) {
    if (!path || path->count >= MU_PATH_MAX) return false;
    path->cmds[path->count] = cmd;
    path->xs[path->count] = x;
    path->ys[path->count] = y;
    path->count++;
    return true;
}

bool mu_path_move_to(MuPath *path, float x, float y) {
    return path_push(path, MU_PATH_MOVE, x, y);
}

bool mu_path_line_to(MuPath *path, float x, float y) {
    return path_push(path, MU_PATH_LINE, x, y);
}

bool mu_path_close(MuPath *path) {
    if (!path || path->count <= 0) return false;
    return path_push(path, MU_PATH_CLOSE, 0.f, 0.f);
}

void mu_path_rect(MuPath *path, MuRect r) {
    if (!path) return;
    mu_path_move_to(path, r.x, r.y);
    mu_path_line_to(path, r.x + r.w, r.y);
    mu_path_line_to(path, r.x + r.w, r.y + r.h);
    mu_path_line_to(path, r.x, r.y + r.h);
    mu_path_close(path);
}

void mu_draw_path_opts_init(MuDrawPathOpts *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->stroke_width = 1.f;
}
