# MarkUp UI — Rebuild Specification

**Purpose:** Capture the product intent of the current codebase, document why the present implementation fights itself, and define a single cohesive architecture for a clean rewrite (suitable for plan-mode design).  
**Stack target:** C (C11 or later), [Raylib](https://www.raylib.com/) as the only mandatory backend, CMake + optional MSVC project generation.  
**North star:** A **fully customizable**, **lightweight** way to build UIs **quickly**, including on **resource-constrained** devices (e.g. Raspberry Pi class: limited RAM/CPU, single-threaded apps).  
**Non-goals:** Matching every CSS3 feature or browser layout engine; shipping non-Raylib render backends in core; accessibility at WCAG-audit level in v1.

---

## 1. Product vision

**MarkUp UI** is **declarative styling + layout + widgets for C**, built on Raylib — a **fast path from idea to interface** with **one mental model** for structure, presentation, and behavior.

**Design goals**

- **Separation of concerns:** Structure (tree / nodes), **presentation** (rules, inheritance, states, spacing, typography), and **behavior** (callbacks) are not tangled in the same struct fields.
- **Full customization:** Apps can restyle **everything** visible (metrics, colors borders, alignment) without forking the library — via tokens, named themes, rules, and per-node overrides, not hard-coded widget palettes only.
- **Quick to build:** Common screens (forms, settings panels, lists, simple dashboards) stay **short in C** because layout and look are data-driven or rule-driven instead of hand-placed rectangles everywhere.

**Platform stance: lightweight and embeddable**

- **Small dependency surface:** Raylib only for core; no web stack, no heavy parser stack required for minimal use.
- **Pi-friendly defaults:** Predictable per-frame cost, optional compile-time **feature flags** to drop blur/heavy effects, and documented memory strategy (arenas / fixed pools) so 512MB–1GB class devices remain viable targets.
- **Single-threaded first:** Matches Raylib and typical Pi apps; concurrent UI is explicitly out of scope.

**Concrete deliverables (same codebase, different emphasis)**

- Widget set for typical apps: buttons, toggles, inputs, sliders, dropdowns, panels, modals, toasts, progress, decorative chrome — all **styleable through the same pipeline**.
- **Layout** that feels like the web subset people actually use (flex stacks early; grid/selectors later as the engine matures).
- **Ergonomic C API** layered on top: predictable lifecycle, clear ownership, optional “bake rules once, run many frames” for static UIs.

The existing README points in this direction; the current gap is **implementation alignment** (parallel type systems, split styling paths), not the product idea.

---

## 2. Autopsy — what is wrong today (scrap drivers)

These are structural issues to **fix by design**, not patch incrementally.

### 2.1 Two parallel identity systems

- `theme.h` defines `ComponentType` (`COMPONENT_BUTTON` … `COMPONENT_COUNT`).
- `component_base.h` defines another `ComponentType` (`COMPONENT_TYPE_BUTTON` …) with a **different member set**.

**Symptom:** Including both headers risks confusion or subtle bugs; styles, themes, and the “base component” layer cannot be one truth.

### 2.2 `ComponentBase` is not the base

Concrete widgets (e.g. `Button`) **duplicate** rect, hover, pressed, focus, z-index, transform, visibility—**without** embedding or delegating to `ComponentBase`. The tree, callbacks, and lifecycle in `ComponentBase` are **orphaned** from real widgets.

**Symptom:** No unified hit-testing, focus traversal, or tree walk; every widget reinvents state flags (`is_clicked`, etc.).

### 2.3 `MarkupSystem` vs how the app actually runs

`markup.h` describes a central context (`MarkupSystem`) with input, focus, modals, and optional **global** `g_markup_system`.

**Reality:** Primary demos (`main.c`, `demo.c`) **never** call `Markup_Init` / `Markup_Update`; they call **Raylib** and per-widget `*_Update` / `*_Draw` manually.

**Symptom:** Documentation and architecture diagram do not match executable behavior; feature work lands in the wrong layer.

### 2.4 Layout is defined three ways

- `layout_engine.h`: flex/grid/absolute models and `LayoutContext`.
- `panel.c` / `sidebar.c`: **local** flex enums and algorithms.
- `panel.h` **redefines** `FlexDirection`, `JustifyContent`, `AlignItems` with names that **do not match** `layout_engine.h` (e.g. `JUSTIFY_START` vs `JUSTIFY_CONTENT_FLEX_START`).

**Symptom:** Impossible mental model for contributors; grid/flex “support” is fragmented.

### 2.5 Style system vs per-widget style fields

`style_system.h` promises cascade, specificity, and stylesheets. Widgets implement **ad hoc** `SetPadding`, pointer-overridden fields, and theme lookups in parallel.

**Symptom:** No single “computed style” path; CSS-like features are half-real.

### 2.6 Event system vs polling flags

`event_system.h` describes registration, bubbling, and queues. Widgets mostly expose **`is_clicked`** and similar after `*_Update`.

**Symptom:** Two interaction models; composing behaviors (e.g. drag, capture, global shortcuts) is unclear.

### 2.7 Type-erased children

`Panel` stores children as `void*` + `PanelChildType` / “custom” buckets (`Panel_AddCustom`).

**Symptom:** No compiler help; easy to desync Update/Draw/order; harder to add layout passes that need intrinsic sizes.

### 2.8 Demo / entrypoint fragmentation

Different init paths (`Theme_Init` vs `Theme_Init_Default`), commented “not implemented” imports, and mixed responsibilities (e.g. profile tab layout partly inside `Panel`, partly manual absolute rects for `TextInput`).

**Symptom:** The showcase is **not** a proof of the core architecture—it bypasses it.

---

## 3. Design principles for the rebuild

1. **One node model** — Every on-screen thing is a `UiNode` (or equivalent): identity, parent, children, bounds, z-order, flags, and a small **vtable** or tagged command table for measure/layout/paint/hit/input.
2. **One layout pass** — A single engine (even if internally simple) computes rectangles from a **small** constraint model (flex column/row + optional scroll in v1). No duplicate flex enums across headers.
3. **One style resolution path** — All visuals resolve through **the same cascade**: tokens → theme / rules → parental inheritance (where defined) → per-node overrides → computed **`UiStyle` snapshot** (per frame or invalidated on change). *Phasing is allowed* (e.g. tokens + struct-based rules first; optional text/stylesheet loader later), but **no** second parallel “each widget has its own bespoke `SetColor` path” as the only truth.
4. **One input model** — Either events **or** immediate queries, but **not both** at the public API. Recommended: internal event queue + public high-level “subscribe” API; eliminate scattered `is_clicked` as the primary contract if possible.
5. **Explicit context** — No mandatory global UI singleton; allow `UiContext*` everywhere. (Optional `Ui_DefaultContext` only if you truly need it.)
6. **Memory ownership** — Document who allocates/frees strings and child lists; prefer arena or pool per frame/window for transient strings if needed.
7. **Incremental delivery** — Ship a **usable core** (panel + button + input + slider) before blur, shadow, background presets, etc.

---

## 4. Functional requirements

### 4.1 Core context (`UiContext`)

- Owns: root node, theme, fonts, input snapshot, focus id, modal stack id, frame ids, scratch arenas (if used), debug flags.
- Lifecycle: `Ui_Init`, `Ui_Shutdown`, `Ui_BeginFrame` / `Ui_EndFrame` (or a single `Ui_Update` that consumes Raylib state for one frame).
- **Must** drive: layout invalidation, hit-testing order, focus changes, modal blocking of pointers.

### 4.2 Node tree

- Parent/child list with stable ordering; optional keyed children for tests.
- Hit-test from front to back using z-order or tree order + explicit overlay layer.
- Destroy subtree in deterministic order.

### 4.3 Layout (v1 scope)

**Minimum (enough to “build UIs quickly”):**

- Vertical and horizontal **stack** (flex main axis): gap, padding, child `flex_grow` / fixed basis, cross-axis align (start/center/end/stretch).
- Optional: scroll area (one axis) — high value on small screens (Pi + HDMI touchscreen).

**Grow toward richer declarative layout:**

- Percent / fractional sizes, min/max constraints, and **grid** as the engine hardens.

**Defer / lower priority for tiny devices:**

- Subpixel-perfect animation layout, full flex-wrap complexity, and exotic alignment modes can trail behind correct common cases.

### 4.4 Styling / theme (customization is the point)

- **Design tokens:** semantic palette (background, foreground, muted, border, accent, danger, …), **spacing scale**, radii, type ramps, focus ring — apps redefine tokens to **re-skin the entire UI** in one place.
- **Rules / “stylesheet” layer:** Named rules (by role, tag, class, id — exact selector grammar is an implementation choice) map to property sets; **specificity + inheritance** behave predictably and match developer expectations from CSS where feasible.
- **State styles:** default / hover / pressed / disabled / focused (and any widget-specific states) are **data-defined**, not only wire-up in C.
- **Resolution order (fixed in spec):** UA/library defaults → theme tokens → stylesheet rules (by specificity) → parent inheritance (document which properties inherit) → inline overrides on node.
- **Phasing (planned, not abandoned):** v1 may ship with **programmatic rule registration** (C structs or a tiny embedded DSL) before a file loader; the **architecture** must still be “one resolver, one snapshot,” so adding `.mss` / `.css`-subset files later does not require rewriting widgets.
- **Pi / embedded:** default build avoids large runtime parsers; optional stylesheet loader can be a separate translation unit `#ifdef`’d off for minimal binaries.

### 4.5 Input & focus

- Mouse: position, buttons, wheel within clip rects.
- Keyboard: focusable widgets, tab order (explicit or tree order), char input for text fields (Raylib `GetCharPressed` / codepoint strategy documented).
- **Modal:** top modal captures input; clicking outside behavior configurable.

### 4.6 Widgets (v1 set)

| Widget        | Behaviors |
|---------------|-----------|
| Panel/Box     | stack layout, padding, clip, background |
| Label         | text, ellipsis policy (simple) |
| Button        | click, keyboard activate |
| Checkbox      | toggle, mixed state optional later |
| Radio group   | mutual exclusion |
| Slider        | drag + keyboard nudge |
| Text input    | selection optional later; at least insert/delete and caret |
| Dropdown      | open/close, select option |
| Modal + overlay | dim, ESC to close optional |
| Toast         | queue, timed dismiss |

**Defer:** Card/Tabs as compositional patterns built from v1 primitives, unless trivial.

### 4.7 Rendering

- Use Raylib primitives and `DrawText`/custom font loading; document DPI/font scaling assumptions.
- Effects (blur, backdrop, heavy shadows) **behind a feature flag** or separate module so core stays simple.

### 4.8 Build & distribution

- CMake: `markup_ui` library target, `markup_ui_demo` executable, `find_package` config if exporting.
- CI: compile on Windows + Linux at least; optional macOS.
- No vendored Raylib in-repo unless policy changes; document version pin.

### 4.9 Documentation

- **Single** quickstart that matches **one** entry API (context-based).
- Generated or hand-written **widget table** with ownership rules for `char*`.

---

## 5. Non-functional requirements

- **Performance:** O(n) per frame over visible nodes for layout + draw + hit-test; avoid heap churn on the hot path (prefer scratch arenas or reuse buffers). Target **smooth 60 FPS** on mid-range Pi for representative demos at 720p when effects are trimmed.
- **Memory:** Document peak usage for the demo gallery; support **tuning** (max texture cache, max string cache, disabled effects) for 512MB-class systems.
- **Binary size:** Optional modules (blur, backdrop blur, rich backgrounds, file-based stylesheet parsing) must be **link-time or compile-time optional** so minimal apps stay small.
- **Customization without cost explosion:** Full theming must not require duplicating widget code — cost is **O(nodes + rules)**, not O(widget types × forks).
- **Threading:** Single-threaded only; UI runs on the Raylib main thread.
- **Stability:** Public API versioned; breaking changes bump major or use a clear namespacing prefix (`mu_`, `ui_`).

---

## 6. Migration / cutover strategy

1. **Rename or archive** this tree (e.g. `legacy/` or new branch) so mental debt is not mixed with the rewrite.
2. Implement **core + one demo** that uses only the new API (settings dialog or component gallery with **no** manual layout for stacked rows).
3. Port widgets **only** through the node/vtable path—no parallel `Button` struct that ignores the tree.
4. Reintroduce advanced visuals only after the gallery demo is maintainable in **<500 lines**.

---

## 7. Open decisions (resolve in plan mode)

- **vtable vs tagged union** for widget polymorphism in C.
- **Text model:** UTF-8 byte indices vs codepoint (recommendation: UTF-8 storage, careful iteration for caret).
- **Stylesheet surface:** start with C API + structs vs minimal external format; if external, define **grammar subset** and max parse cost for Pi.
- **ID type:** `uint32_t` vs string slug for focus and automation.
- **Error handling:** return `bool` + context `last_error` vs `Result` structs.
- **Raylib version** floor (e.g. 5.x vs 4.x) and **GLES** path notes if targeting Pi without X.

---

## 8. Success criteria

- One demo application uses **only** `UiContext` + node tree for structure; no duplicate flex enums; no second `ComponentType`.
- **A second demo or variant** shows **global re-theme** (token + rule swap) **without** rewriting screen logic.
- Adding a new widget requires: node kind, layout hints, **style property hooks**, and one implementation file pair — without panel “special case” lists growing without bound.
- README quickstart matches the compiled demo line-for-line and states **embedded/Pi** expectations honestly (flags, optional modules).

---

*End of rebuild spec.*
