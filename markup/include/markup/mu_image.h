#ifndef MU_IMAGE_H
#define MU_IMAGE_H

#include "mu_style.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MU_IMAGE_INVALID 0u
#define MU_IMAGE_MAX 64u

typedef enum MuImageFit {
    MU_IMAGE_FIT_FILL = 0,
    MU_IMAGE_FIT_CONTAIN,
    MU_IMAGE_FIT_COVER,
} MuImageFit;

typedef struct MuDrawImageOpts {
    MuImageFit fit;
    MuColor tint;
    float radius;
    /** Texture source rect; w/h <= 0 means use the full image. */
    MuRect src;
} MuDrawImageOpts;

/** Uniform grid atlas (sprite sheet) parsed from cell size + column count. */
typedef struct MuImageSheet {
    uint32_t image_id;
    float cell_w;
    float cell_h;
    int cols;
} MuImageSheet;

void mu_draw_image_opts_init(MuDrawImageOpts *opts);

/** Resolve src to pixel rect; zero w/h in src uses full texture dimensions. */
bool mu_image_resolve_src(const MuRect *src, float tex_w, float tex_h, MuRect *out);

void mu_image_sheet_init(MuImageSheet *sheet);
bool mu_image_sheet_load(MuImageSheet *sheet, MuRenderContext *rc, const char *path, float cell_w, float cell_h,
                         int cols);
/** Row-major cell index (0 = top-left). Returns zero rect if invalid. */
MuRect mu_image_sheet_cell(const MuImageSheet *sheet, int index);

/** Load PNG/JPEG/etc. Returns MU_IMAGE_INVALID on failure. */
uint32_t mu_image_load_file(MuRenderContext *rc, const char *path);

bool mu_image_get_size(MuRenderContext *rc, uint32_t image_id, float *out_w, float *out_h);

void mu_draw_image(MuRenderContext *rc, uint32_t image_id, MuRect dst, const MuDrawImageOpts *opts);

/** Compute destination rect for fit mode (shared by backends). */
MuRect mu_image_fit_dst(float src_w, float src_h, MuRect bounds, MuImageFit fit);

/** Decoded RGBA8 pixels (backend-agnostic; freed with mu_image_rgba_free). */
typedef struct MuImageRgba {
    uint8_t *pixels;
    int width;
    int height;
    int stride;
} MuImageRgba;

/** Decode PNG/JPEG/BMP from disk into RGBA8. */
bool mu_image_decode_rgba_file(const char *path, MuImageRgba *out);

/** Decode PNG/JPEG/BMP bytes into RGBA8. The path targets with no stdio should use. */
bool mu_image_decode_rgba_memory(const void *data, size_t size, MuImageRgba *out);

/**
 * Why the last decode failed, or NULL if the last one succeeded.
 *
 * Distinguishes a missing file from a corrupt one from an unsupported format, which
 * MU_IMAGE_INVALID on its own cannot. Points at static storage; not thread-safe.
 */
const char *mu_image_decode_last_error(void);

void mu_image_rgba_free(MuImageRgba *img);

#ifdef __cplusplus
}
#endif

#endif
