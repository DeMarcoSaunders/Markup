#include "example_host.h"
#include <stdio.h>
typedef struct Form { MuContext *ctx; MuNodeHandle echo; char name[256]; } Form;
static void edited(void *user_data,MuNode *input,const char *utf8) {
    (void)input; Form *form=(Form *)user_data;
    snprintf(form->name,sizeof(form->name),"%s",utf8);
    MuNode *echo=mu_context_resolve_handle(form->ctx,form->echo);
    if (echo) mu_label_set_text(echo,form->name);
}
int main(int argc,char **argv) {
    Example e; if (!example_init(&e)) { example_destroy(&e); return 1; }
    MuNode *heading=mu_make_label(&e.ctx,"Your name"); heading->role="heading";
    MuNode *input=mu_make_textinput(&e.ctx,""); mu_textinput_set_placeholder(input,"Enter your name");
    MuNode *echo=mu_make_label(&e.ctx,"");
    mu_node_add_child(&e.ctx,e.root,heading); mu_node_add_child(&e.ctx,e.root,input); mu_node_add_child(&e.ctx,e.root,echo);
    Form state={&e.ctx,mu_node_handle(echo),{0}};
    mu_textinput_set_on_change(input,edited,&state); mu_textinput_set_text(input,"Ada Lovelace");
    int result=example_run(&e,"Form with caller-owned state",argc,argv); example_destroy(&e); return result;
}
