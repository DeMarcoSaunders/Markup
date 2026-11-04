/*
 * Link test for the installed, deprecated raylib backend.
 *
 * The calls are real calls, inside a branch that never runs -- actually executing them
 * would need a GL window. Taking function addresses instead does NOT work: at /O2 the
 * compiler folds `&f != 0` to 1 and emits no reference to the symbol at all, so such a
 * test links happily with the library missing from the link line. That false pass is how
 * these bugs survived.
 *
 * What this pins down is markup::raylib's install-tree link interface, which
 * MarkupImportedRaylib.cmake writes by hand because raylib's PRIVATE linkage cannot go
 * through install(EXPORT). markup::paint and the LINK_ONLY markup::widgets_basic were both
 * missing from it, so a consumer linking markup::raylib alone got undefined references to
 * mu_paint_all, mu_make_panel, mu_text_style_init and the mu_image_* helpers.
 *
 * mu_backend.h is included with no -D: MU_BACKEND_RAYLIB must come from the imported
 * target's INTERFACE_COMPILE_DEFINITIONS.
 */
#include "markup/mu_backend.h"

#include "markup/mu_core.h"
#include "markup/mu_input.h"
#include "markup/mu_render.h"
#include "markup/mu_widgets_basic.h"

#include <stdio.h>

#if !defined(MU_BACKEND_RAYLIB)
#error "MU_BACKEND_RAYLIB was not defined by linking markup::raylib"
#endif

int main(int argc, char **argv) {
    (void)argv;

    /* Never true. The linker still has to resolve every symbol below. */
    if (argc > 99) {
        MuContext ctx;
        MuRenderContext rc;

        mu_context_init(&ctx, 1 << 16);              /* markup::core */
        mu_input_update_hover(&ctx, (MuVec2){0, 0}); /* markup::input */
        mu_damage_collect(&ctx);                     /* markup::paint */
        mu_paint_all(&ctx, &rc);                     /* markup::paint */
        (void)mu_make_panel(&ctx, true);             /* markup::widgets_basic (LINK_ONLY) */
        mu_raylib_frame(&ctx, &rc);                  /* markup::raylib */
    }

    printf("consumer_raylib: linked ok\n");
    return 0;
}
