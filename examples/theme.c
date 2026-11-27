#include "example_host.h"
int main(int argc,char **argv) {
    Example e; if (!example_init(&e)) { example_destroy(&e); return 1; }
    MuStyleModule theme=*e.ctx.style;
    theme.page_bg=(MuColor){244,247,250,255}; theme.label_fg=(MuColor){31,51,66,255};
    theme.default_font_size=18; mu_style_init(&e.ctx,&theme);
    MuNode *heading=mu_make_label(&e.ctx,"Shared theme, individual styles"); heading->role="heading";
    mu_node_add_child(&e.ctx,e.root,heading);
    MuNode *field=mu_make_textinput(&e.ctx,""); mu_textinput_set_placeholder(field,"Search");
    mu_node_set_fill(field,(MuColor){255,255,255,255}); mu_node_set_radius(field,14);
    mu_node_set_border_width(field,1); mu_node_set_border_color(field,(MuColor){239,239,239,255});
    mu_node_set_placeholder_ink(field,(MuColor){212,212,212,255});
    mu_node_set_shadow(field,(MuColor){0,0,0,15},24,(MuVec2){0,8});
    mu_node_add_child(&e.ctx,e.root,field);
    MuNode *title=mu_make_label(&e.ctx,"This deliberately long single-line title ends with an ellipsis when its bounds run out");
    mu_label_set_ellipsis(title,true); mu_node_set_ink(title,(MuColor){140,140,140,255});
    mu_node_add_child(&e.ctx,e.root,title);
    int result=example_run(&e,"Theme overrides",argc,argv); example_destroy(&e); return result;
}
