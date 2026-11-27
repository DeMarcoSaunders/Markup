#include "example_host.h"
#include <stdio.h>
int main(int argc,char **argv) {
    Example e; if (!example_init(&e)) { example_destroy(&e); return 1; }
    MuNode *heading=mu_make_label(&e.ctx,"Scrollable list"); heading->role="heading";
    MuNode *scroll=mu_make_scroll(&e.ctx,MU_SCROLL_VERTICAL);
    mu_node_add_child(&e.ctx,e.root,heading); mu_node_add_child(&e.ctx,e.root,scroll);
    MuNode *content=mu_scroll_content(scroll); mu_layout_set_gap(content,6);
    for (int i=0;i<24;i++) {
        char text[64]; snprintf(text,sizeof(text),"Item %02d — copied from a stack string",i+1);
        MuNode *row=mu_make_label(&e.ctx,text); mu_layout_set_flex(row,0,0,28);
        mu_node_add_child(&e.ctx,content,row);
    }
    int result=example_run(&e,"Clipped scrolling",argc,argv); example_destroy(&e); return result;
}
