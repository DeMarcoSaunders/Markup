# MarkUp UI v2

**MarkUp v2** is a small UI layer for **C11 + [Raylib](https://www.raylib.com/)**. You describe a tree of **`MuNode` widgets**, run a **flexbox-style layout**, feed **pointer / keyboard** events from Raylib (or your own backend), and **paint** via a thin Renderer API. Split into static CMake libraries so you can link only what you need.

Use it when you want a **native Raylib HUD**—settings panels, overlays, sliders, dialogs—without pulling in HTML or a heavyweight toolkit. Version 2 is a **focused rewrite**: fewer bundled controls than older experiments, but a cleaner extension model (**`MuNode` + `MuNodeOps`**). See **`apps/demo_minimal/main.c`** for a full working UI.

---

## Using Markup in your own project

You always need Raylib initialized before `mu_render_init` (see demos). Beyond that there are three common ways to depend on Markup under CMake.

### 1. Add Markup inside your repo (`add_subdirectory`)

Copy or submodule this repo (for example under `third_party/markup`). From your root `CMakeLists.txt`:

```cmake
add_subdirectory(third_party/markup) # path to upstream CMakeLists.txt

add_executable(mygame main.c ...)
target_include_directories(mygame PRIVATE third_party/markup/apps) # optional: only if you share demo_font helpers
target_link_libraries(mygame PRIVATE
    markup::widgets_basic
    markup::layout_flex
    markup::style
    markup::raylib
    markup::input
    markup::core
    raylib
)
```

Markup’s CMake sets **include directories automatically** through those targets—the public headers live under **`markup/include/`** and you **`#include "markup/mu_core.h"`** (etc.) after linking.

Your `main.c` does **not** need this repo’s `apps/` helpers unless you want them; demos use `demo_font.c` only to load a bundled TTF. You may use **`GetFontDefault()`** (`mu_render_init` already does until you substitute a font).

**Raylib duplication:** Markup defaults to **`MARKUP_FETCH_RAYLIB=ON`** (it uses `FetchContent` for Raylib 5.x). If *your* project already fetches or `find_package`s Raylib, turn Markup’s fetch off so you only resolve Raylib once:

```cmake
set(MARKUP_FETCH_RAYLIB OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/markup)
```

Ensure `raylib` is a CMake target/`find_package`-visible before configuring Markup in that setup.

---

### 2. Pull Markup from Git (`FetchContent`)

```cmake
include(FetchContent)
FetchContent_Declare(markup_upstream
    GIT_REPOSITORY https://github.com/<you>/Markup.git # replace URL
    GIT_TAG        main                                   # pin a tag/sha when shipping
)
FetchContent_MakeAvailable(markup_upstream)

add_executable(mygame main.c)
target_link_libraries(mygame PRIVATE
    markup::widgets_basic markup::layout_flex markup::style markup::raylib markup::input markup::core raylib)
```

Treat **`MARKUP_FETCH_RAYLIB` the same as above** if your repo already pulls Raylib.

---

### 3. Install Markup (`find_package`) then depend on `/prefix`

Build Markup separately and install headers + static libs onto a prefix (see [Build](#build)):

```bash
cmake -S path/to/markup -B build_markup -DCMAKE_BUILD_TYPE=Release
cmake --install build_markup --prefix /path/to/markup_install
```

In your consumer project:

```cmake
cmake_minimum_required(VERSION 3.16)
find_package(Markup CONFIG REQUIRED PATHS /path/to/markup_install)
find_package(raylib REQUIRED) # markup::imports expect raylib to exist

add_executable(mygame main.c)
target_link_libraries(mygame PRIVATE
    markup::widgets_basic markup::layout_flex markup::style markup::raylib markup::input markup::core raylib)
```

Exported targets are **`markup::core`**, **`markup::layout_flex`**, **`markup::style`**, **`markup::input`**, **`markup::raylib`**, **`markup::widgets_basic`**. CMake config also wires **`MarkupImportedRaylib.cmake`** so installed **`markup::raylib`** / **`markup::widgets_basic`** find Raylib transitively (`find_dependency`).

---

### Minimal application shape (conceptual)

Roughly:

1. Create **`MuContext`**, optional **`MuStyleModule`**, **`MuRenderContext`**.
2. Call **`mu_widgets_basic_register`** (or only register kinds you bundle yourself).
3. Build once (or mutate) a **`panel`/`label`/`button`** tree; **`mu_context_set_root`**; optional **`mu_modal_bind_layer`**.
4. Each frame (order matters):

   **`mu_frame_begin`** → resize root **`bounds`** to the window → **`mu_layout_run`** → **`mu_raylib_frame`** → paint background → **`mu_paint_all`** → **`mu_frame_end`**.

Concrete code mirrors this in **`apps/demo_minimal/main.c`**.

See [Getting started](#getting-started) for a copy-pasteable loop snippet.

---

### Runtime: fonts & assets

- Markup paints text with **`MuRenderContext::font`** (Raylib **`Font`**). Default is **`GetFontDefault()`** unless you **`mu_render_set_font`** (demo uses Inter from **`apps/demo_minimal/assets/`**).
- If you load fonts or images from paths, distribute those files **next to your executable** (or fix working directory). CMake examples in this repo **`POST_BUILD` copy** **`assets`** into `bin/` for demos.

---

### Without CMake

Treat each **`markup/src/*.c`** as normal C sources, **`markup/include`** as **`-I`**, link **Raylib** and your platform window/OpenGL libs. Link order should mirror CMake (core → layout/style/input → raylib bridge → widgets). This is workable but brittle; CMake is how the upstream project is exercised.

---

## What’s in the box (v2)

Bundled **`mu_widgets_basic`**: **`panel`** (flex row/column container), **`label`**, **`button`**, **`slider`**, **`checkbox`**, **`dropdown`**, **`textinput`**, **`modal`**, **`toast`**.

**Demos**

- **`demo_minimal`** — layout + widgets + modal + toast.
- **`demo_retheme`** — two baked themes; swaps **`MuStyleModule`** without restructuring the UI tree.

Optional CMake stubs (placeholders today): **`MARKUP_WITH_BLUR`**, **`MARKUP_WITH_STYLE_FILE`**, **`MARKUP_WITH_EXTRA_WIDGETS`**.

---

## Getting started

1. **Build** the repo ([Build](#build)), then run **`demo_minimal`** from **the executable’s folder** so copied **`assets/`** is available.
2. **Read** **`apps/demo_minimal/main.c`** end-to-end; it mirrors the checklist above (Raylib → context → widgets → modal → frame loop).
3. Initialise Raylib (**`InitWindow`**, …) **before** **`mu_render_init`** / loading a bundled font pattern from the demos.

Minimal frame loop excerpt:

```c
MuContext ctx;
mu_context_init(&ctx, 65536);
mu_style_init(&ctx, NULL); /* or &your_theme — see markup/mu_style.h */

MuRenderContext rc;
mu_render_init(&rc);

mu_widgets_basic_register(&ctx);
mu_context_set_root(&ctx, root);          /* MuNode tree you built */
mu_modal_bind_layer(&ctx, modal_root);    /* optional */

while (!WindowShouldClose()) {
    mu_frame_begin(&ctx);
    root->bounds = (MuRect){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()};

    mu_layout_run(&ctx);
    mu_raylib_frame(&ctx, &rc);   /* pushes Raylib input into markup */

    BeginDrawing();
    ClearBackground(/* from ctx.style or RAYWHITE */);
    mu_paint_all(&ctx, &rc);
    EndDrawing();

    mu_frame_end(&ctx);
}

mu_render_shutdown(&rc);
mu_style_shutdown(&ctx);
mu_context_shutdown(&ctx);
CloseWindow();
```

**Headers**: include granular headers (`markup/mu_core.h`, `markup/mu_widgets_basic.h`, …) or the umbrella **`markup/mu.h`** for everything.

---

## Architecture

| Library | Responsibility |
|---------|----------------|
| `markup::core` | `MuContext`, `MuNode` tree, node kind registry, arenas, modal stack hooks |
| `markup::layout_flex` | `mu_layout_run` — assigns `MuNode.bounds` for flex containers and children |
| `markup::style` | `MuStyleModule` tokens + `mu_style_resolve` from `MuNode.role` |
| `markup::input` | Hover/focus/tab order, modal routing, pointer/key/text dispatch |
| `markup::raylib` | `MuRenderContext`, `mu_draw_*`, `mu_paint_all`, `mu_raylib_frame` |
| `markup::widgets_basic` | Built-in kinds (`panel`, `label`, `button`, …) and **`mu_make_*`** factories |

Each visible node uses a **`uint32_t` kind id** wired to **`MuNodeOps`** (measure / layout / paint / hit-test / input). Flex containers (**`MU_NODE_FLEX_CONTAINER`**) use **`mu_layout_flex`**.

---

## Adding new UI elements

Extend Markup by **registering a node kind** and **creating nodes** with **`mu_node_create`**. Patterns live in **`markup/src/mu_widgets_basic.c`**.

1. **Where to put code**: either **`mu_widgets_extra`** (optional stub target **`MARKUP_WITH_EXTRA_WIDGETS`**) / fork **`mu_widgets_basic`**, **or register from your app** with **`mu_register_node_kind`** — no upstream patch required.

2. **Implement `MuNodeOps`**: `measure`, optional `layout_children`, `paint`, optionally `hit_test` / `on_pointer` / `on_key` / `on_char`, and `destroy_state` for **`node->state`**.

```c
uint32_t my_kind = mu_register_node_kind(ctx, "my_progress", &my_progress_ops);
MyProgressState *s = calloc(1, sizeof *s);
MuNode *n = mu_node_create(ctx, my_kind, s);
n->role = "panel"; /* or widen mu_style_resolve in mu_style.c for a new token */
mu_node_add_child(ctx, parent, n);
```

3. **Theming**: `mu_style_resolve` keys off **`node->role`** (see **`markup/src/mu_style.c`**) — reuse a role (**`button`**, **`input`**, …) or add **`MuStyleModule`** fields plus new branches.

4. **Flex / visibility**: panels from **`mu_make_panel`**, **`flex_grow` / `flex_basis`**, **`MU_NODE_VISIBLE`**, **`mu_modal_set_visible`**, etc., as in the demos.

---

## Build

Requirements: CMake **3.16+**, **C11** compiler, Git (for **`FetchContent` Raylib**, default **ON**).

### MSVC (Windows)

Use **Developer PowerShell for Visual Studio** (or **x64 Native Tools**), not Git Bash/BusyBox paths that break generators. Prefer Kitware CMake: **`cmake --version`**, **`where cmake`**.

**Visual Studio 18 2026** needs **CMake 4.2+**:

```powershell
cd D:\Markup
cmake --preset msvc-x64
cmake --build build --config Release
```

Without presets:

```powershell
cmake -G "Visual Studio 18 2026" -A x64 -S . -B build -DMARKUP_FETCH_RAYLIB=ON
cmake --build build --config Release
```

Use matching generators for older VS (e.g. **Visual Studio 17 2022**).

**Troubleshooting**

1. Prefer **`C:\Program Files\CMake\bin\cmake.exe`** over Git-for-Windows **cmake**.
2. If **VS 2026** generator is missing → install **CMake 4.2+**.
3. Try **`cmake --preset msvc-x64-vs2022`** if **`msvc-x64`** cannot find MSVC.
4. Multiple VS installs: **`CMAKE_GENERATOR_INSTANCE`** to force one tree; locate with **`vswhere`**.

### Generic (Ninja / Makefile)

```bash
cmake -S . -B build -DMARKUP_FETCH_RAYLIB=ON
cmake --build build --config Release
```

CMake options:

- **`MARKUP_FETCH_RAYLIB`** (default ON) — Raylib 5.0 via FetchContent. OFF → **`find_package(raylib REQUIRED)`**.
- **`MARKUP_WITH_BLUR`**, **`MARKUP_WITH_STYLE_FILE`**, **`MARKUP_WITH_EXTRA_WIDGETS`** — optional stub libs.

Install (local prefix):

```bash
cmake --install build --prefix dist
```

---

## Relation to legacy

The older monolithic tree is only relevant if you keep a local **`V1 legacy/`** copy—v2’s CMake builds only **`markup/`** and **`apps/`**. That folder is **`gitignored`** so clones stay small; retain it privately if you still want to diff against history.
