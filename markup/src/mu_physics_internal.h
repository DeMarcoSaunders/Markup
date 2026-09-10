#ifndef MU_PHYSICS_INTERNAL_H
#define MU_PHYSICS_INTERNAL_H

#include "../include/markup/mu_physics.h"

#include <math.h>

/* Shared between mu_physics.c (world, integration, solver) and mu_physics_collide.c
 * (narrowphase). Not installed. */

#define MU_MANIFOLD_MAX_POINTS 2

/* --- Small vector/rotation helpers ------------------------------------------ */

static inline MuVec2 mv_add(MuVec2 a, MuVec2 b) { return (MuVec2){a.x + b.x, a.y + b.y}; }
static inline MuVec2 mv_sub(MuVec2 a, MuVec2 b) { return (MuVec2){a.x - b.x, a.y - b.y}; }
static inline MuVec2 mv_scale(MuVec2 a, float s) { return (MuVec2){a.x * s, a.y * s}; }
static inline MuVec2 mv_neg(MuVec2 a) { return (MuVec2){-a.x, -a.y}; }
static inline float mv_dot(MuVec2 a, MuVec2 b) { return a.x * b.x + a.y * b.y; }
/* 2D "cross" is the z of the 3D cross product: a scalar. */
static inline float mv_cross(MuVec2 a, MuVec2 b) { return a.x * b.y - a.y * b.x; }
/* Cross of a vector with a scalar angular quantity, both orders. */
static inline MuVec2 mv_cross_vs(MuVec2 a, float s) { return (MuVec2){s * a.y, -s * a.x}; }
static inline MuVec2 mv_cross_sv(float s, MuVec2 a) { return (MuVec2){-s * a.y, s * a.x}; }
static inline float mv_len_sq(MuVec2 a) { return a.x * a.x + a.y * a.y; }
static inline float mv_len(MuVec2 a) { return sqrtf(mv_len_sq(a)); }

static inline MuVec2 mv_normalize(MuVec2 a) {
    float l = mv_len(a);
    if (l < 1e-8f) return (MuVec2){0.f, 0.f};
    return (MuVec2){a.x / l, a.y / l};
}

/** Rotation stored as cos/sin so the hot paths never call trig. */
typedef struct MuRot {
    float c, s;
} MuRot;

static inline MuRot mrot(float angle) { return (MuRot){cosf(angle), sinf(angle)}; }
static inline MuVec2 mrot_apply(MuRot r, MuVec2 v) {
    return (MuVec2){r.c * v.x - r.s * v.y, r.s * v.x + r.c * v.y};
}
/** Inverse rotation: world vector into local space. */
static inline MuVec2 mrot_unapply(MuRot r, MuVec2 v) {
    return (MuVec2){r.c * v.x + r.s * v.y, -r.s * v.x + r.c * v.y};
}

/* --- Shapes ----------------------------------------------------------------- */

typedef enum MuShapeType {
    MU_SHAPE_CIRCLE = 0,
    MU_SHAPE_POLYGON,
} MuShapeType;

typedef struct MuShape {
    MuShapeType type;
    float radius;                      /* circle only */
    MuVec2 verts[MU_POLY_MAX_VERTS];   /* polygon, local space, centred on the centroid */
    MuVec2 normals[MU_POLY_MAX_VERTS]; /* outward face normals, normals[i] belongs to edge i..i+1 */
    int count;
} MuShape;

/* --- Bodies ----------------------------------------------------------------- */

typedef struct MuBody {
    MuBodyType type;
    bool alive;
    uint16_t generation; /* bumped on removal so stale ids are rejected */

    MuVec2 position; /* of the centre of mass */
    float angle;
    MuRot rot; /* cached from angle each step */

    MuVec2 velocity;
    float angular_velocity;
    MuVec2 force;
    float torque;

    /* Split-impulse position correction. Penetration is pushed out through these rather
     * than through the real velocity, so the correction does not leave kinetic energy
     * behind. Discarded at the end of each step. */
    MuVec2 pseudo_velocity;
    float pseudo_angular;

    float mass, inv_mass;
    float inertia, inv_inertia;
    float restitution, friction;
    float linear_damping, angular_damping;

    MuShape shape;
    MuRect aabb;
    void *user;
} MuBody;

/* --- Contacts --------------------------------------------------------------- */

typedef struct MuContactPoint {
    MuVec2 position;    /* world space */
    float penetration;  /* positive when overlapping */
    uint32_t id;        /* feature id, matches points across steps for warm starting */

    /* Persisted across steps: the whole point of warm starting. */
    float normal_impulse;
    float tangent_impulse;
    /* Position-correction accumulator; per-step, never warm started. */
    float pseudo_impulse;

    /* Recomputed each step. */
    MuVec2 ra, rb;
    float normal_mass, tangent_mass;
    float bias;
    float rel_velocity_bias; /* restitution target */
} MuContactPoint;

typedef struct MuManifold {
    MuVec2 normal; /* unit, points from body a toward body b */
    MuContactPoint points[MU_MANIFOLD_MAX_POINTS];
    int count;
} MuManifold;

typedef struct MuContact {
    int a, b; /* body slot indices */
    uint32_t id_a, id_b;
    MuManifold manifold;
    float restitution, friction;
} MuContact;

/**
 * Narrowphase. Fills `out` and returns true when the shapes touch.
 * The normal always points from `a` toward `b`.
 */
bool mu_collide(const MuBody *a, const MuBody *b, MuManifold *out);

/** Recompute a body's world AABB from its shape and transform. */
void mu_body_update_aabb(MuBody *b);

/** Build normals, recentre on the centroid, and compute mass properties. */
bool mu_shape_init_polygon(MuShape *s, const MuVec2 *verts, int count, MuVec2 *out_centroid);
void mu_shape_mass(const MuShape *s, float density, float *out_mass, float *out_inertia);

#endif
