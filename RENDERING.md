# Rendering: software backend, damage tracking, frosted glass

Three features that interlock. The software backend renders without a GPU or a window
system; damage tracking makes it cheap enough to run a real UI on a CPU; frosted glass is
the effect that motivated most of the optimisation work, and it depends on both.

You can use the software backend without damage tracking, and damage tracking without
glass. Glass needs damage tracking to be affordable, and needs the software backend to
look like anything at all.

- [The software backend](#the-software-backend)
- [Damage tracking](#damage-tracking)
- [Frosted glass](#frosted-glass)
- [The blur cache](#the-blur-cache)
- [Performance](#performance)
- [Gotchas](#gotchas)

---

## The software backend

`markup_soft` renders into a plain ARGB8888 buffer using nothing but libc and `math.h` —
no GPU, no windowing library. Hand it a framebuffer and feed it input. It is the backend
intended for bare metal and for headless tests.

### Selecting it

Backend selection is automatic and happens at link time. Each backend library declares its
`MU_BACKEND_*` macro as a PUBLIC compile definition, so linking `markup_soft` is what
makes `markup/mu.h` resolve to the software `MuRenderContext`.

```cmake
target_link_libraries(my_app PRIVATE markup_soft markup_widgets_basic markup_input)
```

You cannot link two backends into one target. Every backend defines `struct
MuRenderContext` with a different layout, and mixing them would compile cleanly and then
write every field at the wrong offset. `mu_backend.h` turns that into a compile error
instead.

### Creating a surface

Two ways in, depending on who owns the pixels.

```c
#include "markup/mu_soft.h"

MuRenderContext rc;

/* Markup allocates and owns the surface. */
mu_soft_render_init(&rc, 1280, 720);

/* Or render straight into memory you already have — a framebuffer, a mapped
 * DRM dumb buffer, a VESA linear framebuffer. `stride` is in pixels, not bytes. */
mu_soft_render_init_borrowed(&rc, fb_pixels, 1280, 720, fb_stride_px);

mu_render_bind_measure(&rc);   /* so mu_text_measure(NULL, ...) can resolve a font */
```

Pixels are `0xAARRGGBB` in a `uint32_t`, which is byte order B,G,R,A on little-endian.
That matches `SDL_PIXELFORMAT_ARGB8888`, so an SDL host blits with no conversion pass.

### Getting the pixels back out

For a custom host, `mu_present_pixel_data` is the backend-neutral way to reach the frame.
CPU backends return the buffer; GPU backends return `NULL` because they present directly.

```c
int w, h, row_bytes;
const void *px = mu_present_pixel_data(&rc, &w, &h, &row_bytes);
if (px) memcpy(framebuffer, px, (size_t)row_bytes * h);
```

---

## Damage tracking

Repainting an entire window every frame is affordable on a GPU and not on a CPU. Damage
tracking repaints only what changed. An idle frame does no pixel work at all.

It is **opt-in**: `mu_paint_all` still repaints everything, and callers that do not want to
manage the clear/present region should keep using it.

### The frame loop

The call order matters. Collect damage, ask for the rect, clear *that rect only*, paint
clipped to it, present the same rect, then reset.

```c
mu_frame_begin(&ctx);
mu_layout_run(&ctx);

mu_damage_collect(&ctx);
if (!mu_damage_empty(&ctx)) {
    MuRect area = mu_damage_rect(&ctx);
    mu_soft_begin_frame_rect(&rc, clear_color, area);  /* clears only `area` */
    mu_paint_damaged(&ctx, &rc);                       /* paints clipped to `area` */
    mu_sdl_present_rect(app, &rc, area);               /* uploads only `area` */
}
mu_damage_reset(&ctx);

mu_frame_end(&ctx);
```

Clearing and presenting the *same* rect the paint used is the part that is easy to get
wrong. Clear more than you paint and you get holes; present less and you get stale pixels.

### What is automatic

Most damage is **discovered, not declared**. `mu_damage_collect` compares every node's
bounds and style flags against a snapshot of the previous frame, so moves, resizes,
visibility changes and hover/press/focus are all picked up without any call site
remembering to mark anything.

That matters because the failure mode of a missed invalidation — stale pixels that persist
until something happens to overlap them — is miserable to debug.

### What is not

Appearance changes that the snapshot cannot see: a slider value, a checkbox toggle, a text
edit. Anything that changes how a node paints without changing its bounds or its style
flags.

```c
s->value = new_value;
mu_node_mark_paint_dirty(node);
```

### When a stale pixel appears anyway

Set `ctx.damage_force_all = true`. Everything repaints every frame, and the flag is the
A/B reference: if the artifact disappears, a `mu_node_mark_paint_dirty` is missing
somewhere. The tree still gets walked with the flag set, so turning it back off does not
report a spurious frame of damage.

`tests/test_damage.c` uses exactly this: it renders the same interaction twice, once
partial and once forced, and compares the surfaces pixel for pixel.

---

## Frosted glass

A backdrop blur reads whatever has already been painted underneath a node, blurs it, tints
it, and writes it back masked to the node's rounded rect.

Reading the backdrop is free on this backend — it owns its pixel buffer, so there is no
framebuffer readback or render-target dance. This is the one place the CPU renderer has a
structural advantage over a GPU one.

### The easy way

```c
MuNode *glass = mu_make_panel(&ctx, true);
glass->role = "glass";                    /* translucent background + light border */
mu_node_set_backdrop_blur(glass, 16.f);   /* radius in pixels; 0 disables */
```

The `"glass"` role resolves to `glass_bg` / `glass_border` from the active
`MuStyleModule` — deliberately low alpha, because the blur is the effect and the fill only
lifts and cools it. An opaque background here hides the blur completely.

### Where to put it

**Paint order is the whole thing.** A blurred node blurs what has already been drawn, so
anything meant to show through must be painted before it.

The usual arrangement is the modal layer, which paints last and is not laid out by flex —
so the panel can be positioned freely over the content below:

```c
MuNode *overlay = mu_make_panel(&ctx, true);
overlay->role = "group";
mu_modal_bind_layer(&ctx, overlay);

MuNode *glass = mu_make_panel(&ctx, true);
glass->role = "glass";
mu_node_set_backdrop_blur(glass, 16.f);
mu_node_add_child(&ctx, overlay, glass);

/* mu_layout_run only walks ctx->root, so set these yourself each frame. */
mu_node_set_bounds(overlay, (MuRect){0.f, 0.f, (float)w, (float)h});
mu_node_set_bounds(glass, (MuRect){360.f, 140.f, 330.f, 150.f});
mu_layout_node(&ctx, glass);
```

### Doing it in your own node kind

```c
void mu_draw_backdrop_blur(MuRenderContext *rc, MuRect area, float blur_radius,
                           MuColor tint, float corner_radius);
```

`tint.a` is the mix amount: 0 leaves the blur untouched, 255 is a solid fill. Call it from
a `paint` op, never from `paint_overlay`, so the things you want to show through are
already down.

Pass your background colour **as the tint** rather than painting a fill over the result.
It is the same source-over arithmetic done once per pixel instead of twice, and it is more
correct — filling afterwards applies the rounded rect's antialiasing twice and leaves edge
pixels under-tinted. This is what the built-in `panel` does.

### Other backends

**Glass is a software-backend feature.** The other backends compile the same calls — so
your code is portable — but degrade to a flat tint with no blur, because neither has cheap
framebuffer readback. Skia could do it properly via `saveLayer` with
`SkImageFilters::Blur` if that backend is ever built out. Treat anything in this document
as `markup_soft` only.

---

## The blur cache

A blur is expensive, and its input changes far less often than its output is painted.
Anything happening *inside* a glass panel repaints the panel without touching what the
blur reads — a button hovering inside it is the common case.

The cache handles this automatically for `panel`. You do not have to do anything.

### What it keys on

A cached blur is reused only when **nothing outside the node's own subtree** has damaged
the region the blur kernel reads. A node's own children are excluded because they paint
*over* the blur, never into it.

That verdict is sticky. It is not cleared at the end of the frame that raised it, but when
the blur is actually recomputed — which may be many frames later if the node was hidden or
fell outside the damage rect in between. A per-frame flag would go stale exactly there: a
panel hidden while the content behind it moves would come back believing its cached blur
still matched.

The cache also misses when the request itself changes — geometry, blur radius, tint,
corner radius, or the clip rect. Entries are matched on node pointer *and* node id, so a
destroyed node whose address is recycled cannot be handed the dead node's pixels.

### In your own node kind

```c
MuColor tint = st.background;
if (!mu_node_backdrop_unchanged(ctx, node) ||
    !mu_backdrop_cache_try(rc, node, node->bounds, node->backdrop_blur, tint, rad)) {
    mu_draw_backdrop_blur(rc, node->bounds, node->backdrop_blur, tint, rad);
    mu_backdrop_cache_store(rc, node, node->bounds, node->backdrop_blur, tint, rad);
    mu_node_backdrop_mark_clean(node);
}
```

`mu_node_backdrop_unchanged` is the question you must ask first. `mu_backdrop_cache_try`
only verifies that the cached pixels answer the same *request*; it cannot tell that the
content underneath has moved. **Calling `try` without the `unchanged` check will happily
replay a stale blur.**

Every miss stores. That is not incidental — it is what keeps a recycled node pointer safe.

Backends whose blur is a flat tint implement all three as no-ops, so the same code is
correct everywhere.

---

## Performance

Measured on a 600×400 panel at radius 16, in a release build. Absolute numbers move a lot
with CPU clock state — the same binary varied by 40% across runs on the machine these were
taken on — so treat the ratios as the durable part and re-measure before trusting any
absolute figure.

Frame cost for the common interaction — hovering a button *inside* a 600×400 glass panel
at radius 16, so the panel repaints every frame but its backdrop never changes:

| | ms/frame |
|---|---|
| **with the cache** | **0.12 – 0.17** |
| without the cache | 1.65 – 2.20 |

A ~13× difference, at a 60/61 hit rate. Two separate things produce it:

- **The cache** turns the blur itself into a blit of the panel rect.
- **Folding the background into the blur's tint** removed a translucent rounded-rect fill
  over the whole panel — roughly 0.9 ms on its own — which used to run on *every* frame,
  cache hit or not. That is why the cached path is so cheap now rather than merely cheaper.

Three tiers, in practice:

| Situation | Cost |
|---|---|
| Nothing changed | Panel is not repainted at all — damage tracking prunes it |
| Something changed *inside* the panel | Cache hit: a blit |
| Content *behind* the panel moved | Full blur, ~1.5 ms at this size |

### Tuning knobs

In `mu_soft_blur.c`:

| Constant | Meaning |
|---|---|
| `MU_BLUR_PASSES` | 3 box passes ≈ a Gaussian. Lower is faster and boxier. |
| `MU_BLUR_HALFRES_MIN` | Radius ≥ this blurs at half resolution (default 4) |
| `MU_BLUR_QUARTERRES_MIN` | Radius ≥ this blurs at quarter resolution (default 12) |
| `MU_BACKDROP_CACHE_MAX` | Cached panels, LRU (default 8). Each costs its own area in pixels. |
| `MU_BACKDROP_TRACK_MAX` | Blurred nodes trackable per frame (default 16, in `mu_core.h`). Past this, caching switches off for the frame rather than guessing — correct, but slow. |

Downsampling is the single biggest lever, and it is close to free visually: a blur is a
low-pass filter, so the detail the downsample discards is detail the blur was about to
destroy anyway. Below `MU_BLUR_HALFRES_MIN` the image is barely blurred, so resampling
artifacts would be the most visible thing in it — small radii stay exact.

Note the cost profile inverts at small radii. At radius 16 the composite is ~75% of the
call and the box passes under 10%; at radius 3, where no downsampling happens, the box
passes are the majority of it. Tuning against one radius will disappoint at the other.

---

## Gotchas

**A blurred node forces its own damage.** Because its appearance depends on pixels it does
not own, and because the kernel samples beyond its edge, `mu_damage_collect` grows the
damage rect to cover the node plus its blur radius whenever it is touched at all. A large
blurred panel therefore makes damage rects large. This is not optional — without it the
blur re-reads its own previous output near the damage boundary and smears a little more
every frame.

**`mu_layout_run` only walks `ctx->root`.** The modal and popup layers are not laid out.
Set their bounds yourself, and call `mu_layout_node` on subtrees that need it.

**Resizing drops every cached blur.** A cached rect names a position on a surface that no
longer exists. `mu_soft_resize` clears the cache for you; follow it with `mu_damage_all`.

**The blur reads outside the scissor on purpose.** Clipping governs what may be *written*;
the blur legitimately needs to see its surroundings. Writing stays clipped.

**Fonts and images are per-`MuRenderContext`.** Two contexts do not share a glyph cache.

---

## Where to look

| | |
|---|---|
| `apps/demo_soft/main.c` | Full working loop: damage, glass on the modal layer, `--shot` |
| `tests/test_damage.c` | Damage and cache behaviour, including the partial-vs-full A/B |
| `tests/test_blur.c` | Blur correctness: edges, tint, corners, clipping |
| `SPEC_SOFT_BACKEND.md` | How the backend was built, phase by phase |
