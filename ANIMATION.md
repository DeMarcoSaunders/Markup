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
same way it discovers a layout pass moving something.

- [Quick start](#quick-start)
- [The spring](#the-spring)
- [Animating node bounds](#animating-node-bounds)
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

### Animating a node layout still owns

The modal/popup layer is the safe path because nothing else ever touches those bounds. A
node layout genuinely owns — an ordinary flex child, like a button — can still be animated,
but not by reading `node->bounds` as "the resting position" each frame: layout in this
codebase is dirty-flag-gated (`mu_layout_node` bails out unless `MU_NODE_LAYOUT_DIRTY` is
set), and `mu_anim_advance` writes bounds directly rather than through `mu_node_set_bounds`,
so nothing re-dirties the node either. Once it has animated even once, `node->bounds` is
wherever the spring last left it, not where flex would put it at rest — read it back as the
base for a new target and each retarget lifts (or drops) it a little further than the last,
forever, because the "resting" reference it launches from has already moved. It looks
exactly like an animation that never finishes, because it is one.

Capture the resting rect once, before the node has ever animated, and compute every target
from that stable copy — never from live `node->bounds` once animation may have touched it.
`apps/demo_soft/main.c`'s hover-lift on the "Primary" button does exactly this: the resting
rect is captured right after the first real layout pass (and again after any resize), and
the per-frame target is always `rest`, or `rest` shifted up a few pixels while hovered —
never a re-read of the node's current, possibly-mid-flight bounds.

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
tied to `MuRect`. Scroll rubber-banding in `apps/demo_soft/main.c` is a real instance: one
`MuSpring` models a visual y-offset layered on top of a scroll view's already-clamped
position, kicked whenever `mu_scroll_get_excess` reports that a wheel or thumb-drag tried
to go further than the content allows. `markup_widgets_basic` has no idea this is
happening — `mu_scroll_get_excess` is ordinary bookkeeping the clamp already had to do,
not an animation hook, which is what lets a widget library stay free of a hard dependency
on `markup_anim` while still supporting this from the outside. Same reasoning as the hover
lift: the effect is real, but the widget that makes it possible doesn't need to know what
`mu_anim` is.

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
| `apps/demo_soft/main.c` | A card that springs to a new corner on click (modal layer), a button that lifts on hover (an ordinary flex child), and scroll rubber-banding at the content's limits |
| `tests/test_anim.c` | The solver's three regimes, retargeting, id-safety, and two ablations |
| `markup/include/markup/mu_anim.h` | Full API |
