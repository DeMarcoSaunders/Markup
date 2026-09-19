#ifndef MU_PATH_RASTER_H
#define MU_PATH_RASTER_H

#include "../include/markup/mu_path.h"
#include <stdint.h>

typedef void (*MuPathSpanFn)(void *ctx, int y, int x0, int x1, MuColor col, uint8_t coverage);

void mu_path_raster_fill_even_odd(const MuPath *path, MuPathSpanFn span, void *ctx, MuColor fill);
void mu_path_raster_stroke(const MuPath *path, MuPathSpanFn span, void *ctx, MuColor stroke, float width);

#endif
