#include "../include/markup/mu_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Image decoding.
 *
 * Uses stb_image (vendored, public domain) rather than libpng. libpng was an external
 * dependency that had to be found at configure time, and when it was not — which was the
 * default on this tree — every mu_image_load_file call returned MU_IMAGE_INVALID without
 * so much as opening the file. It also cannot exist on a bare-metal target, which is the
 * whole point of the software backend.
 *
 * Decoding runs from memory, never from a path, so a target with no stdio can drop
 * mu_image_decode_rgba_file and feed embedded bytes to mu_image_decode_rgba_memory.
 *
 * Only PNG, JPEG and BMP are compiled in; the rest of stb_image's formats are excluded to
 * keep the binary small. Failure strings are kept — see mu_image_decode_last_error.
 */

#define STB_IMAGE_IMPLEMENTATION
/* Internal linkage: raylib embeds its own copy of stb_image, and without this every
 * stbi_* symbol collides at link time in any target using both. Note the spelling —
 * stb_image wants STB_IMAGE_STATIC while stb_truetype wants STBTT_STATIC; they differ. */
#define STB_IMAGE_STATIC
#define STBI_NO_STDIO /* file reading is done here, so stb needs none */
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#include "../vendor/stb_image.h"

/* Set on every failure path so callers can report why, rather than only that. */
static const char *g_last_error;

const char *mu_image_decode_last_error(void) {
    return g_last_error;
}

void mu_image_rgba_free(MuImageRgba *img) {
    if (!img) return;
    free(img->pixels);
    img->pixels = NULL;
    img->width = 0;
    img->height = 0;
    img->stride = 0;
}

bool mu_image_decode_rgba_memory(const void *data, size_t size, MuImageRgba *out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!data || size == 0) {
        g_last_error = "no data";
        return false;
    }

    int w = 0, h = 0, channels = 0;
    /* req_comp 4 forces RGBA8 regardless of the source format. */
    stbi_uc *pixels = stbi_load_from_memory((const stbi_uc *)data, (int)size, &w, &h, &channels, 4);
    if (!pixels) {
        g_last_error = stbi_failure_reason();
        if (!g_last_error) g_last_error = "decode failed";
        return false;
    }
    if (w < 1 || h < 1) {
        stbi_image_free(pixels);
        g_last_error = "zero-sized image";
        return false;
    }

    out->pixels = pixels;
    out->width = w;
    out->height = h;
    out->stride = w * 4; /* stb returns tightly packed rows */
    g_last_error = NULL;
    return true;
}

bool mu_image_decode_rgba_file(const char *path, MuImageRgba *out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!path || !path[0]) {
        g_last_error = "empty path";
        return false;
    }

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        g_last_error = "cannot open file";
        return false;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        g_last_error = "seek failed";
        return false;
    }
    long file_size = ftell(fp);
    if (file_size <= 0) {
        fclose(fp);
        g_last_error = "empty file";
        return false;
    }
    rewind(fp);

    uint8_t *file_data = (uint8_t *)malloc((size_t)file_size);
    if (!file_data) {
        fclose(fp);
        g_last_error = "out of memory";
        return false;
    }
    size_t got = fread(file_data, 1, (size_t)file_size, fp);
    fclose(fp);
    if (got != (size_t)file_size) {
        free(file_data);
        g_last_error = "short read";
        return false;
    }

    const bool ok = mu_image_decode_rgba_memory(file_data, got, out);
    free(file_data);
    return ok;
}
