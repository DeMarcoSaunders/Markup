/*
 * Smoke test for the installed Markup package, software backend.
 *
 * The ordinary tests link the in-build targets, where a wrong EXPORT_NAME, a library
 * missing from install(), or a backend define that never made it into the export are all
 * invisible. This is built as a standalone project against an installed prefix, so it
 * exercises the thing consumers actually get. Every bug that motivated it was of that
 * shape: the install defined markup::markup_core rather than markup::core, markup_paint
 * was never installed at all, and markup::soft had no imported target.
 *
 * It calls at least one symbol from every exported library, because linking a library
 * without referencing it proves nothing. markup/mu_backend.h is included with no -D of
 * its own: MU_BACKEND_SOFT has to arrive through markup::soft's exported compile
 * definitions, which is what a hand-written IMPORTED target used to get wrong.
 */
#include "markup/mu_backend.h"

#include "markup/mu_anim.h"
#include "markup/mu_compose.h"
#include "markup/mu_core.h"
#include "markup/mu_input.h"
#include "markup/mu_layout_flex.h"
#include "markup/mu_physics_node.h"
#include "markup/mu_render.h"
#include "markup/mu_style.h"
#include "markup/mu_widgets_basic.h"

#include <stdio.h>

#if !defined(MU_BACKEND_SOFT)
#error "MU_BACKEND_SOFT was not defined by linking markup::soft"
#endif

#define W 160
#define H 120

int main(void) {
    MuContext ctx;
    MuRenderContext rc;

    /* core + style + soft */
    mu_context_init(&ctx, 1 << 16);
    mu_style_init(&ctx, NULL);
    mu_soft_render_init(&rc, W, H);
    mu_render_bind_measure(&rc);

    /* widgets_basic */
    mu_widgets_basic_register(&ctx);
    MuNode *root = mu_make_panel(&ctx, true);
    root->role = "page";
    mu_node_set_bounds(root, (MuRect){0, 0, W, H});
    mu_context_set_root(&ctx, root);
    MuNode *button = mu_make_button(&ctx, "Hi", NULL, NULL);
    mu_node_add_child(&ctx, root, button);

    /* compose */
    mu_compose_decorative(button);

    /* physics + physics_node */
    mu_physics_register(&ctx);
    MuPhysConfig cfg;
    mu_phys_config_init(&cfg);
    MuNode *world = mu_make_physics_world(&ctx, &cfg);
    mu_node_add_child(&ctx, root, world);

    /* anim */
    MuAnimator *anim = mu_anim_create();
    mu_anim_set(anim, button, button->bounds);

    /* layout_flex + input + paint, driven through the damage region API */
    mu_frame_begin(&ctx);
    mu_layout_run(&ctx);
    mu_input_update_hover(&ctx, (MuVec2){4.f, 4.f});
    mu_damage_collect(&ctx);

    int rects = mu_region_count(mu_damage_region(&ctx));
    mu_soft_begin_frame_region(&rc, (MuColor){20, 20, 24, 255}, mu_damage_region(&ctx));
    mu_paint_damaged(&ctx, &rc);
    mu_damage_reset(&ctx);
    mu_frame_end(&ctx);

    MuColor px = mu_soft_get_pixel(&rc, W / 2, H / 2);

    mu_anim_destroy(anim);
    mu_soft_render_shutdown(&rc);
    mu_style_shutdown(&ctx);
    mu_context_shutdown(&ctx);

    /* A first frame damages everything, and the centre of a painted page is opaque. Both
     * would also hold for a stub, but a stub could not have linked. */
    if (rects < 1) {
        printf("consumer_soft: FAIL - first frame damaged nothing\n");
        return 1;
    }
    if (px.a == 0) {
        printf("consumer_soft: FAIL - centre pixel never painted\n");
        return 1;
    }
    printf("consumer_soft: ok - %d damage rect(s), centre pixel {%u,%u,%u,%u}\n", rects, px.r,
           px.g, px.b, px.a);
    return 0;
}
