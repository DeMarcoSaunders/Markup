#include "markup/mu_soft.h"
#include "markup/mu_widgets_basic.h"
#include "markup/mu_layout_flex.h"
#include "markup/mu_input.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 320
#define H 160
static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)
static const MuColor WHITE = {255,255,255,255}, BLACK = {0,0,0,255}, CLEAR = {0};

static int same(const MuRenderContext *a, const MuRenderContext *b) {
    for (int y = 0; y < H; y++)
        if (memcmp(a->pixels + (size_t)y * a->stride, b->pixels + (size_t)y * b->stride,
                   W * sizeof(uint32_t))) return 0;
    return 1;
}

static void test_styles(void) {
    MuContext ctx;
    MuRenderContext a, b;
    mu_context_init(&ctx, 65536);
    mu_style_init(&ctx, NULL);
    mu_widgets_basic_register(&ctx);
    MuNode *input = mu_make_textinput(&ctx, "");
    mu_context_set_root(&ctx, input);
    input->bounds = (MuRect){30,30,180,40};
    mu_node_set_fill(input, WHITE);
    mu_node_set_ink(input, (MuColor){140,140,140,255});
    mu_node_set_border_color(input, (MuColor){212,212,212,255});
    mu_node_set_border_width(input, 1.f);
    mu_node_set_radius(input, 14.f);
    mu_node_set_placeholder_ink(input, (MuColor){212,212,212,255});
    mu_soft_render_init(&a, W, H);
    mu_soft_render_init(&b, W, H);
    /* No font, and the caret is in its off phase: isolate the field's box. */
    ctx.frame_index = 30;
    for (int focused = 0; focused < 2; focused++) {
        input->flags = (input->flags & ~MU_NODE_FOCUSED) | (focused ? MU_NODE_FOCUSED : 0);
        MuStyleSnapshot st;
        mu_style_resolve(&ctx, input, &st);
        CHECK(st.background.r == 255 && st.foreground.r == 140);
        CHECK(st.border_width == 1.f && st.border.r == 212 && st.radius_tl == 14.f);
        CHECK(st.placeholder.r == 212);
        mu_soft_begin_frame(&a, BLACK);
        mu_soft_begin_frame(&b, BLACK);
        mu_paint_all(&ctx, &a);
        mu_draw_rect(&b, input->bounds, WHITE, st.border, 1.f, 14.f);
        CHECK(same(&a, &b));
    }
    MuNode sibling = {0}; sibling.role = "input"; mu_text_style_init(&sibling.text);
    MuStyleSnapshot st;
    mu_style_resolve(&ctx, &sibling, &st);
    CHECK(st.radius_tl == ctx.style->default_radius && st.border_width == 1.f);
    mu_node_set_fill(input, CLEAR);
    mu_node_set_border_width(input, 0.f);
    mu_node_set_radius(input, 0.f);
    mu_style_resolve(&ctx, input, &st);
    CHECK(st.background.a == 0 && st.border_width == 0 && st.radius_tl == 0);
    mu_node_clear_style(input, MU_STYLE_FILL | MU_STYLE_BORDER_WIDTH | MU_STYLE_RADIUS);
    mu_style_resolve(&ctx, input, &st);
    CHECK(st.background.a == 255 && st.border_width == 2.f && st.radius_tl == 6.f);
    mu_style_shutdown(&ctx);
    mu_style_resolve(&ctx, input, &st);
    CHECK(st.foreground.r == 140 && st.border.r == 212); /* overrides without a theme */
    mu_context_shutdown(&ctx);
    mu_soft_render_shutdown(&a); mu_soft_render_shutdown(&b);
}

