#ifndef MU_PHYSICS_H
#define MU_PHYSICS_H

#include "mu_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 2D rigid body physics.
 *
 * Independent of the rest of Markup — it borrows MuVec2/MuRect for interop and nothing
 * else, so it can be used, tested and debugged without a render context. Integration
 * with the node tree lives elsewhere.
 *
 * The approach follows the sequential-impulse method Erin Catto documented for Box2D
 * (GDC talks, 2005-2014): discrete steps, SAT narrowphase producing a 1-2 point
 * manifold, accumulated impulses persisted across steps for warm starting, and Baumgarte
 * bias with a penetration slop for positional correction. No Box2D code is used.
 *
 * Bodies are referred to by MuBodyId, not pointers: the body array grows, and ids stay
 * valid across that. Ids carry a generation counter, so a stale id after removal is
 * rejected rather than silently addressing whatever now occupies the slot.
 */

typedef struct MuPhysWorld MuPhysWorld;

/** Opaque handle. 0 is never valid. */
typedef uint32_t MuBodyId;
#define MU_BODY_NONE 0u

typedef enum MuBodyType {
    MU_BODY_STATIC = 0, /* never moves, infinite mass */
    MU_BODY_DYNAMIC,    /* driven by forces and collisions */
    MU_BODY_KINEMATIC,  /* moved by velocity only; unaffected by collisions */
} MuBodyType;

/** Maximum vertices in a convex polygon shape. */
#define MU_POLY_MAX_VERTS 8

typedef struct MuPhysConfig {
    MuVec2 gravity;
    /* Velocity solver iterations. More is stiffer; 8 handles ordinary stacks. */
    int velocity_iterations;
    /* Allowed overlap, in world units. Prevents jitter from constantly correcting
     * contacts that are touching to within floating point noise. */
    float slop;
    /* Fraction of excess penetration corrected per step, 0..1. */
    float correction;
    /* Relative normal speed below which a collision is treated as resting rather than
     * bouncing, so restitution does not add energy to a settled stack. */
    float rest_velocity;
} MuPhysConfig;

/** Defaults tuned for a world measured in pixels with gravity around 900. */
void mu_phys_config_init(MuPhysConfig *cfg);

MuPhysWorld *mu_phys_create(const MuPhysConfig *cfg);
void mu_phys_destroy(MuPhysWorld *w);

/**
 * Advance the simulation by `dt` seconds.
 *
 * Use a fixed dt. A variable step changes the effective stiffness of the solver from
 * frame to frame, which makes stacks visibly breathe and makes results irreproducible.
 */
void mu_phys_step(MuPhysWorld *w, float dt);

/* --- Body creation. `density` is ignored for static bodies. ------------------ */

MuBodyId mu_phys_add_circle(MuPhysWorld *w, MuBodyType type, MuVec2 position, float radius,
                            float density);
MuBodyId mu_phys_add_box(MuPhysWorld *w, MuBodyType type, MuVec2 position, float half_w, float half_h,
                         float density);
/** `verts` must be convex and counter-clockwise, 3..MU_POLY_MAX_VERTS points, in local space. */
MuBodyId mu_phys_add_polygon(MuPhysWorld *w, MuBodyType type, MuVec2 position, const MuVec2 *verts,
                             int count, float density);

void mu_phys_remove(MuPhysWorld *w, MuBodyId id);
bool mu_phys_valid(const MuPhysWorld *w, MuBodyId id);
int mu_phys_body_count(const MuPhysWorld *w);

/** Iterate live bodies: pass MU_BODY_NONE to start, returns MU_BODY_NONE when done. */
MuBodyId mu_phys_next(const MuPhysWorld *w, MuBodyId prev);

/* --- State ------------------------------------------------------------------ */

bool mu_phys_get_transform(const MuPhysWorld *w, MuBodyId id, MuVec2 *out_position, float *out_angle);
void mu_phys_set_transform(MuPhysWorld *w, MuBodyId id, MuVec2 position, float angle);

bool mu_phys_get_velocity(const MuPhysWorld *w, MuBodyId id, MuVec2 *out_linear, float *out_angular);
void mu_phys_set_velocity(MuPhysWorld *w, MuBodyId id, MuVec2 linear, float angular);

/** World-space axis-aligned bounds of the shape, as of the last step. */
bool mu_phys_get_aabb(const MuPhysWorld *w, MuBodyId id, MuRect *out);

void mu_phys_set_material(MuPhysWorld *w, MuBodyId id, float restitution, float friction);
void mu_phys_set_damping(MuPhysWorld *w, MuBodyId id, float linear, float angular);

void mu_phys_apply_force(MuPhysWorld *w, MuBodyId id, MuVec2 force);
void mu_phys_apply_impulse(MuPhysWorld *w, MuBodyId id, MuVec2 impulse, MuVec2 world_point);

/** Arbitrary caller data, e.g. the MuNode this body drives. */
void mu_phys_set_user(MuPhysWorld *w, MuBodyId id, void *user);
void *mu_phys_get_user(const MuPhysWorld *w, MuBodyId id);

/** Number of contact manifolds resolved in the last step. Useful in tests. */
int mu_phys_contact_count(const MuPhysWorld *w);

#ifdef __cplusplus
}
#endif

#endif
