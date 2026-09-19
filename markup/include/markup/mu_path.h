#ifndef MU_PATH_H
#define MU_PATH_H

#include "mu_style.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MuRenderContext MuRenderContext;

#define MU_PATH_MAX 128

typedef enum MuPathCmd {
    MU_PATH_MOVE = 0,
    MU_PATH_LINE,
    MU_PATH_CLOSE,
} MuPathCmd;

/** Lightweight path: move/line/close commands in logical coordinates. */
typedef struct MuPath {
    float xs[MU_PATH_MAX];
    float ys[MU_PATH_MAX];
    MuPathCmd cmds[MU_PATH_MAX];
    int count;
} MuPath;

typedef struct MuDrawPathOpts {
    MuColor fill;
    MuColor stroke;
    float stroke_width;
} MuDrawPathOpts;

void mu_path_init(MuPath *path);
bool mu_path_move_to(MuPath *path, float x, float y);
bool mu_path_line_to(MuPath *path, float x, float y);
bool mu_path_close(MuPath *path);

/** Convenience: axis-aligned rectangle as a closed path. */
void mu_path_rect(MuPath *path, MuRect r);

void mu_draw_path_opts_init(MuDrawPathOpts *opts);
void mu_draw_path(MuRenderContext *rc, const MuPath *path, const MuDrawPathOpts *opts);

#ifdef __cplusplus
}
#endif

#endif
