#ifndef MU_BACKEND_H
#define MU_BACKEND_H

/*
 * Backend selection.
 *
 * Every backend header defines `struct MuRenderContext` with its own layout, so
 * at most one may be visible in any translation unit. Getting this wrong is
 * silent: a TU compiled against one backend's layout but linked against another
 * writes every field at the wrong offset, with no diagnostic.
 *
 * Selection is automatic. Each backend library (markup_soft / markup_skia /
 * markup_raylib) declares its MU_BACKEND_* macro as a PUBLIC compile definition,
 * so linking the library is what picks the header. Apps include this header (or
 * mu.h) and configure nothing.
 *
 * markup_soft is the supported backend. markup_raylib is deprecated and off by
 * default (see mu_raylib.h); markup_skia renders everything but backdrop blur.
 *
 * Including a backend header directly still works and is equally safe — each one
 * asserts that no other backend has already been selected.
 */

#if (defined(MU_BACKEND_RAYLIB) + defined(MU_BACKEND_SKIA) + defined(MU_BACKEND_SOFT)) > 1
#error "Markup: more than one MU_BACKEND_* is defined. Each backend declares an incompatible struct MuRenderContext; link exactly one backend library."
#endif

#if (defined(MU_BACKEND_RAYLIB) + defined(MU_BACKEND_SKIA) + defined(MU_BACKEND_SOFT)) == 0
#error "Markup: no rendering backend selected. Link markup_soft, markup_skia, or markup_raylib (deprecated) — each defines MU_BACKEND_* publicly — or define one of MU_BACKEND_SOFT / MU_BACKEND_SKIA / MU_BACKEND_RAYLIB before including this header."
#endif

#if defined(MU_BACKEND_RAYLIB)
#include "mu_raylib.h"
#elif defined(MU_BACKEND_SKIA)
#include "mu_skia.h"
#elif defined(MU_BACKEND_SOFT)
#include "mu_soft.h"
#endif

#endif