static void test_shadow(void) {
    MuRenderContext a, b;
    mu_soft_render_init(&a, W, H); mu_soft_render_init(&b, W, H);
    MuRect box = {80,40,150,60};
    MuVec2 offset = {0,8};
    MuColor faint = {0,0,0,15};
    mu_soft_begin_frame(&a, WHITE);
    mu_draw_shadow(&a, box, 14, faint, 24, offset);
    CHECK(mu_soft_get_pixel(&a, 150, 75).r == 240);
    CHECK(mu_soft_get_pixel(&a, 150, 112).r < 255);
    CHECK(mu_soft_get_pixel(&a, 150, 112).r < mu_soft_get_pixel(&a, 150, 35).r);
    CHECK(mu_soft_get_pixel(&a, 10, 10).r == 255);
    /* A damage clip changes only the composite, never the mask being blurred. */
    mu_soft_begin_frame(&b, WHITE);
    mu_push_scissor(&b, (MuRect){140,95,16,30});
    mu_draw_shadow(&b, box, 14, faint, 24, offset);
    mu_pop_scissor(&b);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        if (x >= 140 && x < 156 && y >= 95 && y < 125)
            CHECK(mu_soft_get_pixel(&a,x,y).r == mu_soft_get_pixel(&b,x,y).r);
        else CHECK(mu_soft_get_pixel(&b,x,y).r == 255);
    }
    /* Logical scaling must equal the same request in physical coordinates. */
    mu_soft_set_scale(&a, 2.f);
    mu_soft_begin_frame(&a, WHITE); mu_soft_begin_frame(&b, WHITE);
    mu_draw_shadow(&a, (MuRect){20,15,70,30}, 14, faint, 12, (MuVec2){0,4});
    mu_draw_shadow(&b, (MuRect){40,30,140,60}, 28, faint, 24, (MuVec2){0,8});
    CHECK(same(&a, &b));
    mu_soft_set_scale(&a, 1.f);
    mu_soft_begin_frame(&a, CLEAR);
    mu_draw_shadow(&a, box, 14, (MuColor){80,40,20,128}, 0, offset);
    CHECK(mu_soft_get_pixel(&a, 150,75).a == 128);
    CHECK(mu_soft_get_pixel(&a, 80,48).a < 128); /* rounded corner */
    mu_soft_render_shutdown(&a); mu_soft_render_shutdown(&b);
}

static void compare_frame(MuContext *ctx, MuRenderContext *a, MuRenderContext *b) {
    mu_damage_collect(ctx);
    mu_soft_begin_frame_region(a, WHITE, mu_damage_region(ctx));
    mu_paint_damaged(ctx, a);
    mu_soft_begin_frame(b, WHITE);
    mu_paint_all(ctx, b);
    CHECK(same(a, b));
    mu_damage_reset(ctx);
}

static void test_shadow_damage(void) {
    MuContext ctx; MuRenderContext a,b;
    mu_context_init(&ctx, 65536); mu_style_init(&ctx, NULL); mu_widgets_basic_register(&ctx);
    mu_soft_render_init(&a,W,H); mu_soft_render_init(&b,W,H);
    mu_soft_begin_frame(&a, WHITE);
    MuNode *root = mu_make_panel(&ctx,true), *field = mu_make_textinput(&ctx, "");
    MuNode *group = mu_make_panel(&ctx,true);
    group->role = "group";
    group->flags &= ~MU_NODE_CLIP_CHILDREN;
    group->bounds = (MuRect){60,40,20,20};
    root->bounds = (MuRect){0,0,W,H};
    field->bounds = (MuRect){60,40,150,40};
    mu_context_set_root(&ctx,root); mu_node_add_child(&ctx,root,group);
    mu_node_add_child(&ctx,group,field);
    mu_node_set_fill(root, WHITE); mu_node_set_border_width(root,0);
    mu_node_set_fill(field, WHITE); mu_node_set_radius(field,14);
    mu_node_set_shadow(field,(MuColor){0,0,0,15},24,(MuVec2){0,8});
    compare_frame(&ctx,&a,&b);
    field->paint_offset = (MuVec2){30,8}; compare_frame(&ctx,&a,&b);
    field->bounds.w = 70; compare_frame(&ctx,&a,&b);
    mu_node_set_shadow(field,(MuColor){0,0,0,30},8,(MuVec2){-12,-4}); compare_frame(&ctx,&a,&b);
    mu_node_set_fill(field,(MuColor){220,230,240,255}); compare_frame(&ctx,&a,&b);
    mu_node_set_shadow(field,CLEAR,0,(MuVec2){0}); compare_frame(&ctx,&a,&b);
    mu_node_set_shadow(field,(MuColor){0,0,0,15},24,(MuVec2){0,8}); compare_frame(&ctx,&a,&b);
    field->flags &= ~MU_NODE_VISIBLE; compare_frame(&ctx,&a,&b);
    field->flags |= MU_NODE_VISIBLE; compare_frame(&ctx,&a,&b);
    group->flags &= ~MU_NODE_VISIBLE; compare_frame(&ctx,&a,&b);
    group->flags |= MU_NODE_VISIBLE; compare_frame(&ctx,&a,&b);
    const float scales[] = {.5f, 2.f};
    for (int i=0; i<2; i++) {
        mu_soft_set_scale(&a,scales[i]); mu_soft_set_scale(&b,scales[i]);
        mu_node_set_shadow(field,(MuColor){0,0,0,128},2,(MuVec2){0,8});
        mu_damage_all(&ctx); compare_frame(&ctx,&a,&b);
        field->paint_offset.x += 30; compare_frame(&ctx,&a,&b);
    }
    mu_node_remove_child(&ctx,root,group); mu_node_destroy_recursive(&ctx,group);
    compare_frame(&ctx,&a,&b);
    mu_style_shutdown(&ctx); mu_context_shutdown(&ctx);
    mu_soft_render_shutdown(&a); mu_soft_render_shutdown(&b);
}

