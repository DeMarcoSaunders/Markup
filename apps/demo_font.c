#include "demo_font.h"
#include "markup/mu_raylib.h"
#include "markup/mu_style.h"
#include "markup_demo_font_config.h"
#include <math.h>
#include <raylib.h>

void demo_font_try_load_ui(MuRenderContext *rc, const MuStyleModule *style) {
    if (!rc)
        return;
    Font def = GetFontDefault();
    float fz = style ? style->default_font_size : 16.f;
    int px = (int)(floorf(fz + 0.5f));
    if (px < 10)
        px = 10;

    const char *candidates[] = {
        "assets/InterVariable.ttf",
        MARKUP_DEMO_FONT_FILE_ABS,
        NULL,
    };

    for (int i = 0; candidates[i]; i++) {
        if (!FileExists(candidates[i]))
            continue;

        Font f = LoadFontEx(candidates[i], px, NULL, 0);
        if (!IsFontReady(f))
            continue;
        if (f.texture.id == def.texture.id)
            continue;

        mu_render_set_font(rc, f, true);
        TraceLog(LOG_INFO, "Markup demo: UI font `%s` @ %i px", candidates[i], px);
        return;
    }

    TraceLog(LOG_WARNING,
             "Markup demo: bundled Inter Variable not loaded (cwd / path). Pixel font fallback.");
}
