# Physics

Two libraries, deliberately separate.

**`markup::physics`** is a 2D rigid body simulation that knows nothing about the UI. It
borrows `MuVec2` and `MuRect` for interop and nothing else, so it can be built, stepped,
tested and debugged with no context, no renderer and no window.

**`markup::physics_node`** is the only file that joins it to the node tree: bodies drive
node bounds, and everything downstream — damage tracking, clipping, painting — carries on
as if a layout pass had moved them.

Use the first on its own if you want a simulation. Use both if you want it on screen.

- [Quick start](#quick-start)
- [The simulation](#the-simulation)
- [Binding bodies to nodes](#binding-bodies-to-nodes)
- [Stepping](#stepping)
- [Lifetime](#lifetime)
- [Limitations](#limitations)

---

## Quick start

```c
#include "markup/mu_physics_node.h"

mu_physics_register(&ctx);

MuNode *world = mu_make_physics_world(&ctx, NULL);   /* NULL = default config */
mu_node_add_child(&ctx, parent, world);
mu_node_set_bounds(world, (MuRect){20.f, 60.f, 600.f, 400.f});

MuPhysWorld *w = mu_physics_world(world);

/* A floor, in coordinates local to the container. */
mu_phys_add_box(w, MU_BODY_STATIC, (MuVec2){300.f, 395.f}, 300.f, 5.f, 0.f);

/* A ball, bound to a node that will follow it. */
MuNode *ball = my_make_circle_node(&ctx);
MuBodyId body = mu_phys_add_circle(w, MU_BODY_DYNAMIC, (MuVec2){300.f, 40.f}, 12.f, 1.f);
mu_physics_attach(&ctx, world, ball, body);
```

Then once per frame, after layout and before damage collection:

```c
mu_physics_advance(world, delta_seconds);
```

That is the whole integration. **Nothing needs marking dirty** — `mu_damage_collect`
discovers the moved bounds by comparing against the previous frame, so falling bodies
repaint correctly and a settled pile goes quiet on its own.

Working example: **`apps/demo_physics/main.c`** — a container of balls, click to drop,
with walls rebuilt on resize.

---

## The simulation

The approach follows the sequential-impulse method Erin Catto documented for Box2D
(GDC 2005–2014): discrete steps, an SAT narrowphase producing a one or two point
manifold, and accumulated impulses persisted across steps for warm starting. **No Box2D
code is used** — it was a reference, not a source.

Bodies are `MU_BODY_STATIC` (never moves, infinite mass), `MU_BODY_DYNAMIC` (driven by
forces and collisions) or `MU_BODY_KINEMATIC` (moved by velocity, unaffected by
collisions). Shapes are circles, boxes, and convex polygons up to `MU_POLY_MAX_VERTS`.

Bodies are referred to by `MuBodyId`, never by pointer: the body array grows, and ids stay
valid across that. Ids carry a generation counter, so a stale id after removal is rejected
rather than silently addressing whatever now occupies the slot.

```c
MuPhysConfig cfg;
mu_phys_config_init(&cfg);       /* tuned for pixels, gravity ~900 */
cfg.velocity_iterations = 8;     /* more is stiffer; 8 handles ordinary stacks */
cfg.slop = 0.5f;                 /* allowed overlap, stops jitter at rest */
MuNode *world = mu_make_physics_world(&ctx, &cfg);
```

`mu_phys_set_material` sets restitution and friction per body; `mu_phys_set_damping` bleeds
off linear and angular velocity. `mu_phys_apply_force` and `mu_phys_apply_impulse` do what
they say. Full API in `markup/include/markup/mu_physics.h`.

---

## Binding bodies to nodes

A body may drive one node. `mu_physics_attach` adds the node to the world node if it is not
already a child, and records the binding.

**Bodies live in coordinates local to the world node's rect.** Laying the container out
somewhere else moves the whole simulation with it, which is what you want when flex owns
the container's position. `mu_physics_to_local` converts a point in node space — a click,
say — into that space.

**Nodes paint themselves.** Physics only moves them, so a body can drive any node kind: a
panel, an image, or a custom kind of your own. The demo registers a small `phys_shape`
kind that draws a filled rounded rect, with the radius as a fraction of its width, so the
same kind renders both the balls and the walls.

### Layout must not fight physics

The world node has no `layout_children` op and is not a flex container, so the layout pass
leaves its children alone — their bounds belong to the simulation. The world node itself
is laid out normally by whatever contains it.

Watch for this when placing the container: **`mu_make_panel` always produces a flex
container**, so a world node inside a panel gets its bounds assigned by flex every frame.
That is fine and often what you want. If you need to position the container yourself, put
it on the modal layer, which `mu_layout_run` never walks:

```c
MuNode *layer = mu_make_panel(&ctx, true);
layer->role = "group";
mu_modal_bind_layer(&ctx, layer);
mu_node_add_child(&ctx, layer, world);
mu_node_set_bounds(world, region);     /* now yours to set */
```

### Clicks

The world node reports hits only once a callback is set, so a decorative simulation does
not swallow pointer events meant for what is behind it.

```c
static void on_click(void *user, MuVec2 local) { drop_a_ball(user, local); }
mu_physics_set_on_click(world, on_click, &my_state);
```

The position arrives already converted to world-local coordinates, which is what you need
to place a body there.

---

## Stepping

```c
void mu_physics_advance(MuNode *world, float elapsed);
```

Pass real elapsed seconds. Stepping happens at a **fixed internal rate** regardless of how
ragged that is — a variable step changes the effective stiffness of the solver frame to
frame, which makes stacks visibly breathe and makes results irreproducible. Time left over
is carried to the next call rather than stretched to fit.

A very long `elapsed` — a breakpoint, a stalled frame — is **clamped rather than simulated
in full**. Catching up on a lost second costs more than the frame that lost it, and
running further behind each time is how a simulation spirals.

Defaults are 120 Hz with a 0.25 s ceiling, so a 60 Hz frame costs two steps and a stall
costs at most thirty. `mu_physics_set_timing` changes both. `mu_physics_last_steps`
reports what the last call actually ran, which is 0 when it only banked time.

Call it **after layout and before `mu_damage_collect`**: the container's bounds must be
final before bodies are placed relative to them, and the new bounds must be in place
before damage is compared.

---

## Lifetime

Destroying the world node destroys the simulation with it, and that is always safe.

Removing one body needs care, because the binding holds a raw `MuNode` pointer:

```c
mu_physics_remove_body(world, body);      /* unbind first */
mu_node_remove_child(&ctx, world, node);
mu_node_destroy_recursive(&ctx, node);    /* then free the node */
```

Destroying a node that still has a body attached leaves the world writing into freed
memory on its next advance. `mu_physics_remove_body` clears the binding before removing
the body, so the order above is the safe one.

---

## Limitations

**Rotation is not applied to nodes.** `MuNode` has axis-aligned bounds and no angle, so a
body's node covers its AABB. That is exact for circles and for boxes that stay upright,
and visibly wrong for a box at 45 degrees — the node grows and shrinks as it tumbles. Use
circles, or write a paint op that reads the angle from `mu_phys_get_transform` and draws
the real shape.

**Shapes cannot be resized.** A container that changes size needs its static walls removed
and re-added; `rebuild_walls` in the demo shows the pattern.

**No sleeping.** Bodies at rest still cost solver time every step. Damage tracking means a
settled pile costs nothing to *draw*, but it is still being simulated.

**No joints, no continuous collision.** Fast small bodies can tunnel through thin static
geometry. Make walls thicker than the fastest body travels in one step.

**Broadphase is O(n²).** Fine for the low hundreds of bodies; a spatial hash is the
obvious next step if you need more.

---

## Where to look

| | |
|---|---|
| `apps/demo_physics/main.c` | Container of balls, click to drop, walls rebuilt on resize |
| `tests/test_physics.c` | The solver: stacking, restitution, friction, warm starting |
| `tests/test_physics_node.c` | The binding: bounds sync, damage discovery, fixed stepping |
| `markup/include/markup/mu_physics.h` | Simulation API |
| `markup/include/markup/mu_physics_node.h` | Node binding API |
