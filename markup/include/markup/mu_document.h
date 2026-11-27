#ifndef MU_DOCUMENT_H
#define MU_DOCUMENT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct MuSlice { const char *ptr; size_t len; } MuSlice;
typedef uint32_t MuSymbol; /* document-local interned name; zero means absent */
typedef struct MuSourceLocation { size_t offset, line, column; } MuSourceLocation;
typedef enum MuParseError {
    MU_PARSE_OK, MU_PARSE_SYNTAX, MU_PARSE_MISMATCH, MU_PARSE_DUPLICATE_ATTRIBUTE,
    MU_PARSE_DEPTH, MU_PARSE_NOMEM, MU_PARSE_INVALID
} MuParseError;
typedef struct MuDiagnostic {
    MuParseError code;
    MuSourceLocation location; /* 1-based line/byte column, 0-based byte offset */
    const char *message; /* static storage; survives a failed parse */
} MuDiagnostic;
typedef struct MuAttribute {
    MuSymbol key;
    MuSlice value;
    MuSourceLocation location;
    struct MuAttribute *next;
} MuAttribute;
typedef struct MuElement {
    MuSymbol tag; /* zero for a text node */
    MuSlice text;
    MuSourceLocation location;
    struct MuElement *first_child, *next_sibling;
    MuAttribute *attributes;
} MuElement;
typedef struct MuDocument MuDocument;

/** Structural markup parser: one root, paired/self-closing tags, quoted attributes,
 * comments and text. No HTML recovery, CSS, entities or draw calls. Text/attribute
 * values are literal slices; all tag/attribute names are interned without copying.
 * SOURCE IS BORROWED: keep it immutable until document_destroy and until all slices
 * derived from it are no longer used. Structural storage uses one bounded arena.
 * capacity=0 selects 64 KiB. On failure returns NULL and a source diagnostic. */
MuDocument *mu_document_parse(MuSlice source, size_t capacity, MuDiagnostic *diagnostic);
void mu_document_destroy(MuDocument *document);
const MuElement *mu_document_root(const MuDocument *document);
/** Resolve names during initialization, then compare integer IDs during traversal. */
MuSymbol mu_document_find_symbol(const MuDocument *document, MuSlice name);
MuSlice mu_document_symbol_name(const MuDocument *document, MuSymbol symbol);
const MuAttribute *mu_element_attribute(const MuElement *element, MuSymbol key);
MuSlice mu_slice_from_cstr(const char *text);
bool mu_slice_equal(MuSlice a, MuSlice b);
#ifdef __cplusplus
}
#endif
#endif
