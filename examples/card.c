#include "example_host.h"
#include "markup/mu_document.h"
#include <stdio.h>
typedef struct Card { MuNode *label; int clicks; } Card;
static void clicked(void *user_data) {
    Card *card=(Card *)user_data;
    char text[64]; snprintf(text,sizeof(text),"Clicked %d times",++card->clicks);
    mu_label_set_text(card->label,text);
}
int main(int argc,char **argv) {
    Example e; if (!example_init(&e)) { example_destroy(&e); return 1; }
    /* Parsing has no layout/drawing side effects. Only copied widget text survives
     * document destruction; the structural parser does not impose a widget schema. */
    MuDiagnostic error;
    MuDocument *doc=mu_document_parse(mu_slice_from_cstr("<card title='A small card'/>"),0,&error);
    if (!doc) { fprintf(stderr,"%zu:%zu: %s\n",error.location.line,error.location.column,error.message); example_destroy(&e); return 1; }
    MuSymbol title=mu_document_find_symbol(doc,mu_slice_from_cstr("title"));
    MuSlice value=mu_element_attribute(mu_document_root(doc),title)->value;
    char heading[64]; snprintf(heading,sizeof(heading),"%.*s",(int)value.len,value.ptr);
    MuNode *card=mu_make_panel(&e.ctx,true); mu_node_add_child(&e.ctx,e.root,card);
    mu_layout_set_padding_all(card,20); mu_layout_set_gap(card,12);
    MuNode *label=mu_make_label(&e.ctx,heading); label->role="heading"; mu_node_add_child(&e.ctx,card,label);
    mu_document_destroy(doc);
    Card state={mu_make_label(&e.ctx,"Clicked 0 times"),0};
    mu_node_add_child(&e.ctx,card,state.label);
    MuNode *button=mu_make_button(&e.ctx,"Click me",clicked,&state); mu_node_add_child(&e.ctx,card,button);
    int result=example_run(&e,"Card and button",argc,argv); example_destroy(&e); return result;
}
