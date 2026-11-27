# Allocation, lifetimes, parsing, and incremental layout

MarkUp v2 keeps structural parsing, retained widget construction, layout, and painting
as separate steps. The parser is optional (`markup::document`); applications using the
C node API do not need to link it. It returns a structural tree, not HTML/CSS widgets
or a diff engine. The card example shows resolving a name once and copying a parsed
attribute into a widget before discarding the document.

## Ownership and lifetime rules

| Data | Owner | Valid until |
| --- | --- | --- |
| `MuNode *` and child arrays | Context, including detached nodes | Node destruction, `mu_context_clear_nodes`, or shutdown |
| Widget text passed to constructors/setters | Copied by the widget | Its next replacement or widget destruction |
| Text returned by `mu_textinput_get_text` or change callbacks | Input widget; borrowed by caller | Next content edit or input destruction |
| Callback `void *user_data` | Caller; never freed by MarkUp | Caller must keep it alive while the callback is installed |
| Custom `node->state` | Custom node kind | Its `destroy_state` callback; without a callback MarkUp calls `free` |
| `node->role` and registered kind names | Caller; borrowed, NUL-terminated | All uses of the node/kind; string literals are convenient |
| Theme passed to `mu_style_init` | Copied by context | `mu_style_shutdown`; call it separately from context shutdown |
| `MuNodeHandle` | Value owned by caller | Resolve before use; context must still be initialized |
| Parsed elements, attributes, intern records | Document arena | `mu_document_destroy` |
| Parser source, text/attribute slices, interned name bytes | Caller; borrowed and immutable | Caller must retain source while any document/slice uses it |
| Renderer images/fonts and owned software pixels | Render context | Render shutdown or resource replacement |
| Pixels passed to `mu_image_bind_pixels` | Caller; borrowed | Caller must retain through every paint using that binding |
| `ctx->scratch` allocations | Context | End of the current layout scope or next `mu_frame_begin`; never retain them |

Destroy nodes through the context that created them. Destruction detaches a node from
its parent and clears matching root/layer pointers. Attaching a node twice, attaching
across contexts, and creating cycles fail with `MU_ERR_INVALID`. Call
`mu_context_clear_nodes` after an abandoned build to reclaim detached nodes as well.
The context tracks them so constructor failures cannot orphan node storage.

## Arena-backed retained trees

Heap-backed nodes remain the default. Opt into node and child-array allocation from
the context's persistent arena **before creating any nodes**:

```c
mu_context_init(&ctx, 128 * 1024);
if (!mu_context_enable_node_arena(&ctx)) { /* inspect ctx.last_error */ }
mu_widgets_basic_register(&ctx);
/* Construct nodes, lay out, dispatch events, paint, and retain across frames. */
mu_context_clear_nodes(&ctx); /* destroy state, invalidate handles, reset node storage */
/* Kinds, theme, and seat callback bindings survive; build the next tree here. */
```

`mu_frame_begin` resets only layout scratch, never the retained tree. Do not manually
reset `ctx.arena` or rewind it while nodes are alive. Arena exhaustion reports
`MU_ERR_NOMEM`, with no automatic heap fallback. Existing child arrays remain valid
if growth fails. Superseded child arrays stay in the arena until the tree is cleared;
this mode suits batch builds/rebuilds. Frequently edited long-lived trees can retain
the default heap policy.

Widget state and copied strings retain their existing heap allocations and destructors.
Clearing still walks nodes to run destructors and invalidate handles; the arena reclaims
node/child storage in one reset. This is not a promise of constant-time widget teardown.
The handle registry retains its high-water allocation for reuse across builds.

`mu_arena_alloc` rejects zero/non-power-of-two alignment and arithmetic overflow.
`mu_arena_used`/`mu_arena_rewind` support temporary allocation scopes. A rewind mark
must come from that same arena, and only allocation addresses after the mark expire.

## Binding events without globals

Button, panel, checkbox, list, tabs, and seat callbacks already take an explicit user
pointer. Inputs now offer the same pattern:

```c
typedef struct Form { MuContext *ctx; MuNodeHandle summary; } Form;
static void edited(void *user_data, MuNode *input, const char *utf8) {
    Form *form = user_data;
    MuNode *summary = mu_context_resolve_handle(form->ctx, form->summary);
    if (summary) mu_label_set_text(summary, utf8);
    (void)input;
}
/* form lives at least as long as this binding */
mu_textinput_set_on_change(input, edited, &form);
```

Change callbacks run once for each actual content change, including programmatic
`set_text`; cursor movement and setting identical text do not notify. Constructors do
not notify. Removing a binding uses a `NULL` callback. Callbacks may update other
widgets; retain callback text only by copying it. Text truncation preserves UTF-8
codepoint boundaries, including for placeholders.

