#include "markup/mu_soft.h"
#include "markup/mu_widgets_basic.h"
#include "markup/mu_layout_flex.h"
#include "markup/mu_input.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %d: %s\n",__LINE__,#c); failures++; } } while(0)
typedef struct Leaf { float height; int measures; int *destroyed; } Leaf;
static void measure(MuContext *ctx, MuNode *n, MuVec2 avail, MuVec2 *out) {
    (void)ctx; (void)avail;
    Leaf *s=(Leaf *)n->state; s->measures++; *out=(MuVec2){40,s->height};
}
static void destroy(MuContext *ctx, MuNode *n) {
    (void)ctx; Leaf *s=(Leaf *)n->state;
    if (s) { if (s->destroyed) (*s->destroyed)++; free(s); }
}
static const MuNodeOps OPS={.measure=measure,.destroy_state=destroy};
static MuNode *leaf(MuContext *ctx,uint32_t kind,int *destroyed) {
    Leaf *s=(Leaf *)calloc(1,sizeof(*s)); s->height=20; s->destroyed=destroyed;
    MuNode *n=mu_node_create(ctx,kind,s); if (!n) free(s); return n;
}
static void arena_and_handles(void) {
    MuArena *arena=mu_arena_create(128);
    CHECK(!mu_arena_alloc(arena,1,0)); CHECK(!mu_arena_alloc(arena,1,3));
    CHECK(!mu_arena_alloc(arena,SIZE_MAX,8));
    CHECK(mu_arena_alloc(arena,16,16)!=NULL);
    size_t mark=mu_arena_used(arena);
    void *a=mu_arena_alloc(arena,8,8); mu_arena_rewind(arena,mark);
    CHECK(a==mu_arena_alloc(arena,8,8));
    mu_arena_reset(arena); CHECK(mu_arena_used(arena)==0); mu_arena_destroy(arena);
    MuContext ctx,other; mu_context_init(&ctx,65536); mu_context_init(&other,65536);
    CHECK(mu_context_enable_node_arena(&ctx));
    uint32_t kind=mu_register_node_kind(&ctx,"leaf",&OPS);
    int destroyed=0;
    MuNode *root=leaf(&ctx,kind,&destroyed); mu_context_set_root(&ctx,root);
    MuNodeHandle stale=mu_node_handle(root);
    for (int i=0;i<20;i++) CHECK(mu_node_add_child(&ctx,root,leaf(&ctx,kind,&destroyed)));
    CHECK(root->arena_owned && root->child_cap>=20);
    CHECK(!mu_node_add_child(&ctx,root->children[0],root)); CHECK(ctx.last_error==MU_ERR_INVALID);
    CHECK(!mu_node_add_child(&ctx,root,root->children[0]));
    CHECK(mu_context_resolve_handle(&ctx,stale)==root);
    CHECK(!mu_context_resolve_handle(&other,stale));
    CHECK(!mu_context_enable_node_arena(&ctx));
    CHECK(leaf(&ctx,kind,&destroyed)!=NULL); /* detached nodes are owned too */
    mu_context_clear_nodes(&ctx);
    CHECK(destroyed==22 && mu_arena_used(ctx.arena)==0 && !ctx.root);
    CHECK(!mu_context_resolve_handle(&ctx,stale));
    MuNode *new_node=leaf(&ctx,kind,&destroyed);
    CHECK(new_node && !mu_context_resolve_handle(&ctx,stale));
    MuNodeHandle new_handle=mu_node_handle(new_node);
    mu_node_destroy_recursive(&ctx,new_node); CHECK(!mu_context_resolve_handle(&ctx,new_handle));
    mu_context_shutdown(&ctx); mu_context_shutdown(&other);
    mu_context_init(&ctx,sizeof(MuNode)+16); CHECK(mu_context_enable_node_arena(&ctx));
    kind=mu_register_node_kind(&ctx,"leaf",&OPS);
    CHECK(leaf(&ctx,kind,NULL)!=NULL); CHECK(leaf(&ctx,kind,NULL)==NULL);
    CHECK(ctx.last_error==MU_ERR_NOMEM); mu_context_shutdown(&ctx);
}
static void box(MuNode *n,float x,float y,float w,float h) {
    CHECK(fabsf(n->bounds.x-x)<.01f && fabsf(n->bounds.y-y)<.01f);
    CHECK(fabsf(n->bounds.w-w)<.01f && fabsf(n->bounds.h-h)<.01f);
}
static void layout(void) {
    MuContext ctx; mu_context_init(&ctx,65536); mu_widgets_basic_register(&ctx);
    uint32_t kind=mu_register_node_kind(&ctx,"fixed",&OPS);
    MuNode *root=mu_make_panel(&ctx,false), *left=mu_make_panel(&ctx,true), *right=mu_make_panel(&ctx,true);
    mu_context_set_root(&ctx,root); mu_node_set_bounds(root,(MuRect){0,0,300,180});
    mu_layout_set_padding_all(root,10); mu_layout_set_gap(root,20);
    mu_layout_set_min_size(left,120,0); mu_layout_set_max_size(left,120,0);
    mu_layout_set_min_size(right,120,0); mu_layout_set_max_size(right,120,0);
    mu_layout_set_padding_all(left,5); mu_layout_set_padding_all(right,5); mu_layout_set_gap(left,5);
    MuNode *a=leaf(&ctx,kind,NULL),*b=leaf(&ctx,kind,NULL),*c=leaf(&ctx,kind,NULL);
    mu_node_add_child(&ctx,root,left); mu_node_add_child(&ctx,root,right);
    mu_node_add_child(&ctx,left,a); mu_node_add_child(&ctx,left,b); mu_node_add_child(&ctx,right,c);
    mu_frame_begin(&ctx); mu_layout_run(&ctx);
    box(left,10,10,120,160); box(right,150,10,120,160);
    box(a,15,15,110,20); box(b,15,40,110,20); box(c,155,15,110,20);
    int old=((Leaf *)c->state)->measures;
    mu_frame_begin(&ctx); mu_layout_run(&ctx); CHECK(ctx.layout_stats.layouts==0 && ctx.layout_stats.measures==0);
    mu_layout_set_gap(root,20); mu_layout_run(&ctx); CHECK(ctx.layout_stats.layouts==0);
    ((Leaf *)a->state)->height=30; mu_layout_mark_dirty(a); mu_layout_run(&ctx);
    box(b,15,50,110,20); CHECK(((Leaf *)c->state)->measures==old);
    CHECK(ctx.layout_stats.measure_hits>0);
    mu_layout_invalidate_all(&ctx); mu_layout_run(&ctx); CHECK(((Leaf *)c->state)->measures>old);
    mu_arena_destroy(ctx.scratch); ctx.scratch=mu_arena_create(8); mu_layout_mark_dirty(root);
    mu_layout_run(&ctx); CHECK(ctx.layout_failed && ctx.last_error==MU_ERR_NOMEM);
    CHECK(root->flags & MU_NODE_LAYOUT_DIRTY);
    mu_arena_destroy(ctx.scratch); ctx.scratch=mu_arena_create(32768);
    mu_layout_run(&ctx); CHECK(!ctx.layout_failed && !(root->flags & MU_NODE_LAYOUT_DIRTY));
    /* A changed root box is detected even for callers assigning bounds directly. */
    root->bounds.x=10; mu_layout_run(&ctx); box(a,25,15,110,30);
    mu_node_set_visible(a,false); mu_layout_run(&ctx); box(b,25,15,110,20);
    mu_context_clear_nodes(&ctx);
    /* Row wrapping is deterministic and does not require a renderer or a font. */
    root=mu_make_panel(&ctx,false); mu_context_set_root(&ctx,root);
    mu_node_set_bounds(root,(MuRect){0,0,100,100}); mu_layout_set_padding_all(root,0);
    mu_layout_set_gap(root,5); mu_layout_set_flex_wrap(root,true);
    a=leaf(&ctx,kind,NULL); b=leaf(&ctx,kind,NULL); c=leaf(&ctx,kind,NULL);
    mu_node_add_child(&ctx,root,a); mu_node_add_child(&ctx,root,b); mu_node_add_child(&ctx,root,c);
    mu_frame_begin(&ctx); mu_layout_run(&ctx);
    box(a,0,0,40,20); box(b,45,0,40,20); box(c,0,25,40,20);
    mu_context_shutdown(&ctx);
}
static void clipping(void) {
    MuContext ctx; MuRenderContext rc;
    mu_context_init(&ctx,65536); mu_widgets_basic_register(&ctx); mu_soft_render_init(&rc,160,160);
    MuNode *root=mu_make_panel(&ctx,true), *child=mu_make_button(&ctx,"",NULL,NULL);
    mu_context_set_root(&ctx,root); mu_node_add_child(&ctx,root,child);
    root->bounds=(MuRect){10,10,80,80}; child->bounds=(MuRect){70,70,60,60};
    mu_node_set_fill(root,(MuColor){0}); mu_node_set_border_width(root,0);
    mu_node_set_fill(child,(MuColor){255,0,0,255}); mu_node_set_radius(child,0); mu_node_set_border_width(child,0);
    CHECK(!mu_input_node_at(&ctx,(MuVec2){110,80}));
    mu_soft_begin_frame(&rc,(MuColor){0}); mu_paint_all(&ctx,&rc);
    CHECK(mu_soft_get_pixel(&rc,80,80).r==255); CHECK(mu_soft_get_pixel(&rc,110,80).a==0);
    mu_node_set_overflow(root,MU_OVERFLOW_VISIBLE);
    CHECK(mu_input_node_at(&ctx,(MuVec2){110,80})==child);
    mu_soft_begin_frame(&rc,(MuColor){0});
    mu_push_scissor(&rc,(MuRect){10,10,60,60}); mu_push_scissor(&rc,(MuRect){40,40,60,60});
    mu_draw_rect(&rc,(MuRect){0,0,160,160},(MuColor){255,0,0,255},(MuColor){0},0,0);
    CHECK(mu_soft_get_pixel(&rc,50,50).r==255); CHECK(mu_soft_get_pixel(&rc,80,50).a==0);
    mu_pop_scissor(&rc);
    for (int i=0;i<MU_SOFT_CLIP_MAX+3;i++) mu_push_scissor(&rc,(MuRect){0,0,160,160});
    mu_draw_rect(&rc,(MuRect){0,0,160,160},(MuColor){0,255,0,255},(MuColor){0},0,0);
    CHECK(mu_soft_get_pixel(&rc,50,50).g==0); /* overflow fails closed */
    for (int i=0;i<MU_SOFT_CLIP_MAX+3;i++) mu_pop_scissor(&rc);
    mu_draw_rect(&rc,(MuRect){0,0,160,160},(MuColor){0,0,255,255},(MuColor){0},0,0);
    CHECK(mu_soft_get_pixel(&rc,20,20).b==255 && mu_soft_get_pixel(&rc,80,50).a==0);
    mu_pop_scissor(&rc); CHECK(rc.scissor_depth==0 && rc.scissor_overflow==0);
    mu_context_clear_nodes(&ctx);
    MuNode *scroll=mu_make_scroll(&ctx,MU_SCROLL_VERTICAL);
    mu_context_set_root(&ctx,scroll); mu_node_set_bounds(scroll,(MuRect){0,0,80,60});
    MuNode *content=mu_scroll_content(scroll), *first=NULL;
    for (int i=0;i<12;i++) {
        MuNode *row=mu_make_button(&ctx,"",NULL,NULL); if (!i) first=row;
        mu_layout_set_flex(row,0,0,30); mu_node_set_fill(row,(MuColor){255,0,0,255});
        mu_node_add_child(&ctx,content,row);
    }
    mu_frame_begin(&ctx); mu_layout_run(&ctx);
    CHECK(mu_input_node_at(&ctx,(MuVec2){5,5})==first);
    CHECK(mu_input_node_at(&ctx,(MuVec2){79,5})==scroll); /* gutter isn't a row */
    mu_scroll_set_offset(&ctx,scroll,0,99999);
    float x,y; mu_scroll_get_offset(scroll,&x,&y); CHECK(y>0 && y<99999 && content->bounds.y==-y);
    mu_node_set_bounds(scroll,(MuRect){0,0,5,60}); mu_layout_run(&ctx);
    mu_soft_begin_frame(&rc,(MuColor){0}); mu_paint_all(&ctx,&rc);
    CHECK(rc.scissor_depth==0 && rc.scissor_overflow==0); /* even a zero-width viewport */
    mu_context_shutdown(&ctx); mu_soft_render_shutdown(&rc);
}
typedef struct Form { MuContext *ctx; MuNodeHandle input; int changes; char value[256]; } Form;
static void changed(void *user,MuNode *input,const char *text) {
    Form *form=(Form *)user;
    CHECK(mu_context_resolve_handle(form->ctx,form->input)==input);
    form->changes++; snprintf(form->value,sizeof(form->value),"%s",text);
}
static void callbacks(void) {
    MuContext ctx; mu_context_init(&ctx,65536); mu_widgets_basic_register(&ctx);
    MuNode *input=mu_make_textinput(&ctx,""); mu_context_set_root(&ctx,input);
    Form form={&ctx,mu_node_handle(input),0,{0}}; mu_textinput_set_on_change(input,changed,&form);
    mu_textinput_set_text(input,"Ada"); mu_textinput_set_text(input,"Ada"); CHECK(form.changes==1);
    mu_textinput_insert_utf8(&ctx,input,"!"); CHECK(form.changes==2 && !strcmp(form.value,"Ada!"));
    MuKeyEvent key={0}; key.pressed=true; key.key=MU_KEY_LEFT;
    mu_get_node_ops(&ctx,input->kind)->on_key(&ctx,input,&key); CHECK(form.changes==2);
    key.key=MU_KEY_DELETE; mu_get_node_ops(&ctx,input->kind)->on_key(&ctx,input,&key);
    CHECK(form.changes==3 && !strcmp(form.value,"Ada"));
    CHECK(mu_get_node_ops(&ctx,input->kind)->on_char(&ctx,input,0xE9)); CHECK(form.changes==4);
    mu_context_clear_nodes(&ctx); CHECK(!mu_context_resolve_handle(&ctx,form.input)); mu_context_shutdown(&ctx);
}
int main(void) {
    arena_and_handles(); layout(); clipping(); callbacks();
    printf("%d foundation failure(s)\n",failures); return failures ? 1 : 0;
}
