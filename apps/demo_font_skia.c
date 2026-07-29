#include "demo_font_skia.h"
#include "markup_demo_font_config.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static int file_exists(const char *path) {
    if (!path || !path[0]) return 0;
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    return access(path, F_OK) == 0;
#endif
}

void demo_font_skia_try_load(MuRenderContext *rc, const MuStyleModule *style) {
    if (!rc) return;
    float fz = style ? style->default_font_size : 16.f;
    const char *candidates[] = {
        "assets/InterVariable.ttf",
        MARKUP_DEMO_FONT_FILE_ABS,
        NULL,
    };

    for (int i = 0; candidates[i]; i++) {
        if (!file_exists(candidates[i])) continue;
        if (mu_skia_set_font_file(rc, candidates[i], fz)) {
            (void)fprintf(stderr, "Markup demo (Skia): UI font `%s` @ %.0f px\n", candidates[i], fz);
            return;
        }
    }

    mu_skia_set_font_file(rc, NULL, fz);
    (void)fprintf(stderr, "Markup demo (Skia): bundled Inter not found; using default typeface.\n");
}
