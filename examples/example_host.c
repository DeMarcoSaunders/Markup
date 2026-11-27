#include "example_host.h"
#include <stdio.h>
#include <string.h>
#ifdef MARKUP_EXAMPLE_SDL
#include "markup/mu_sdl.h"
#endif
bool example_init(Example *e) {
    memset(e,0,sizeof(*e));
    mu_context_init(&e->ctx,128*1024);
    if (!mu_context_enable_node_arena(&e->ctx)) return false;
    mu_widgets_basic_register(&e->ctx); mu_style_init(&e->ctx,NULL);
    mu_soft_render_init(&e->render,480,320);
    if (!e->render.pixels || !mu_soft_set_font_file(&e->render,MARKUP_EXAMPLE_FONT)) return false;
    mu_render_bind_measure(&e->render);
    e->root=mu_make_panel(&e->ctx,true);
    if (!e->root) return false;
    e->root->role="page";
    mu_layout_set_padding_all(e->root,20); mu_layout_set_gap(e->root,12);
    mu_node_set_bounds(e->root,(MuRect){0,0,480,320}); mu_context_set_root(&e->ctx,e->root);
    return true;
}
static bool snapshot(Example *e,const char *path) {
    FILE *file=fopen(path,"wb");
    if (!file) return false;
    bool ok=fprintf(file,"P6\n480 320\n255\n")>0;
    for (int y=0;y<320 && ok;y++) for (int x=0;x<480;x++) {
        MuColor c=mu_soft_get_pixel(&e->render,x,y);
        unsigned char rgb[]={c.r,c.g,c.b};
        if (fwrite(rgb,1,3,file)!=3) { ok=false; break; }
    }
    if (fclose(file)) ok=false;
    return ok;
}
int example_run(Example *e,const char *title,int argc,char **argv) {
    if (e->ctx.last_error!=MU_OK) {
        fprintf(stderr,"%s: %s\n",title,e->ctx.last_message); return 1;
    }
    if (argc==2 && !strcmp(argv[1],"--window")) {
#ifdef MARKUP_EXAMPLE_SDL
        MuSdlApp *app=mu_sdl_create(title,480,320);
        if (!app) return 1;
        while (mu_sdl_poll(app,&e->ctx)) {
            mu_frame_begin(&e->ctx);
            int w=mu_sdl_width(app),h=mu_sdl_height(app);
            mu_soft_resize(&e->render,w,h);
            mu_node_set_bounds(e->root,(MuRect){0,0,(float)w,(float)h});
            mu_layout_run(&e->ctx); mu_sdl_frame(&e->ctx,app);
            mu_soft_begin_frame(&e->render,(MuColor){255,255,255,255});
            mu_paint_all(&e->ctx,&e->render); mu_sdl_present(app,&e->render);
        }
        mu_sdl_destroy(app); return 0;
#else
        fprintf(stderr,"--window requires -DMARKUP_WITH_SDL=ON\n"); return 2;
#endif
    }
    if (argc!=3 || strcmp(argv[1],"--snapshot")) {
        fprintf(stderr,"usage: %s --snapshot output.ppm | --window\n",argv[0]); return 2;
    }
    mu_frame_begin(&e->ctx); mu_layout_run(&e->ctx);
    if (e->ctx.layout_failed) { fprintf(stderr,"%s\n",e->ctx.last_message); return 1; }
    mu_soft_begin_frame(&e->render,(MuColor){255,255,255,255}); mu_paint_all(&e->ctx,&e->render);
    if (!snapshot(e,argv[2])) { fprintf(stderr,"cannot write %s\n",argv[2]); return 1; }
    printf("%s: wrote %s\n",title,argv[2]); return 0;
}
void example_destroy(Example *e) {
    mu_context_shutdown(&e->ctx); mu_style_shutdown(&e->ctx);
    mu_soft_render_shutdown(&e->render); mu_render_bind_measure(NULL);
}