`mu_node_handle` returns a context-scoped slot/generation pair; resolution is O(1).
Destroyed or cleared nodes stop resolving, even when a slot or arena address is reused.
Handles from another context do not resolve. Handles expire at context shutdown and
must not be reused after reinitializing that context. Existing numeric node IDs and
`mu_context_find_id` remain available; tree searches are for setup or explicit subtree
queries, while handles are the direct binding path.

## Incremental layout and invalidation

Each node caches its most recent intrinsic measurement, available size, revision, and
layout epoch. Changing a leaf invalidates its ancestors' measurements; unrelated
subtrees retain their measurements and boxes. A parent pass visits a child's layout
only when that child is dirty, its assigned box changed, or the global epoch changed.
Unchanged assignments to layout setters do no work. `ctx.layout_stats` reports actual
measurements, measurement-cache hits, and layout visits for the last root layout call.

Use setters when changing layout/text, or call `mu_layout_mark_dirty(node)` after
editing custom measurement state, roles, or layout properties directly. Bounds changes
are also detected against the cached box. `mu_style_init` invalidates layout and paint;
after replacing the bound font/measurement backend call `mu_layout_invalidate_all(ctx)`.
Text setters invalidate paint as well as layout. Painting never runs layout implicitly.

Layout scratch is reclaimed after each subtree, keeping siblings from accumulating
temporary arrays across a pass. Scratch exhaustion sets `ctx.layout_failed` and a
`MU_ERR_NOMEM` message, leaves the affected layout dirty, and permits a retry. A host
should skip presenting a failed layout rather than show partially updated geometry.

## Overflow and clip rules

- `mu_node_set_visible(node, false)` removes a node from layout and painting and
  invalidates its container. This is distinct from clipping overflow.
- Panels clip children by default. `MU_OVERFLOW_VISIBLE` permits drawing and hits
  outside their bounds; `MU_OVERFLOW_HIDDEN` clips both to the container rectangle.
  This is a rectangular clip, not a rounded mask.
- Wrapping is supported for rows through `mu_layout_set_flex_wrap`. Column wrapping
  and grid layout are not implemented. A non-wrapping row shrinks eligible children;
  children with `flex_shrink = 0` retain their main-axis size.
- Scrolling is an explicit widget (`mu_make_scroll`) with offset state. It lays out
  natural-size content, clamps offsets, and clips content to the viewport excluding
  scrollbar gutters. Gutters cannot dispatch clicks to invisible rows.
- Every scissor intersects its ancestor. The tree pushes the container clip, paints
  its box, pushes any `MuNodeOps.children_clip` viewport for the children, pops that
  viewport, paints overlays, then pops the container clip. Custom paint ops must
  balance any additional pushes locally. Shadows precede the node's own clip.
- The software stack is bounded by `MU_SOFT_CLIP_MAX`. Excess pushes fail closed
  (draw nothing) and are counted so matching pops restore the previous clip exactly.
  Skia uses its native nested stack. Deprecated Raylib intersects/restores its bounded
  stack and also fails closed on overflow.

## Structural parser and diagnostics

```c
MuDiagnostic error;
MuDocument *doc = mu_document_parse(mu_slice_from_cstr(source), 0, &error);
if (!doc) {
    fprintf(stderr, "%zu:%zu: %s\n",
            error.location.line, error.location.column, error.message);
    /* Existing widgets remain untouched. */
}
```

The grammar accepts one root element, paired or self-closing tags, quoted attributes,
comments, and text. Names are case-sensitive ASCII identifiers (letters/underscore,
then letters, digits, underscore, hyphen, dot, or colon). Attribute values, including
`class` values, and text are zero-copy slices. Entity expansion, HTML recovery, CSS,
and widget schema validation are deliberately outside this structural layer.

All tag and attribute names share a document-local intern table. Resolve a name once
with `mu_document_find_symbol`, then compare `MuSymbol` integers while building or
updating widgets. Symbols never cross documents; do not cache them across a reparse.
No string terminator is required for source/slices passed with an explicit length.

Malformed tokens, mismatched closing tags, duplicate attributes, embedded NUL bytes,
arena exhaustion, and depth above 64 produce a structured `MuParseError`. Locations
use zero-based byte offsets, one-based lines, and one-based byte columns. Diagnostic
messages have static storage and survive failure cleanup. No partial document escapes
on failure. Structural storage is reclaimed in one arena destruction. Source positions
advance incrementally during valid parsing rather than rescanning from the start for
each token.

Focused examples and commands are in [examples/README.md](examples/README.md).
