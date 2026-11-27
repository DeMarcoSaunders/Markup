# Four focused C examples

These use the software renderer and a bundled font; no window system is required.
From the repository root, configure and build all four with one shell command:

```powershell
cmake -S . -B build-examples -DMARKUP_WITH_RAYLIB=OFF -DMARKUP_BUILD_TESTS=OFF -DMARKUP_BUILD_EXAMPLES=ON && cmake --build build-examples --config Release --target markup_examples
```

For Windows PowerShell 5, run the configure and build commands on separate lines
(`&&` requires PowerShell 7). On single-configuration generators such as Ninja add
`-DCMAKE_BUILD_TYPE=Release`; executables are directly under the build directory.

| Executable | Demonstrates |
| --- | --- |
| `example_card` | Parse a structural attribute, build a nested card, bind a button with caller-owned state |
| `example_form` | Input change notifications, caller-owned form state, a checked handle to another widget |
| `example_scroll_list` | Copied stack strings, fixed-height rows, clipped natural-size scroll content |
| `example_theme` | Shared theme replacement, local radius/border/placeholder overrides, shadow and ellipsis |

Render an example without opening a window:

```powershell
.\build-examples\Release\example_form.exe --snapshot form.ppm
```

The output is a portable P6 RGB image. All examples return nonzero for initialization,
layout, or file errors. Builds with testing enabled also render each example through
CTest to the build directory's `example-snapshots` folder.

For interactive buttons, typing, and scrolling, configure with
`-DMARKUP_WITH_SDL=ON`, rebuild `markup_examples`, and run an example with `--window`.
That opt-in build needs SDL3 (fetched by the existing project configuration, or supplied
with `-DMARKUP_FETCH_SDL3=OFF`). The four example source files share only the small
font/frame/snapshot/window host; their widget and event code stays in each example.

Retained node/child storage uses a 128 KiB arena in these examples. Callback state lives
on `main`'s stack until the example exits its event loop and destroys the context. See
[FOUNDATIONS.md](../FOUNDATIONS.md) for ownership, failure, and invalidation rules.
