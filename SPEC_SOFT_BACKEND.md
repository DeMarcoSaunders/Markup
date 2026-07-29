# SPEC: Software Rasterizer Backend (`mu_soft`)

Plan for a dependency-free rendering backend, enabling Markup to run on bare metal
and non-Linux targets without OpenGL, Skia, or a windowing system.

---

## 1. Goal

Produce a third Markup backend that rasterizes the UI tree into a plain RGBA
framebuffer using only `libc` + `math.h`, so that porting Markup to a new OS
reduces to:

1. Hand it a pointer to a framebuffer.
2. Feed it pointer/key events.

**Non-goals for this spec:** compositor, multi-process client surfaces, IPC/window
protocol, text shaping (HarfBuzz), bidi, IME, accessibility. Those belong to an OS
UI layer built *on top* of Markup, not to Markup itself.

---

## 2. Why this is tractable

The backend seam is already real. Raylib is confined to `mu_raylib.c` (348 lines,
22 raylib calls). A second backend (`mu_sdl.c` + `mu_skia.cpp`) already proves the
abstraction holds.

### The contract — everything a backend must implement

| Group | Functions |
|---|---|
| Lifecycle | `mu_render_init`, `mu_render_shutdown`, `mu_render_begin`, `mu_render_end`, `mu_render_bind_measure` |
| Struct | `struct MuRenderContext` (backend-owned definition) |
| Draw | `mu_draw_rect`, `mu_draw_text`, `mu_draw_image`, `mu_push_scissor`, `mu_pop_scissor` |
| Text | `mu_text_measure`, `mu_font_load_file` |
| Image | `mu_image_load_file`, `mu_image_get_size` |
| Frame | `mu_soft_frame` (pump input into `mu_input_dispatch_*`) |

**~15 functions.** That is the whole job.

### What comes free (already backend-neutral — do not reimplement)

- **Word wrapping** — `mu_text_layout.c` implements `mu_text_measure_wrapped` and
  `mu_draw_text_wrapped` purely in terms of the `mu_text_measure` / `mu_draw_text`
  primitives. Implement one measure function and wrapping follows.
- **Paint traversal** — `mu_paint.c` (`mu_paint_tree`, `mu_paint_all`).
- **Image geometry** — `mu_image_util.c` (`mu_image_fit_dst`, `mu_image_resolve_src`,
  sprite sheets).
- **PNG decode** — `mu_image_decode.c` already yields raw RGBA8 (`MuImageRgba`).
- **Everything above the seam** — layout, style, input dispatch, all widgets.

### The harness already exists

`mu_sdl.c:154` consumes a raw pixel buffer, not a Skia object:

```c
const void *pixels = mu_skia_pixel_data(rc, &w, &h, &stride);
SDL_UpdateTexture(app->texture, NULL, pixels, stride);
```

Providing `mu_soft_pixel_data()` with the same signature makes `mu_sdl.c` work
against the software rasterizer **unchanged**. This means the entire rasterizer is
developed and debugged in a normal desktop window with gdb and a profiler. Only the
final phase touches bare metal.

---

## 3. Architecture

```
markup/include/markup/mu_soft.h    MuRenderContext def + soft-specific API
markup/src/mu_soft.c               backend entry points (the ~15 contract fns)
markup/src/mu_soft_raster.c        pure pixel ops: blend, spans, SDF, blit
markup/src/mu_soft_text.c          stb_truetype glyph cache + atlas
markup/vendor/stb_truetype.h       vendored, public domain
markup/src/mu_fb.c                 Phase 6: framebuffer target
markup/src/mu_evdev.c              Phase 6: input source
tests/test_raster.c                pixel-level assertions
```

### Pixel format

Store as `uint32_t` in `0xAARRGGBB` (little-endian byte order B,G,R,A). This matches
`SDL_PIXELFORMAT_ARGB8888` used at `mu_sdl.c:47`, so `SDL_UpdateTexture` needs **zero
conversion**. Pack from `MuColor`:

```c
#define MU_SOFT_PACK(c) (((uint32_t)(c).a<<24)|((uint32_t)(c).r<<16)|((uint32_t)(c).g<<8)|(uint32_t)(c).b)
```

### Surface

```c
typedef struct MuSoftSurface {
    uint32_t *pixels;
    int width, height;
    int stride;      /* in pixels, not bytes */
} MuSoftSurface;
```

---

## 4. Phases

### Phase 0 — Unblock backend selection · ~1 day

Three existing issues make a third backend impossible to link cleanly. Fix first.