static void test_ellipsis(const char *font) {
    MuRenderContext a,b;
    mu_soft_render_init(&a,W,H); mu_soft_render_init(&b,W,H);
    CHECK(mu_soft_set_font_file(&a,font)); CHECK(mu_soft_set_font_file(&b,font));
    MuTextStyle st; mu_text_style_init(&st); st.size = 20; st.letter_spacing = 0;
    const char *title = "\xC3\xA9\xE2\x82\xAC" "W a very long title";
    const char *want = "\xC3\xA9\xE2\x82\xAC\xE2\x80\xA6";
    MuTextMetrics tm; mu_text_measure(&a,want,&st,&tm);
    MuRect bounds = {10,10,tm.width + .01f,30};
    mu_soft_begin_frame(&a,BLACK); mu_soft_begin_frame(&b,BLACK);
    mu_draw_text_ellipsis(&a,title,bounds,&st,WHITE);
    mu_push_scissor(&b,bounds); mu_draw_text(&b,want,10,10,&st,WHITE); mu_pop_scissor(&b);
    CHECK(same(&a,&b));
    /* No fixed buffer cutoff, including multibyte prefixes. */
    char long_title[4096]; memset(long_title,'W',sizeof(long_title)-1); long_title[4095]=0;
    mu_soft_begin_frame(&a,BLACK); mu_soft_begin_frame(&b,BLACK);
    mu_draw_text_ellipsis(&a,long_title,bounds,&st,WHITE);
    mu_draw_text_ellipsis(&b,"WWWWWWWWWWWWWWWWWW",bounds,&st,WHITE);
    CHECK(same(&a,&b));
    mu_soft_begin_frame(&a,BLACK); mu_soft_begin_frame(&b,BLACK);
    mu_draw_text_ellipsis(&a,"Short",(MuRect){10,10,250,30},&st,WHITE);
    mu_draw_text(&b,"Short",10,10,&st,WHITE); CHECK(same(&a,&b));
    mu_soft_begin_frame(&a,BLACK); mu_soft_begin_frame(&b,BLACK);
    mu_draw_text_ellipsis(&a,title,(MuRect){10,10,1,30},&st,WHITE); CHECK(same(&a,&b));
    mu_soft_begin_frame(&a,BLACK); mu_soft_begin_frame(&b,BLACK);
    mu_draw_text_ellipsis(&a,"Hi\nSecond line",(MuRect){10,10,250,30},&st,WHITE);
    mu_draw_text(&b,"Hi\xE2\x80\xA6",10,10,&st,WHITE); CHECK(same(&a,&b));

    /* Placeholder ink and entered-text ink must be independent in the widget. */
    MuContext ctx; mu_context_init(&ctx,65536); mu_style_init(&ctx,NULL);
    mu_widgets_basic_register(&ctx);
    MuNode *input=mu_make_textinput(&ctx,""); mu_context_set_root(&ctx,input);
    input->bounds=(MuRect){0,0,W,H};
    mu_textinput_set_placeholder(input,"Hint");
    mu_node_set_fill(input,CLEAR); mu_node_set_border_width(input,0);
    mu_node_set_ink(input,(MuColor){255,0,0,255}); mu_node_set_placeholder_ink(input,WHITE);
    MuStyleSnapshot resolved; mu_style_resolve(&ctx,input,&resolved);
    mu_text_measure(&a,"Hint",&resolved.text,&tm);
    for (int typed=0; typed<2; typed++) {
        if (typed) mu_textinput_set_text(input,"Hint");
        mu_soft_begin_frame(&a,BLACK); mu_soft_begin_frame(&b,BLACK);
        mu_paint_all(&ctx,&a);
        mu_draw_text(&b,"Hint",10,(H-tm.height)*.5f,&resolved.text,
                      typed ? resolved.foreground : WHITE);
        CHECK(same(&a,&b));
    }
    mu_style_shutdown(&ctx); mu_context_shutdown(&ctx);
    mu_soft_render_shutdown(&a); mu_soft_render_shutdown(&b);
}

