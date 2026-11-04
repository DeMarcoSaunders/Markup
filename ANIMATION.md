# Animation

A spring that converges a value on a target, and a small animator that applies one to each
edge of a node's bounds. Built for interruptible UI motion — a sheet sliding in, a scroll
rubber-banding at its limit, a list closing a gap after a row is removed — where the
defining requirement is that re-aiming mid-flight must not cause a visible jump.

Two pieces, layered the way physics is:

**`MuSpring`** is one damped harmonic oscillator over a single float. It knows nothing about
nodes and can drive any value you like.

**`MuAnimator`** holds four springs per node — one per edge of its bounds — and writes the
result into `node->bounds` each frame, after which damage tracking discovers the move the
same way it discovers a layout pass moving something. It is for nodes whose position is
yours: a card on the modal layer, a sheet, a popup.

**Widget motion** is the third piece, for nodes layout owns: a node springs its own
`paint_offset` from an `animate` op, which moves where it is *drawn* and nothing else.
Buttons lift on hover and scroll views glide and rubber-band this way, built in and opt-in.

`MuSpring` lives in `markup_core` (`markup/mu_spring.h`), so widgets can use it without
linking `markup_anim`; `mu_anim.h` includes it, so nothing changes for animator users.

- [Quick start](#quick-start)
- [The spring](#the-spring)
- [Animating node bounds](#animating-node-bounds)
- [Widget motion](#widget-motion)
- [Stepping](#stepping)
- [Lifetime](#lifetime)
- [Limitations](#limitations)
- [Where to look](#where-to-look)

---

## Quick start

```c
#include "markup/mu_anim.h"

MuAnimator *anim = mu_anim_create();

/* The node already exists, somewhere layout will not fight the animator for control
 * of its bounds — see "Animating node bounds" below. */
mu_anim_to(anim, card, (MuRect){400.f, 200.f, 160.f, 100.f},
           MU_SPRING_RESPONSE, MU_SPRING_BOUNCY);
```

Then once per frame, after layout and before damage collection:

```c
mu_anim_advance(&ctx, anim, delta_seconds);
```

That is the whole integration — nothing needs marking dirty, same as physics.
`mu_damage_collect` compares the node's new bounds against last frame's, so an animating
card repaints correctly and a settled one goes quiet on its own.

Working example: **`apps/demo_soft/main.c`** — a card that springs to a new corner on
click.

---

## The spring

```c
void mu_spring_init(MuSpring *s, float value, float response, float damping);
```

`response` is roughly how long the motion takes, in seconds. `damping` is a ratio:

| | |
|---|---|
| `MU_SPRING_BOUNCY` (0.55) | Underdamped — overshoots, settles back |
| `MU_SPRING_SNAPPY` (0.85) | Underdamped, barely — the faintest overshoot |
| `MU_SPRING_STIFF` (1.0) | Critically damped — the fastest approach that never crosses the target |
| above 1.0 | Overdamped — crawls in, slower the higher it goes |

`MU_SPRING_RESPONSE` (0.32s) is a reasonable default for UI motion.

### Why closed form

The spring is solved analytically each step rather than integrated. There are three
regimes, by damping ratio — underdamped oscillates, overdamped is two decaying
exponentials, and critical damping is not just the boundary between them but a genuinely
different formula, since the two roots coincide and the general solution picks up a factor
of `t`.

The payoff: there is no step size at which this diverges, no energy bleeding away at small
steps the way a stepped integrator loses it, and no difference between the motion at 60 Hz
and at 144 Hz — a frame that stalled for a whole second lands exactly where a second of
motion belongs instead of overshooting into nonsense. `tests/test_anim.c` ablates this
directly: swapping the closed form for semi-implicit Euler and stepping at `dt=10` sends the
value to 3,855,314 against a target of 100, and running the same 0.2 seconds of motion as
12 steps at 60 Hz versus 48 at 240 Hz lands at 162.8 versus 158.8 — a difference the closed
form does not have.

### Retargeting and finishing

```c
void mu_spring_set_target(MuSpring *s, float target);   /* keeps velocity */
void mu_spring_snap(MuSpring *s, float value);           /* jumps, kills velocity */
bool mu_spring_step(MuSpring *s, float dt);              /* true while moving */
bool mu_spring_at_rest(const MuSpring *s);
```

Re-aiming with `mu_spring_set_target` does not disturb the current value or velocity — that
is what makes an interruptible interface feel attached to the pointer rather than to a
clock. A spring retargeted mid-flight bends toward the new target from wherever it already
was, at whatever speed it already had.

A spring **finishes**. Once within `eps_value`/`eps_velocity` of the target — pixel-scale
by default (0.01 / 0.05) — `mu_spring_step` snaps the value exactly onto the target, zeroes
the velocity, and returns `false`. Animating a quantity on a different scale (an opacity in
0..1, an angle in radians) needs `mu_spring_set_rest_threshold`, or it will read as finished
before it visibly arrives.

`mu_spring_step` checks for rest twice: once at the top, before computing the step, and
once at the bottom, right after. Ablation testing (removing each independently) shows the
two are not equally important. Dropping the leading check stops springs from settling at
all — 6 test failures, animator entries that never retire, which would keep damage tracking
repainting forever. Dropping the trailing check changes nothing except that an animation
which arrives exactly on this step reports finished one frame later than it could have,
caught by the leading check on the next call instead. The leading check is load-bearing;
the trailing one is a same-frame convenience.

---

## Animating node bounds

```c
bool mu_anim_to(MuAnimator *a, MuNode *node, MuRect target, float response, float damping);
void mu_anim_set(MuAnimator *a, MuNode *node, MuRect bounds);
void mu_anim_cancel(MuAnimator *a, const MuNode *node);
bool mu_anim_active(const MuAnimator *a, const MuNode *node);
int mu_anim_count(const MuAnimator *a);
```

`mu_anim_to` springs all four edges of `node->bounds` toward `target`. The first call seeds
each spring from the node's *current* bounds; every call after that re-aims the existing
springs in place, keeping their velocity, so a target that moves every frame — following a
drag, say — is tracked smoothly instead of restarted each time. `response`/`damping` are
reapplied on every call too, so reshaping an in-flight animation's character is just calling
`mu_anim_to` again with the same target and different numbers. It returns `false` if `node`
has no id or the animator failed to grow; a UI can typically ignore this and simply see the
node not move.

`mu_anim_set` jumps immediately and cancels whatever was running. `mu_anim_cancel` stops in
place, leaving the node wherever it had reached.

A node holds at most one entry per `MuAnimator` — calling `mu_anim_to` again on a node
already animating reshapes that entry rather than adding a second.

### Layout must not fight the animator

**Bounds written by `mu_anim_advance` are overwritten by any layout pass that owns the
node.** Animate nodes whose position is yours to set: children of a non-flex parent, or the
popup and modal layers, which `mu_layout_run` never walks — the same constraint
**[PHYSICS.md](PHYSICS.md)** documents under "Layout must not fight physics", for the same
reason. `apps/demo_soft/main.c` puts its card on the modal layer alongside the frosted-glass
panel precisely so nothing else claims its bounds.

### Don't use it on nodes layout owns

An ordinary flex child — a button, a list row — should not be animated through its bounds
at all; use [widget motion](#widget-motion). Two separate things go wrong:

- **Hit testing follows bounds.** A button lifted by moving its bounds rises out from under
  a pointer resting near its bottom edge, loses hover, falls back under the pointer, is
  hovered again, and repeats. This is not hypothetical: `tests/test_anim.c` ablates it, and
  with a bounds-based lift the hover drops on 88 of 120 frames and never settles.
- **Bounds stop meaning "rest".** Layout is dirty-flag-gated, so once a node has animated,
  `node->bounds` is wherever the spring left it. Reading it back as the base for the next
  target lifts it again every frame — a runaway that never finishes.

### Keyed by id, not by pointer

Entries are keyed by the node's **id**, not its address. `mu_anim_advance` resolves each
entry by id every frame; a node destroyed mid-animation simply stops resolving, and its
entry is dropped on the next call. There is nothing to unbind and no ordering requirement
between destroying a node and the animator finding out.

This is deliberately safer than the physics binding in `mu_physics_node.c`, which holds a
raw `MuNode *` and requires `mu_physics_remove_body` before the node is freed — skipping
that step leaves physics writing into freed memory on its next advance. Animation cannot
develop that bug; the cost is one linear id lookup per animated node per frame, which is the
trade being made.

---

## Widget motion

```c
void mu_frame_set_dt(MuContext *ctx, float seconds);   /* once per frame, before collect */
int  mu_animating(const MuContext *ctx);                 /* still moving after collect? */

mu_button_set_hover_lift(&ctx, button, 4.f);           /* opt-in, per button */
mu_scroll_set_spring(scroll, true);                    /* opt-in, per scroll view */
```

Every node has a `paint_offset`: where it and its subtree are **drawn**, relative to where
layout put them. Layout, hit testing and input never see it. Painting shifts each node by
its accumulated offset for the duration of its own paint and then restores the saved bounds
by assignment — not by subtracting the offset back out, which is not exact in floating point
and would drift a layout rect into permanent damage. Every widget and every backend already
draws from `node->bounds`, so none of them needed to change.

Damage tracking compares the *painted* rect (`prev_bounds` now records that), so a lifted
button damages where it was drawn and where it is drawn, not where layout says it is.

The motion itself comes from an optional node op:

```c
bool (*animate)(MuContext *ctx, MuNode *node, float dt);   /* in MuNodeOps */
```

`mu_damage_collect` calls it for each node **just before comparing that node** against last
frame, so whatever it changes is damaged in the same frame. Stepping during paint instead
(as a GPU toolkit that redraws everything can) would draw outside the region that was
cleared, because paint here only covers what collect already reported. The op returns true
while moving; `mu_animating` counts those, which is the signal to keep producing frames.
Call `mu_damage_collect` once per frame — a second call steps the motion twice.

With no clock — `mu_frame_set_dt` never called, or 0 — `mu_spring_follow` snaps to the
target instead of stepping. Motion lands at once rather than waiting forever, so an app that
never heard of any of this gets the end state and goes idle; it doubles as "reduce motion".

The built-in uses:

- **Button hover lift** springs `paint_offset.y` toward `-lift` while `MU_NODE_HOVERED`.
- **Spring scrolling.** The content is drawn at `shown`, a spring that chases the real
  offset; the content child's `paint_offset` is the difference. The real offset stays the
  clamped truth that layout and hit testing use, so a wheel step changes it at once and
  the content *glides* there. A push past an end that the clamp refused (see
  `mu_scroll_get_excess`) is added to the spring's **velocity**, not its position: the
  content travels out past the end over a few frames and comes back, with no jump at
  either end. Resistance grows with the stretch, so repeated pushes cannot pull it out
  without bound. The spring is critically damped, so a glide never wobbles past its line
  and an overrun goes out and back once. The scrollbar thumb follows `shown` and squeezes
  against an end it is pulled past. A dragged thumb is followed exactly, with no bounce.

  An earlier version moved the drawn position straight to the stretched point instead,
  which put the whole ~38px overrun on screen in the first frame — the visible jump this
  design exists to avoid. `test_scroll_rubber_band` checks the overrun now builds over
  several frames; putting the displacement back fails it.

  The tradeoff: a row clicked mid-glide is the row at the real offset, which can be a
  little away from where it is drawn for the few frames the glide lasts.

A custom widget does the same: keep a `MuSpring` in its state, step it with
`mu_spring_follow(&s->spring, target, dt)` in `animate`, and write `node->paint_offset`.

One limit of the shift-per-node approach: a paint op that reads its *children's* bounds sees
them at their layout positions, since each child is shifted only when the walk reaches it. No
built-in widget does this.

---

## Stepping

```c
int mu_anim_advance(MuContext *ctx, MuAnimator *a, float dt);
```

Call **after layout and before `mu_damage_collect`** — layout must not overwrite what this
just wrote, and the new bounds must be in place before damage compares them. Bounds are
assigned directly rather than through `mu_node_set_bounds`, which would mark the subtree
layout-dirty every frame for a layout pass that is never going to run; damage tracking finds
the move anyway by comparing bounds against last frame.

Unlike `mu_physics_advance`, there is no fixed internal rate and no clamp on `dt`. Physics
needs both because its collision resolution is not exact at arbitrary step sizes; a spring
is exact at any `dt`, so whatever a frame actually took is exactly the right amount to
advance it.

The return value is how many animations are still moving — 0 means every node has arrived,
which a caller can use as the signal to stop asking for frames.

---

## Lifetime

```c
MuAnimator *mu_anim_create(void);
void mu_anim_destroy(MuAnimator *a);
```

Destroying the animator frees only its own bookkeeping; it holds no node pointers, so the
order relative to destroying the context or its nodes does not matter. Destroying a node it
was animating is always safe, for the reasons above — there is nothing to do on either side.

---

## Limitations

**Bounds only.** `MuAnimator` springs a node's `x`/`y`/`w`/`h` as one unit. Animating
anything else — opacity, a color channel, a custom node's internal state — means using
`MuSpring` directly and writing the result wherever it needs to go; nothing about it is
tied to `MuRect`. [Widget motion](#widget-motion) is exactly that: hover lift and
rubber-banding are each a `MuSpring` driving a `paint_offset`.

**Overrun after a long glide is absorbed.** A single huge scroll from the top straight
past the bottom glides most of the way and the refused part of the push is swallowed by
the glide's own deceleration, so there is no bounce at the end. That is the physics of a
critically damped spring, and what you want: the bounce is for pushing at an end you are
already at.

**One shape per node.** All four edges share the `response`/`damping` passed to a given
`mu_anim_to` call, and a node holds at most one entry, so there is no way to make width
settle snappier than position within a single `MuAnimator`. Spring position and size at
different rates by running two separate animators over the same node, or by dropping to
`MuSpring` directly for the properties that need independent shaping.

**Linear lookup.** Every `mu_anim_to` and every `mu_anim_advance` entry resolves by
scanning the active animations. Fine for the tens of concurrently-animating nodes a UI
actually has; not a data structure to reach for if that number grows into the thousands.

---

## Where to look

| | |
|---|---|
| `apps/demo_soft/main.c` | A card that springs to a new corner on click (`MuAnimator`, modal layer); buttons that lift on hover and a scroll view that glides and rubber-bands (widget motion) |
| `tests/test_anim.c` | The solver's regimes and retargeting; the animator's id-safety; widget motion: a hit area that never moves, partial repaint pixel-identical to full repaint, the no-clock path, an overrun that travels instead of jumping, a glide that never overshoots |
| `markup/include/markup/mu_spring.h` | `MuSpring` (in `markup_core`) |
| `markup/include/markup/mu_anim.h` | `MuAnimator` |