- **`mu.h` is raylib-only.** `mu.h:9` includes `mu_raylib.h` unconditionally, and
  both `mu_raylib.h` and `mu_skia.h` define `struct MuRenderContext` with *different
  layouts*. Any TU including `mu.h` in a non-raylib build gets the wrong struct
  layout with no compiler error. **This is already broken for the SDL/Skia backend
  today.** Gate behind `MU_BACKEND_RAYLIB` / `MU_BACKEND_SKIA` / `MU_BACKEND_SOFT`.
- **Backend statics.** `g_mu_measure_rc` and `g_mu_ui_font` (`mu_raylib.c:12`) are
  file-scope globals — the same single-instance problem as the old `K_PANEL` kind
  IDs. Move onto `MuRenderContext` while defining the new one.
- **CMake.** Add `MARKUP_BACKEND=raylib|skia|soft`, compile exactly one backend TU.

**Exit criteria:** `-DMARKUP_BACKEND=skia` builds and runs; `mu.h` is safe to include
under any backend.

---

### Phase 1 — Rasterizer core · ~2–3 days

Square-cornered rects only. No text, no images.

- `MuSoftSurface` alloc/free/clear.
- Clip stack (32 deep, rect intersection) backing `mu_push_scissor` / `mu_pop_scissor`.
- Source-over blend. Use the standard 8-bit approximation:
  `t = x*a + 0x80; out = (t + (t>>8)) >> 8`.
- `fill_span` (clipped, blended) and `fill_rect`.
- `mu_draw_rect` with `radius` ignored.
- `mu_soft_pixel_data()`; point `mu_sdl.c` at it.

**Exit criteria:** the demo app renders in an SDL window with all panels, buttons,
and chrome as correctly-positioned flat colored boxes. Layout is visibly correct.

---

### Phase 2 — Rounded rects, borders, AA · ~2 days

**Decision: use an SDF, not analytic scanline spans.** One function handles fill,
border, and any radius; uniform AA quality; ~60 lines instead of ~180.

```c
static float sdf_round_box(float px, float py, float hw, float hh, float r) {
    float qx = fabsf(px) - hw + r, qy = fabsf(py) - hh + r;
    float ax = qx > 0 ? qx : 0, ay = qy > 0 ? qy : 0;
    float m = qx > qy ? qx : qy;
    return sqrtf(ax*ax + ay*ay) + (m < 0 ? m : 0) - r;
}
/* coverage for 1px AA */
cov = clampf(0.5f - d, 0.f, 1.f);
```

Border falls out as `cov_outer - cov_inner`, where inner is inset by `border_w`.

**Perf escape hatch (defer until measured):** only evaluate the SDF inside the four
`r × r` corner boxes. The cross-shaped interior is fully opaque and span-fills.

**Exit criteria:** visual parity with the raylib backend for all non-text chrome.
Side-by-side screenshots differ only in AA quality.

---

### Phase 3 — Text via stb_truetype · ~3–4 days

Do not write a font rasterizer. `stb_truetype.h` is public domain, single-header,
and needs only `libc` + `math.h`. It provides TTF parsing, glyph rasterization to an
8-bit coverage bitmap, and metrics (advance, kerning, bbox) — turning the hardest
item on this list into one of the easier ones.

- Vendor `stb_truetype.h`.
- **Glyph cache** keyed by `(font_id, size_quantized_to_0.5px, codepoint)`, lazily
  rasterized. Bounded memory, handles arbitrary `MuTextStyle.size`. Simple
  open-addressing table is fine.
- **Atlas**: growable R8 buffer with a shelf packer (~60 lines). Entries store
  atlas offset, `w`, `h`, `xoff`, `yoff`, `xadvance`.
- `mu_font_load_file` → `stbtt_InitFont`, assign slot id.
- `mu_text_measure` → sum advances + kerning. No rasterization needed.
- `mu_draw_text` → per-glyph coverage blit, modulated by `fg`.
- Synthetic bold/italic: mirror the raylib approach (`mu_raylib.c:181`) — redraw
  offset by 1px for weight ≥ 600, shear for italic.

**Deliberately deferred:** subpixel/LCD AA (needs panel geometry, complicates
blending) and gamma-correct blending (text will look slightly thin; acceptable in v1,
note it).

**Exit criteria:** labels, buttons, text inputs, and wrapped text all render legibly.
`mu_text_measure_wrapped` works with zero changes to `mu_text_layout.c`.

---

### Phase 4 — Images · ~1–2 days

- `mu_image_load_file` → call the existing `mu_image_decode_rgba_file`, store the
  RGBA8 buffer in an image slot. No new decoder needed.
