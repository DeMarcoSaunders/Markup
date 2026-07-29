#include "../include/markup/mu_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(MU_HAVE_LIBPNG)
#include <png.h>

typedef struct MuPngReadCtx {
    const uint8_t *data;
    size_t size;
    size_t offset;
} MuPngReadCtx;

static void png_read_fn(png_structp png, png_bytep out, png_size_t length) {
    MuPngReadCtx *ctx = (MuPngReadCtx *)png_get_io_ptr(png);
    if (!ctx || !out || length == 0) return;
    if (ctx->offset + length > ctx->size) {
        png_error(png, "PNG read overrun");
        return;
    }
    memcpy(out, ctx->data + ctx->offset, length);
    ctx->offset += length;
}

static bool png_decode_rgba(png_structp png, png_infop info, MuImageRgba *out) {
    if (!png || !info || !out) return false;

    png_read_info(png, info);

    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    if (width <= 0 || height <= 0) return false;

    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    const png_size_t rowbytes = png_get_rowbytes(png, info);
    const size_t total = rowbytes * (size_t)height;
    uint8_t *buffer = (uint8_t *)malloc(total);
    if (!buffer) return false;

    png_bytep *rows = (png_bytep *)malloc(sizeof(png_bytep) * (size_t)height);
    if (!rows) {
        free(buffer);
        return false;
    }
    for (int y = 0; y < height; y++) rows[y] = buffer + y * rowbytes;

    png_read_image(png, rows);
    free(rows);

    out->pixels = buffer;
    out->width = width;
    out->height = height;
    out->stride = (int)rowbytes;
    return true;
}

static bool png_decode_from_memory(const void *data, size_t size, MuImageRgba *out) {
    if (!data || size < 8 || !out) return false;
    memset(out, 0, sizeof(*out));

    if (png_sig_cmp((const png_bytep)data, 0, 8) != 0) return false;

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) return false;

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        mu_image_rgba_free(out);
        return false;
    }

    MuPngReadCtx ctx = {.data = (const uint8_t *)data, .size = size, .offset = 0};
    png_set_read_fn(png, &ctx, png_read_fn);

    const bool ok = png_decode_rgba(png, info, out);
    png_destroy_read_struct(&png, &info, NULL);
    return ok;
}
#endif /* MU_HAVE_LIBPNG */

void mu_image_rgba_free(MuImageRgba *img) {
    if (!img) return;
    free(img->pixels);
    img->pixels = NULL;
    img->width = 0;
    img->height = 0;
    img->stride = 0;
}

bool mu_image_decode_rgba_memory(const void *data, size_t size, MuImageRgba *out) {
#if defined(MU_HAVE_LIBPNG)
    return png_decode_from_memory(data, size, out);
#else
    (void)data;
    (void)size;
    (void)out;
    return false;
#endif
}

bool mu_image_decode_rgba_file(const char *path, MuImageRgba *out) {
    if (!path || !out) return false;
    memset(out, 0, sizeof(*out));

#if !defined(MU_HAVE_LIBPNG)
    (void)path;
    return false;
#else
    FILE *fp = fopen(path, "rb");
    if (!fp) return false;

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    if (file_size <= 0) {
        fclose(fp);
        return false;
    }
    fseek(fp, 0, SEEK_SET);

    uint8_t *file_data = (uint8_t *)malloc((size_t)file_size);
    if (!file_data) {
        fclose(fp);
        return false;
    }
    if (fread(file_data, 1, (size_t)file_size, fp) != (size_t)file_size) {
        free(file_data);
        fclose(fp);
        return false;
    }
    fclose(fp);

    const bool ok = mu_image_decode_rgba_memory(file_data, (size_t)file_size, out);
    free(file_data);
    return ok;
#endif
}
