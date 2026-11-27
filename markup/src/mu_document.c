#include "markup/mu_document.h"
#include "markup/mu_core.h"
#include <stdlib.h>
#include <string.h>

#define NAME_BUCKETS 64u
#define MAX_DEPTH 64u
typedef struct Name {
    MuSlice text;
    MuSymbol id;
    struct Name *next;
} Name;
struct MuDocument {
    MuArena *arena;
    MuSlice source;
    Name *names[NAME_BUCKETS];
    MuSymbol name_count;
    MuElement *root;
};
typedef struct Parser {
    MuDocument *doc;
    size_t pos;
    MuDiagnostic error;
    MuSourceLocation located;
} Parser;

MuSlice mu_slice_from_cstr(const char *s) { return (MuSlice){s, s ? strlen(s) : 0}; }
bool mu_slice_equal(MuSlice a, MuSlice b) {
    return a.len == b.len && (!a.len || (a.ptr && b.ptr && !memcmp(a.ptr,b.ptr,a.len)));
}
static uint32_t name_hash(MuSlice s) {
    uint32_t h = 2166136261u;
    for (size_t i=0; i<s.len; i++) h = (h ^ (unsigned char)s.ptr[i]) * 16777619u;
    return h % NAME_BUCKETS;
}
MuSymbol mu_document_find_symbol(const MuDocument *doc, MuSlice name) {
    if (!doc || !name.ptr || !name.len) return 0;
    for (Name *n=doc->names[name_hash(name)]; n; n=n->next)
        if (mu_slice_equal(n->text,name)) return n->id;
    return 0;
}
MuSlice mu_document_symbol_name(const MuDocument *doc, MuSymbol id) {
    if (doc && id) for (unsigned i=0; i<NAME_BUCKETS; i++)
        for (Name *n=doc->names[i]; n; n=n->next) if (n->id==id) return n->text;
    return (MuSlice){0};
}
const MuElement *mu_document_root(const MuDocument *doc) { return doc ? doc->root : NULL; }
const MuAttribute *mu_element_attribute(const MuElement *e, MuSymbol key) {
    if (e && key) for (MuAttribute *a=e->attributes; a; a=a->next) if (a->key==key) return a;
    return NULL;
}
static MuSourceLocation location(Parser *p, size_t offset) {
    MuSourceLocation at = p->located;
    if (offset < at.offset) at = (MuSourceLocation){0,1,1};
    for (size_t i=at.offset; i<offset; i++) {
        if (p->doc->source.ptr[i]=='\n') { at.line++; at.column=1; }
        else at.column++;
    }
    at.offset=offset;
    p->located=at;
    return at;
}
static bool fail(Parser *p, MuParseError code, size_t offset, const char *message) {
    if (!p->error.code) p->error=(MuDiagnostic){code,location(p,offset),message};
    return false;
}
static void *alloc(Parser *p, size_t size, size_t align) {
    void *data=mu_arena_alloc(p->doc->arena,size,align);
    if (!data) fail(p,MU_PARSE_NOMEM,p->pos,"document arena exhausted");
    return data;
}
static bool starts(const Parser *p, const char *s) {
    size_t len=strlen(s);
    return len<=p->doc->source.len-p->pos && !memcmp(p->doc->source.ptr+p->pos,s,len);
}
static bool space(char c) { return c==' ' || c=='\t' || c=='\r' || c=='\n'; }
static void spaces(Parser *p) {
    while (p->pos<p->doc->source.len && space(p->doc->source.ptr[p->pos])) p->pos++;
}
static bool name_start(char c) {
    return (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='_';
}
static MuSlice read_name(Parser *p) {
    size_t start=p->pos;
    if (start==p->doc->source.len || !name_start(p->doc->source.ptr[start])) {
        fail(p,MU_PARSE_SYNTAX,start,"expected a name"); return (MuSlice){0};
    }
    p->pos++;
    while (p->pos<p->doc->source.len) {
        char c=p->doc->source.ptr[p->pos];
        if (!name_start(c) && !(c>='0' && c<='9') && c!='-' && c!=':' && c!='.') break;
        p->pos++;
    }
    return (MuSlice){p->doc->source.ptr+start,p->pos-start};
}
static MuSymbol intern(Parser *p, MuSlice text) {
    MuSymbol id=mu_document_find_symbol(p->doc,text);
    if (id) return id;
    Name *name=(Name *)alloc(p,sizeof(*name),_Alignof(Name));
    if (!name) return 0;
    unsigned bucket=name_hash(text);
    *name=(Name){text,++p->doc->name_count,p->doc->names[bucket]};
    p->doc->names[bucket]=name;
    return name->id;
}
static bool comment(Parser *p) {
    size_t begin=p->pos;
    p->pos+=4;
    while (p->pos<p->doc->source.len && !starts(p,"-->")) p->pos++;
    if (p->pos==p->doc->source.len) return fail(p,MU_PARSE_SYNTAX,begin,"unterminated comment");
    p->pos+=3;
    return true;
}
static MuElement *element(Parser *p, unsigned depth) {
    if (depth>=MAX_DEPTH) { fail(p,MU_PARSE_DEPTH,p->pos,"maximum nesting depth is 64"); return NULL; }
    size_t begin=p->pos++;
    MuSlice tag=read_name(p);
    if (p->error.code) return NULL;
    MuElement *e=(MuElement *)alloc(p,sizeof(*e),_Alignof(MuElement));
    if (!e) return NULL;
    e->tag=intern(p,tag);
    e->location=location(p,begin);
    MuAttribute **tail=&e->attributes;
    for (;;) {
        size_t before=p->pos;
        spaces(p);
        if (starts(p,"/>")) { p->pos+=2; return p->error.code ? NULL : e; }
        if (starts(p,">")) { p->pos++; break; }
        if (p->pos==before) { fail(p,MU_PARSE_SYNTAX,p->pos,"expected whitespace or end of tag"); return NULL; }
        size_t attr_pos=p->pos;
        MuSlice key=read_name(p);
        if (p->error.code) return NULL;
        MuSymbol id=intern(p,key);
        if (mu_element_attribute(e,id)) { fail(p,MU_PARSE_DUPLICATE_ATTRIBUTE,attr_pos,"duplicate attribute"); return NULL; }
        spaces(p);
        if (!starts(p,"=")) { fail(p,MU_PARSE_SYNTAX,p->pos,"expected '=' after attribute name"); return NULL; }
        p->pos++; spaces(p);
        if (!starts(p,"\"") && !starts(p,"'")) { fail(p,MU_PARSE_SYNTAX,p->pos,"expected quoted attribute value"); return NULL; }
        char quote=p->doc->source.ptr[p->pos++];
        size_t value=p->pos;
        while (p->pos<p->doc->source.len && p->doc->source.ptr[p->pos]!=quote) {
            if (p->doc->source.ptr[p->pos]=='<') { fail(p,MU_PARSE_SYNTAX,p->pos,"'<' inside attribute value"); return NULL; }
            p->pos++;
        }
        if (p->pos==p->doc->source.len) { fail(p,MU_PARSE_SYNTAX,value,"unterminated attribute value"); return NULL; }
        MuAttribute *a=(MuAttribute *)alloc(p,sizeof(*a),_Alignof(MuAttribute));
        if (!a) return NULL;
        a->key=id; a->value=(MuSlice){p->doc->source.ptr+value,p->pos++-value};
        a->location=location(p,attr_pos);
        *tail=a; tail=&a->next;
    }
    MuElement **child=&e->first_child;
    while (p->pos<p->doc->source.len && !p->error.code) {
        if (starts(p,"</")) {
            size_t close=p->pos;
            p->pos+=2;
            MuSlice closing=read_name(p);
            if (p->error.code) return NULL;
            if (!mu_slice_equal(tag,closing)) { fail(p,MU_PARSE_MISMATCH,close,"closing tag does not match opening tag"); return NULL; }
            spaces(p);
            if (!starts(p,">")) { fail(p,MU_PARSE_SYNTAX,p->pos,"expected '>' after closing tag"); return NULL; }
            p->pos++; return e;
        }
        if (starts(p,"<!--")) { if (!comment(p)) return NULL; continue; }
        MuElement *next;
        if (starts(p,"<")) next=element(p,depth+1);
        else {
            size_t text=p->pos;
            while (p->pos<p->doc->source.len && p->doc->source.ptr[p->pos]!='<') p->pos++;
            next=(MuElement *)alloc(p,sizeof(*next),_Alignof(MuElement));
            if (next) { next->text=(MuSlice){p->doc->source.ptr+text,p->pos-text}; next->location=location(p,text); }
        }
        if (!next) return NULL;
        *child=next; child=&next->next_sibling;
    }
    fail(p,MU_PARSE_SYNTAX,begin,"unclosed element");
    return NULL;
}
MuDocument *mu_document_parse(MuSlice source, size_t capacity, MuDiagnostic *diagnostic) {
    if (diagnostic) *diagnostic=(MuDiagnostic){MU_PARSE_OK,{0,1,1},"ok"};
    if (!source.ptr) {
        if (diagnostic) *diagnostic=(MuDiagnostic){MU_PARSE_INVALID,{0,1,1},"source pointer is null"};
        return NULL;
    }
    MuDocument *doc=(MuDocument *)calloc(1,sizeof(*doc));
    if (doc) { doc->source=source; doc->arena=mu_arena_create(capacity ? capacity : 65536); }
    if (!doc || !doc->arena) {
        mu_document_destroy(doc);
        if (diagnostic) *diagnostic=(MuDiagnostic){MU_PARSE_NOMEM,{0,1,1},"cannot allocate document arena"};
        return NULL;
    }
    Parser p={doc,0,{0},{0,1,1}};
    const char *nul=(const char *)memchr(source.ptr,0,source.len);
    if (nul) fail(&p,MU_PARSE_INVALID,(size_t)(nul-source.ptr),"embedded NUL in source");
    spaces(&p);
    while (!p.error.code && starts(&p,"<!--")) { comment(&p); spaces(&p); }
    if (!p.error.code) {
        if (!starts(&p,"<")) fail(&p,MU_PARSE_SYNTAX,p.pos,"expected a root element");
        else doc->root=element(&p,0);
    }
    spaces(&p);
    while (!p.error.code && starts(&p,"<!--")) { comment(&p); spaces(&p); }
    if (!p.error.code && p.pos!=source.len) fail(&p,MU_PARSE_SYNTAX,p.pos,"expected exactly one root element");
    if (p.error.code) {
        if (diagnostic) *diagnostic=p.error;
        mu_document_destroy(doc); return NULL;
    }
    return doc;
}
void mu_document_destroy(MuDocument *doc) {
    if (doc) { mu_arena_destroy(doc->arena); free(doc); }
}
