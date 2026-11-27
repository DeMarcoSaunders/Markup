#include "markup/mu_document.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %d: %s\n",__LINE__,#c); failures++; } } while(0)
static void bad(const char *source, MuParseError code, size_t line, size_t column) {
    MuDiagnostic d;
    MuDocument *doc=mu_document_parse(mu_slice_from_cstr(source),0,&d);
    CHECK(!doc); CHECK(d.code==code); CHECK(d.location.line==line); CHECK(d.location.column==column);
    CHECK(d.message && *d.message); mu_document_destroy(doc);
}
int main(void) {
    char source[]="<!-- header -->\n<card class='main quiet'><label key=\"a\">Hello</label><label key='b'/></card>";
    MuDiagnostic d;
    MuDocument *doc=mu_document_parse(mu_slice_from_cstr(source),0,&d);
    CHECK(doc && d.code==MU_PARSE_OK);
    const MuElement *root=mu_document_root(doc), *label=root ? root->first_child : NULL;
    CHECK(root && root->location.line==2 && root->location.column==1);
    MuSymbol label_id=mu_document_find_symbol(doc,mu_slice_from_cstr("label"));
    CHECK(label_id && label && label->tag==label_id && label->next_sibling->tag==label_id);
    CHECK(mu_slice_equal(label->first_child->text,mu_slice_from_cstr("Hello")));
    CHECK(label->first_child->text.ptr>=source && label->first_child->text.ptr<source+sizeof(source));
    const MuAttribute *classes=mu_element_attribute(root,mu_document_find_symbol(doc,mu_slice_from_cstr("class")));
    CHECK(classes && mu_slice_equal(classes->value,mu_slice_from_cstr("main quiet")));
    MuSlice interned=mu_document_symbol_name(doc,label_id);
    CHECK(interned.ptr>=source && interned.ptr<source+sizeof(source));
    CHECK(!mu_document_find_symbol(doc,mu_slice_from_cstr("missing")));
    mu_document_destroy(doc);
    bad("<card>\n  </other>",MU_PARSE_MISMATCH,2,3);
    bad("<a x='1' x='2'/>",MU_PARSE_DUPLICATE_ATTRIBUTE,1,10);
    bad("<a x=no/>",MU_PARSE_SYNTAX,1,6);
    bad("<a/ ><b/>",MU_PARSE_SYNTAX,1,3);
    bad("<a/><b/>",MU_PARSE_SYNTAX,1,5);
    bad("<a>",MU_PARSE_SYNTAX,1,1);
    bad("<!-- never closed",MU_PARSE_SYNTAX,1,1);
    bad("<a x='broken",MU_PARSE_SYNTAX,1,7);
    bad("<a x='<'/>",MU_PARSE_SYNTAX,1,7);
    doc=mu_document_parse((MuSlice){"<a/>\0",5},0,&d); CHECK(!doc && d.code==MU_PARSE_INVALID && d.location.offset==4);
    doc=mu_document_parse((MuSlice){0},0,&d); CHECK(!doc && d.code==MU_PARSE_INVALID);
    doc=mu_document_parse(mu_slice_from_cstr("<a/>"),8,&d); CHECK(!doc && d.code==MU_PARSE_NOMEM);
    char deep[1024]={0};
    for (int i=0;i<65;i++) strcat(deep,"<a>");
    for (int i=0;i<65;i++) strcat(deep,"</a>");
    doc=mu_document_parse(mu_slice_from_cstr(deep),0,&d); CHECK(!doc && d.code==MU_PARSE_DEPTH);
    /* Every truncated prefix of a valid document must fail cleanly or be valid on its own. */
    const char *valid="<row title='literal & text'><label/>tail</row>";
    for (size_t i=0;i<=strlen(valid);i++) {
        doc=mu_document_parse((MuSlice){valid,i},0,&d);
        CHECK((doc!=NULL)==(i==strlen(valid))); mu_document_destroy(doc);
    }
    printf("%d document failure(s)\n",failures);
    return failures ? 1 : 0;
}