- `mu_image_get_size` → slot lookup.
- `mu_draw_image` → bilinear-sampled blit with scale + tint. Reuse
  `mu_image_resolve_src` and `mu_image_fit_dst` for all geometry.

**Exit criteria:** imagebox and sprite-sheet widgets render correctly under all three
`MuImageFit` modes.

---

### Phase 5 — Paint damage tracking · ~2–3 days

`mu_paint_all` currently repaints the entire tree unconditionally every frame. On a
GPU this is invisible. Software-rasterizing a full 1080p frame every tick is the
difference between "usable" and "pegged CPU." **Add this before bare metal — it is
far easier now than as a retrofit.**

- Add `MU_NODE_PAINT_DIRTY`, mirroring the existing `MU_NODE_LAYOUT_DIRTY` propagation
  in `mu_layout_flex.c:23`.
- Mark dirty on: style-affecting flag changes (hover/press/focus/disabled/visible),
  state mutation in widgets, layout changes, tree mutation.
- Accumulate a **single union damage rect** on `MuContext`. Start here — it is trivial
  and already a large win. Tiles or per-node rect lists only if measurement demands it.
- `mu_paint_all` clips to the damage rect and skips clean subtrees.
- Backend presents only the damaged region.

**Exit criteria:** an idle frame with no input costs approximately zero fill. Hovering
one button repaints roughly that button's bounds, not the screen.

---

### Phase 6 — Bare metal target · ~3–5 days (target-dependent)

By this point the rasterizer is proven; only the two OS-facing edges are new.

- **`mu_fb.c`** — acquire a target surface as `(pointer, width, height, stride, format)`.
  Concrete options: DRM dumb buffer, VESA/GOP linear framebuffer, or a custom OS
  syscall. Present = `memcpy` of the damage rect. Double-buffer if the target tears.
- **`mu_evdev.c`** — translate the OS input source into `MuPointerEvent` / `MuKeyEvent`.
  Key IDs in `mu_input.h:25` are already backend-neutral, so this is a lookup table.
  Mirror the dispatch order in `raylib_dispatch_input` (`mu_raylib.c:277`): hover →
  pointer → tab → editing keys → chars → drag → wheel.

**Exit criteria:** the demo app runs with no SDL, no X/Wayland, no OpenGL, no Skia.

---

## 5. Testing

A rasterizer without pixel tests is a debugging nightmare. Build these alongside
Phase 1, not afterward.

- **Primitive assertions** (`tests/test_raster.c`) — fill a rect at known coordinates,
  assert exact pixel values at corners, edges, and just outside. Cheap and catches
  off-by-one errors immediately.
- **Clip canary** — fill the surface border with a sentinel color, run a full paint,
  assert the sentinel is untouched. Catches every clip-stack escape.
- **Blend identity** — `src.a == 255` must produce exactly `src`; `src.a == 0` must
  leave `dst` bit-identical.
- **Cross-backend diff** — render the same tree under raylib and soft, dump both to
  PNG, diff visually. Not pixel-exact (AA differs), but catches layout and geometry
  regressions fast.

---

## 6. Risks

| Risk | Mitigation |
|---|---|
| SDF per-pixel cost dominates fill | Corner-box optimization (Phase 2); interior span-fills |
| Glyph cache thrashes on many sizes | Quantize size to 0.5px; LRU eviction; measure before optimizing |
| Text looks thin vs. GPU backends | Known gamma issue; document, defer, revisit with real content |
| Phase 6 target lacks a linear framebuffer | Identify the target's display path **before** starting Phase 6 |
| Damage tracking misses an invalidation → stale pixels | Debug flag forcing full repaint; A/B against it |

---

## 7. Timeline

| Phase | Days |
|---|---|
| 0 — Unblock backend selection | 1 |
| 1 — Rasterizer core | 2–3 |
| 2 — Rounded rects + AA | 2 |
| 3 — Text (stb_truetype) | 3–4 |
| 4 — Images | 1–2 |
| 5 — Paint damage tracking | 2–3 |
| 6 — Bare metal target | 3–5 |
| **Total** | **14–20 days focused** (~1–2 months part-time) |

Phases 0–5 happen entirely in a desktop SDL window. Only Phase 6 touches the target OS.

---

## 8. Sequencing note

Phases 0 → 1 → 2 are strictly ordered. Phase 3 (text) and Phase 4 (images) are
independent of each other and can swap or parallelize. Phase 5 should land before
Phase 6 for performance reasons, but does not block it functionally — if the target
hardware is fast enough, Phase 6 can be attempted early as a spike to de-risk the
display path.