static void close_tab(void *user, int index) { *(int *)user = index; }
static void test_tabs(const char *font) {
    MuContext ctx; MuRenderContext rc;
    mu_context_init(&ctx,65536); mu_style_init(&ctx,NULL); mu_widgets_basic_register(&ctx);
    mu_soft_render_init(&rc,W,H); CHECK(mu_soft_set_font_file(&rc,font)); mu_render_bind_measure(&rc);
    MuNode *tabs = mu_make_tabs(&ctx); mu_context_set_root(&ctx,tabs);
    const char *long_title="A very long tab title that must stay on a single line and retain its entire UTF-8 suffix: \xC3\xA9";
    mu_tabs_add(&ctx,tabs,long_title);
    CHECK(strcmp(mu_tabs_label(tabs,0),long_title)==0);
    mu_tabs_add(&ctx,tabs,"Another long tab title");
    int closed = -1; mu_tabs_set_on_close(tabs,close_tab,&closed);
    for (int width = W; width >= 160; width -= 160) {
        mu_node_set_bounds(tabs,(MuRect){0,0,(float)width,H}); mu_layout_run(&ctx);
        for (int i=0;i<2;i++) {
            MuNode *tab = mu_tabs_tab(tabs,i), *title=tab->children[0], *close=tab->children[1];
            CHECK(close->bounds.w == 24.f);
            CHECK(fabsf(close->bounds.x + close->bounds.w -
                        (tab->bounds.x + tab->bounds.w - tab->layout.padding_right)) < .1f);
            CHECK(title->bounds.x + title->bounds.w <= close->bounds.x + .1f);
            MuVec2 small,big;
            const MuNodeOps *ops=mu_get_node_ops(&ctx,title->kind);
            ops->measure(&ctx,title,(MuVec2){40,100},&small);
            ops->measure(&ctx,title,(MuVec2){400,100},&big);
            CHECK(small.y == big.y);
        }
    }
    MuNode *tab = mu_tabs_tab(tabs,1), *close=tab->children[1];
    MuPointerEvent ev={0}; ev.released=true;
    ev.position=(MuVec2){close->bounds.x+12,close->bounds.y+10};
    mu_get_node_ops(&ctx,close->kind)->on_pointer(&ctx,close,&ev);
    CHECK(closed == 1 && mu_tabs_get_selection(tabs) == 0);
    mu_node_set_ink(tab,(MuColor){212,212,212,255});
    MuStyleSnapshot st; mu_style_resolve(&ctx,tab->children[0],&st); CHECK(st.foreground.r==212);
    mu_tabs_set_on_close(tabs,NULL,NULL); CHECK(!(close->flags & MU_NODE_VISIBLE));
    mu_style_shutdown(&ctx); mu_context_shutdown(&ctx);
    mu_soft_render_shutdown(&rc); mu_render_bind_measure(NULL);
}

int main(int argc,char **argv) {
    if (argc != 2) return 2;
    test_styles(); test_shadow(); test_shadow_damage(); test_ellipsis(argv[1]); test_tabs(argv[1]);
    printf("%d style/effects failure(s)\n",failures);
    return failures ? 1 : 0;
}
