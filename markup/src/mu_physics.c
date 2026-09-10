#include "mu_physics_internal.h"

#include <stdlib.h>
#include <string.h>

/*
 * World, integration and the constraint solver.
 *
 * Each step:
 *   1. integrate forces into velocity
 *   2. broadphase -> candidate pairs, narrowphase -> manifolds
 *   3. carry accumulated impulses over from last step's matching contact points
 *   4. warm start: reapply those impulses before solving
 *   5. iterate velocity constraints (normal, then friction)
 *   6. iterate position constraints against a separate pseudo-velocity
 *   7. integrate both into position, then discard the pseudo-velocity
 *
 * Steps 3 and 4 are what make stacks work. Solving from zero each step means a box has to
 * rediscover the force holding it up from scratch, so the stack sinks and springs back.
 * Reusing last step's answer starts the solver near the correct value.
 *
 * Step 6 is split impulses rather than a Baumgarte bias folded into the velocity
 * constraint. Baumgarte works, but the energy it injects is real and it accumulates: a
 * seven-box stack built that way drifted sideways a little more every step until it
 * toppled. Correcting through a velocity that is thrown away after integration fixes the
 * overlap without feeding the system. Penetration within `slop` is left alone entirely,
 * so contacts resting within floating-point noise do not jitter.
 *
 * Both passes run `velocity_iterations` times. The default of 8 holds a seven-box stack;
 * 4 does not, which is in line with what this class of solver manages generally.
 */

#define MU_MAX_CONTACTS 4096

struct MuPhysWorld {
    MuPhysConfig cfg;

    MuBody *bodies;
    int body_count;
    int body_cap;
    int live_count;

    MuContact *contacts;
    int contact_count;
    int contact_cap;

    /* Previous step's contacts, kept so impulses survive into the next step. */
    MuContact *prev;
    int prev_count;
};

/* Ids pack a 1-based slot index and a generation, so an id referring to a removed body
 * fails validation instead of addressing whoever reused the slot. */
static MuBodyId make_id(int slot, uint16_t gen) {
    return ((uint32_t)(slot + 1) << 16) | (uint32_t)gen;
}
static int id_slot(MuBodyId id) {
    return (int)(id >> 16) - 1;
}
static uint16_t id_gen(MuBodyId id) {
    return (uint16_t)(id & 0xFFFFu);
}

static MuBody *body_of(MuPhysWorld *w, MuBodyId id) {
    if (!w || id == MU_BODY_NONE) return NULL;
    int slot = id_slot(id);
    if (slot < 0 || slot >= w->body_count) return NULL;
    MuBody *b = &w->bodies[slot];
    if (!b->alive || b->generation != id_gen(id)) return NULL;
    return b;
}

static const MuBody *body_of_const(const MuPhysWorld *w, MuBodyId id) {
    return body_of((MuPhysWorld *)w, id);
}

void mu_phys_config_init(MuPhysConfig *cfg) {
    if (!cfg) return;
    cfg->gravity = (MuVec2){0.f, 900.f}; /* +y is down, matching screen coordinates */
    cfg->velocity_iterations = 8;
    cfg->slop = 0.5f;
    cfg->correction = 0.2f;
    cfg->rest_velocity = 60.f;
}

MuPhysWorld *mu_phys_create(const MuPhysConfig *cfg) {
    MuPhysWorld *w = (MuPhysWorld *)calloc(1, sizeof(MuPhysWorld));
    if (!w) return NULL;
    if (cfg)
        w->cfg = *cfg;
    else
        mu_phys_config_init(&w->cfg);
    if (w->cfg.velocity_iterations < 1) w->cfg.velocity_iterations = 1;
    return w;
}

void mu_phys_destroy(MuPhysWorld *w) {
    if (!w) return;
    free(w->bodies);
    free(w->contacts);
    free(w->prev);
    free(w);
}

/* --- Body creation ---------------------------------------------------------- */

static MuBody *alloc_body(MuPhysWorld *w, int *out_slot) {
    for (int i = 0; i < w->body_count; i++) {
        if (!w->bodies[i].alive) {
            *out_slot = i;
            return &w->bodies[i];
        }
    }
    if (w->body_count >= w->body_cap) {
        int cap = w->body_cap ? w->body_cap * 2 : 32;
        MuBody *next = (MuBody *)realloc(w->bodies, sizeof(MuBody) * (size_t)cap);
        if (!next) return NULL;
        memset(next + w->body_cap, 0, sizeof(MuBody) * (size_t)(cap - w->body_cap));
        w->bodies = next;
        w->body_cap = cap;
    }
    *out_slot = w->body_count++;
    return &w->bodies[*out_slot];
}

static void finish_body(MuBody *b, MuBodyType type, MuVec2 position, float density) {
    b->type = type;
    b->position = position;
    b->angle = 0.f;
    b->rot = mrot(0.f);
    b->velocity = (MuVec2){0.f, 0.f};
    b->angular_velocity = 0.f;
    b->force = (MuVec2){0.f, 0.f};
    b->torque = 0.f;
    b->restitution = 0.f;
    b->friction = 0.4f;
    b->linear_damping = 0.f;
    b->angular_damping = 0.05f;
    b->user = NULL;
    b->alive = true;

    if (type == MU_BODY_DYNAMIC) {
        mu_shape_mass(&b->shape, density > 0.f ? density : 1.f, &b->mass, &b->inertia);
        b->inv_mass = b->mass > 0.f ? 1.f / b->mass : 0.f;
        b->inv_inertia = b->inertia > 0.f ? 1.f / b->inertia : 0.f;
    } else {
        /* Static and kinematic bodies are immovable as far as the solver is concerned. */
        b->mass = b->inertia = 0.f;
        b->inv_mass = b->inv_inertia = 0.f;
    }
    mu_body_update_aabb(b);
}

MuBodyId mu_phys_add_circle(MuPhysWorld *w, MuBodyType type, MuVec2 position, float radius, float density) {
    if (!w || radius <= 0.f) return MU_BODY_NONE;
    int slot = 0;
    MuBody *b = alloc_body(w, &slot);
    if (!b) return MU_BODY_NONE;

    uint16_t gen = b->generation;
    memset(b, 0, sizeof(*b));
    b->generation = gen;
    b->shape.type = MU_SHAPE_CIRCLE;
    b->shape.radius = radius;
    finish_body(b, type, position, density);
    w->live_count++;
    return make_id(slot, gen);
}

MuBodyId mu_phys_add_polygon(MuPhysWorld *w, MuBodyType type, MuVec2 position, const MuVec2 *verts,
                             int count, float density) {
    if (!w) return MU_BODY_NONE;
    MuShape shape;
    memset(&shape, 0, sizeof(shape));
    MuVec2 centroid = {0.f, 0.f};
    if (!mu_shape_init_polygon(&shape, verts, count, &centroid)) return MU_BODY_NONE;

    int slot = 0;
    MuBody *b = alloc_body(w, &slot);
    if (!b) return MU_BODY_NONE;

    uint16_t gen = b->generation;
    memset(b, 0, sizeof(*b));
    b->generation = gen;
    b->shape = shape;
    /* Vertices were recentred on the centroid, so shift the body to keep the shape where
     * the caller drew it. */
    finish_body(b, type, mv_add(position, centroid), density);
    w->live_count++;
    return make_id(slot, gen);
}

MuBodyId mu_phys_add_box(MuPhysWorld *w, MuBodyType type, MuVec2 position, float half_w, float half_h,
                         float density) {
    if (half_w <= 0.f || half_h <= 0.f) return MU_BODY_NONE;
    const MuVec2 verts[4] = {
        {-half_w, -half_h}, {half_w, -half_h}, {half_w, half_h}, {-half_w, half_h},
    };
    return mu_phys_add_polygon(w, type, position, verts, 4, density);
}

void mu_phys_remove(MuPhysWorld *w, MuBodyId id) {
    MuBody *b = body_of(w, id);
    if (!b) return;
    b->alive = false;
    b->generation++; /* invalidates every outstanding id for this slot */
    w->live_count--;
    /* Drop cached contacts: their slot indices would now refer to a dead body. */
    w->prev_count = 0;
}

bool mu_phys_valid(const MuPhysWorld *w, MuBodyId id) {
    return body_of_const(w, id) != NULL;
}

int mu_phys_body_count(const MuPhysWorld *w) {
    return w ? w->live_count : 0;
}

MuBodyId mu_phys_next(const MuPhysWorld *w, MuBodyId prev) {
    if (!w) return MU_BODY_NONE;
    int start = (prev == MU_BODY_NONE) ? 0 : id_slot(prev) + 1;
    for (int i = start; i < w->body_count; i++)
        if (w->bodies[i].alive) return make_id(i, w->bodies[i].generation);
    return MU_BODY_NONE;
}

/* --- State access ----------------------------------------------------------- */

bool mu_phys_get_transform(const MuPhysWorld *w, MuBodyId id, MuVec2 *out_position, float *out_angle) {
    const MuBody *b = body_of_const(w, id);
    if (!b) return false;
    if (out_position) *out_position = b->position;
    if (out_angle) *out_angle = b->angle;
    return true;
}

void mu_phys_set_transform(MuPhysWorld *w, MuBodyId id, MuVec2 position, float angle) {
    MuBody *b = body_of(w, id);
    if (!b) return;
    b->position = position;
    b->angle = angle;
    b->rot = mrot(angle);
    mu_body_update_aabb(b);
    w->prev_count = 0; /* teleporting invalidates cached impulses */
}

bool mu_phys_get_velocity(const MuPhysWorld *w, MuBodyId id, MuVec2 *out_linear, float *out_angular) {
    const MuBody *b = body_of_const(w, id);
    if (!b) return false;
    if (out_linear) *out_linear = b->velocity;
    if (out_angular) *out_angular = b->angular_velocity;
    return true;
}

void mu_phys_set_velocity(MuPhysWorld *w, MuBodyId id, MuVec2 linear, float angular) {
    MuBody *b = body_of(w, id);
    if (!b) return;
    b->velocity = linear;
    b->angular_velocity = angular;
}

bool mu_phys_get_aabb(const MuPhysWorld *w, MuBodyId id, MuRect *out) {
    const MuBody *b = body_of_const(w, id);
    if (!b) return false;
    if (out) *out = b->aabb;
    return true;
}

void mu_phys_set_material(MuPhysWorld *w, MuBodyId id, float restitution, float friction) {
    MuBody *b = body_of(w, id);
    if (!b) return;
    b->restitution = restitution < 0.f ? 0.f : (restitution > 1.f ? 1.f : restitution);
    b->friction = friction < 0.f ? 0.f : friction;
}

void mu_phys_set_damping(MuPhysWorld *w, MuBodyId id, float linear, float angular) {
    MuBody *b = body_of(w, id);
    if (!b) return;
    b->linear_damping = linear < 0.f ? 0.f : linear;
    b->angular_damping = angular < 0.f ? 0.f : angular;
}

void mu_phys_apply_force(MuPhysWorld *w, MuBodyId id, MuVec2 force) {
    MuBody *b = body_of(w, id);
    if (!b || b->type != MU_BODY_DYNAMIC) return;
    b->force = mv_add(b->force, force);
}

void mu_phys_apply_impulse(MuPhysWorld *w, MuBodyId id, MuVec2 impulse, MuVec2 world_point) {
    MuBody *b = body_of(w, id);
    if (!b || b->type != MU_BODY_DYNAMIC) return;
    b->velocity = mv_add(b->velocity, mv_scale(impulse, b->inv_mass));
    b->angular_velocity += b->inv_inertia * mv_cross(mv_sub(world_point, b->position), impulse);
}

void mu_phys_set_user(MuPhysWorld *w, MuBodyId id, void *user) {
    MuBody *b = body_of(w, id);
    if (b) b->user = user;
}

void *mu_phys_get_user(const MuPhysWorld *w, MuBodyId id) {
    const MuBody *b = body_of_const(w, id);
    return b ? b->user : NULL;
}

int mu_phys_contact_count(const MuPhysWorld *w) {
    return w ? w->contact_count : 0;
}

/* --- Broadphase ------------------------------------------------------------- */

static bool aabb_overlap(MuRect a, MuRect b) {
    return a.x <= b.x + b.w && b.x <= a.x + a.w && a.y <= b.y + b.h && b.y <= a.y + a.h;
}

static bool contacts_reserve(MuPhysWorld *w, int need) {
    if (need <= w->contact_cap) return true;
    int cap = w->contact_cap ? w->contact_cap : 64;
    while (cap < need && cap < MU_MAX_CONTACTS) cap *= 2;
    MuContact *next = (MuContact *)realloc(w->contacts, sizeof(MuContact) * (size_t)cap);
    if (!next) return false;
    w->contacts = next;
    w->contact_cap = cap;
    return true;
}

/**
 * Brute-force O(n^2) pair test.
 *
 * Deliberately simple: it is correct, allocation-free and fine into the low hundreds of
 * bodies. A grid or BVH belongs here once a profile says so, and swapping it out touches
 * nothing else.
 */
static void broadphase(MuPhysWorld *w) {
    w->contact_count = 0;

    for (int i = 0; i < w->body_count; i++) {
        MuBody *a = &w->bodies[i];
        if (!a->alive) continue;
        for (int j = i + 1; j < w->body_count; j++) {
            MuBody *b = &w->bodies[j];
            if (!b->alive) continue;
            /* Two immovable bodies can never resolve anything. */
            if (a->inv_mass == 0.f && b->inv_mass == 0.f) continue;
            if (!aabb_overlap(a->aabb, b->aabb)) continue;

            MuManifold m;
            if (!mu_collide(a, b, &m)) continue;
            if (!contacts_reserve(w, w->contact_count + 1)) return;

            MuContact *c = &w->contacts[w->contact_count++];
            c->a = i;
            c->b = j;
            c->id_a = make_id(i, a->generation);
            c->id_b = make_id(j, b->generation);
            c->manifold = m;
            /* Combine materials the conventional way: geometric mean for friction so a
             * slippery body stays slippery against anything, max for restitution so the
             * bouncier surface wins. */
            c->friction = sqrtf(a->friction * b->friction);
            c->restitution = a->restitution > b->restitution ? a->restitution : b->restitution;
        }
    }
}

/** Carry accumulated impulses from the matching point in last step's contact. */
static void warm_start_transfer(MuPhysWorld *w) {
    for (int i = 0; i < w->contact_count; i++) {
        MuContact *c = &w->contacts[i];
        for (int p = 0; p < c->manifold.count; p++) {
            c->manifold.points[p].normal_impulse = 0.f;
            c->manifold.points[p].tangent_impulse = 0.f;
            c->manifold.points[p].pseudo_impulse = 0.f; /* per-step, never warm started */
        }

        for (int k = 0; k < w->prev_count; k++) {
            MuContact *old = &w->prev[k];
            if (old->id_a != c->id_a || old->id_b != c->id_b) continue;
            for (int p = 0; p < c->manifold.count; p++) {
                for (int q = 0; q < old->manifold.count; q++) {
                    if (old->manifold.points[q].id != c->manifold.points[p].id) continue;
                    c->manifold.points[p].normal_impulse = old->manifold.points[q].normal_impulse;
                    c->manifold.points[p].tangent_impulse = old->manifold.points[q].tangent_impulse;
                    break;
                }
            }
            break;
        }
    }
}

static void save_contacts(MuPhysWorld *w) {
    if (w->contact_count > 0) {
        MuContact *next = (MuContact *)realloc(w->prev, sizeof(MuContact) * (size_t)w->contact_count);
        if (!next) {
            w->prev_count = 0;
            return;
        }
        w->prev = next;
        memcpy(w->prev, w->contacts, sizeof(MuContact) * (size_t)w->contact_count);
    }
    w->prev_count = w->contact_count;
}

/* --- Solver ----------------------------------------------------------------- */

static MuVec2 point_velocity(const MuBody *b, MuVec2 r) {
    return mv_add(b->velocity, mv_cross_sv(b->angular_velocity, r));
}

static void prepare_contacts(MuPhysWorld *w, float inv_dt) {
    for (int i = 0; i < w->contact_count; i++) {
        MuContact *c = &w->contacts[i];
        MuBody *a = &w->bodies[c->a];
        MuBody *b = &w->bodies[c->b];
        MuVec2 n = c->manifold.normal;
        MuVec2 t = (MuVec2){-n.y, n.x};

        for (int p = 0; p < c->manifold.count; p++) {
            MuContactPoint *cp = &c->manifold.points[p];
            cp->ra = mv_sub(cp->position, a->position);
            cp->rb = mv_sub(cp->position, b->position);

            /* Effective mass along the normal, including the rotational term. */
            float rna = mv_cross(cp->ra, n);
            float rnb = mv_cross(cp->rb, n);
            float k_normal = a->inv_mass + b->inv_mass + a->inv_inertia * rna * rna +
                             b->inv_inertia * rnb * rnb;
            cp->normal_mass = k_normal > 0.f ? 1.f / k_normal : 0.f;

            float rta = mv_cross(cp->ra, t);
            float rtb = mv_cross(cp->rb, t);
            float k_tangent = a->inv_mass + b->inv_mass + a->inv_inertia * rta * rta +
                              b->inv_inertia * rtb * rtb;
            cp->tangent_mass = k_tangent > 0.f ? 1.f / k_tangent : 0.f;

            /* Baumgarte: only correct penetration beyond the slop, and only a fraction
             * per step, so resting contacts do not vibrate. */
            float excess = cp->penetration - w->cfg.slop;
            cp->bias = excess > 0.f ? w->cfg.correction * inv_dt * excess : 0.f;

            /* Restitution, from the approach speed before any impulses are applied.
             * Below rest_velocity it is suppressed: otherwise a settled stack keeps
             * being handed a little energy back and never sleeps. */
            MuVec2 dv = mv_sub(point_velocity(b, cp->rb), point_velocity(a, cp->ra));
            float vn = mv_dot(dv, n);
            cp->rel_velocity_bias = 0.f;
            if (vn < -w->cfg.rest_velocity) cp->rel_velocity_bias = -c->restitution * vn;
        }
    }
}

/** Reapply last step's impulses so the solver starts near the answer. */
static void warm_start_apply(MuPhysWorld *w) {
    for (int i = 0; i < w->contact_count; i++) {
        MuContact *c = &w->contacts[i];
        MuBody *a = &w->bodies[c->a];
        MuBody *b = &w->bodies[c->b];
        MuVec2 n = c->manifold.normal;
        MuVec2 t = (MuVec2){-n.y, n.x};

        for (int p = 0; p < c->manifold.count; p++) {
            MuContactPoint *cp = &c->manifold.points[p];
            MuVec2 impulse = mv_add(mv_scale(n, cp->normal_impulse), mv_scale(t, cp->tangent_impulse));
            a->velocity = mv_sub(a->velocity, mv_scale(impulse, a->inv_mass));
            a->angular_velocity -= a->inv_inertia * mv_cross(cp->ra, impulse);
            b->velocity = mv_add(b->velocity, mv_scale(impulse, b->inv_mass));
            b->angular_velocity += b->inv_inertia * mv_cross(cp->rb, impulse);
        }
    }
}

static void solve_velocity(MuPhysWorld *w) {
    for (int iter = 0; iter < w->cfg.velocity_iterations; iter++) {
        for (int i = 0; i < w->contact_count; i++) {
            MuContact *c = &w->contacts[i];
            MuBody *a = &w->bodies[c->a];
            MuBody *b = &w->bodies[c->b];
            MuVec2 n = c->manifold.normal;
            MuVec2 t = (MuVec2){-n.y, n.x};

            /* Friction first, clamped against the normal impulse accumulated so far.
             * Solving it before the normal this iteration uses last iteration's value,
             * which is the usual trade and converges fine. */
            for (int p = 0; p < c->manifold.count; p++) {
                MuContactPoint *cp = &c->manifold.points[p];
                MuVec2 dv = mv_sub(point_velocity(b, cp->rb), point_velocity(a, cp->ra));
                float vt = mv_dot(dv, t);
                float lambda = cp->tangent_mass * (-vt);

                float max_friction = c->friction * cp->normal_impulse;
                float old = cp->tangent_impulse;
                cp->tangent_impulse = old + lambda;
                if (cp->tangent_impulse < -max_friction) cp->tangent_impulse = -max_friction;
                if (cp->tangent_impulse > max_friction) cp->tangent_impulse = max_friction;
                lambda = cp->tangent_impulse - old;

                MuVec2 impulse = mv_scale(t, lambda);
                a->velocity = mv_sub(a->velocity, mv_scale(impulse, a->inv_mass));
                a->angular_velocity -= a->inv_inertia * mv_cross(cp->ra, impulse);
                b->velocity = mv_add(b->velocity, mv_scale(impulse, b->inv_mass));
                b->angular_velocity += b->inv_inertia * mv_cross(cp->rb, impulse);
            }

            for (int p = 0; p < c->manifold.count; p++) {
                MuContactPoint *cp = &c->manifold.points[p];
                MuVec2 dv = mv_sub(point_velocity(b, cp->rb), point_velocity(a, cp->ra));
                float vn = mv_dot(dv, n);

                /* No penetration bias here — that is the split-impulse pass's job, so
                 * pushing bodies apart never becomes real kinetic energy. */
                float lambda = cp->normal_mass * (-vn + cp->rel_velocity_bias);

                /* Accumulated impulse must stay non-negative: contacts push, never pull.
                 * Clamping the total rather than the increment is what allows an
                 * individual iteration to subtract without the contact sticking. */
                float old = cp->normal_impulse;
                cp->normal_impulse = old + lambda;
                if (cp->normal_impulse < 0.f) cp->normal_impulse = 0.f;
                lambda = cp->normal_impulse - old;

                MuVec2 impulse = mv_scale(n, lambda);
                a->velocity = mv_sub(a->velocity, mv_scale(impulse, a->inv_mass));
                a->angular_velocity -= a->inv_inertia * mv_cross(cp->ra, impulse);
                b->velocity = mv_add(b->velocity, mv_scale(impulse, b->inv_mass));
                b->angular_velocity += b->inv_inertia * mv_cross(cp->rb, impulse);
            }
        }
    }
}

/**
 * Push overlapping bodies apart using a separate pseudo-velocity.
 *
 * Folding a Baumgarte bias into the velocity constraint does the same job, but the energy
 * it adds is real: a deep contact gets a genuine shove that outlives the overlap. In a
 * stack that surplus accumulates, and it showed up here as the tower walking sideways and
 * eventually toppling. Correcting through a velocity that is used for integration and
 * then discarded fixes the geometry without feeding the system.
 */
static void solve_position(MuPhysWorld *w) {
    for (int iter = 0; iter < w->cfg.velocity_iterations; iter++) {
        for (int i = 0; i < w->contact_count; i++) {
            MuContact *c = &w->contacts[i];
            MuBody *a = &w->bodies[c->a];
            MuBody *b = &w->bodies[c->b];
            MuVec2 n = c->manifold.normal;

            for (int p = 0; p < c->manifold.count; p++) {
                MuContactPoint *cp = &c->manifold.points[p];
                if (cp->bias <= 0.f) continue;

                MuVec2 dv = mv_sub(mv_add(b->pseudo_velocity, mv_cross_sv(b->pseudo_angular, cp->rb)),
                                   mv_add(a->pseudo_velocity, mv_cross_sv(a->pseudo_angular, cp->ra)));
                float vn = mv_dot(dv, n);
                float lambda = cp->normal_mass * (cp->bias - vn);

                float old = cp->pseudo_impulse;
                cp->pseudo_impulse = old + lambda;
                if (cp->pseudo_impulse < 0.f) cp->pseudo_impulse = 0.f;
                lambda = cp->pseudo_impulse - old;

                MuVec2 impulse = mv_scale(n, lambda);
                a->pseudo_velocity = mv_sub(a->pseudo_velocity, mv_scale(impulse, a->inv_mass));
                a->pseudo_angular -= a->inv_inertia * mv_cross(cp->ra, impulse);
                b->pseudo_velocity = mv_add(b->pseudo_velocity, mv_scale(impulse, b->inv_mass));
                b->pseudo_angular += b->inv_inertia * mv_cross(cp->rb, impulse);
            }
        }
    }
}

/* --- Step ------------------------------------------------------------------- */

void mu_phys_step(MuPhysWorld *w, float dt) {
    if (!w || dt <= 0.f) return;
    float inv_dt = 1.f / dt;

    /* Integrate forces. Damping is applied as an implicit factor rather than a force so
     * it stays stable at any dt. */
    for (int i = 0; i < w->body_count; i++) {
        MuBody *b = &w->bodies[i];
        if (!b->alive || b->type != MU_BODY_DYNAMIC) continue;
        MuVec2 accel = mv_add(w->cfg.gravity, mv_scale(b->force, b->inv_mass));
        b->velocity = mv_add(b->velocity, mv_scale(accel, dt));
        b->angular_velocity += b->inv_inertia * b->torque * dt;
        b->velocity = mv_scale(b->velocity, 1.f / (1.f + dt * b->linear_damping));
        b->angular_velocity *= 1.f / (1.f + dt * b->angular_damping);
    }

    for (int i = 0; i < w->body_count; i++) {
        w->bodies[i].pseudo_velocity = (MuVec2){0.f, 0.f};
        w->bodies[i].pseudo_angular = 0.f;
    }

    broadphase(w);
    warm_start_transfer(w);
    prepare_contacts(w, inv_dt);
    warm_start_apply(w);
    solve_velocity(w);
    solve_position(w);

    /* Integrate. Pseudo-velocity moves the body but is then discarded, so the positional
     * correction never becomes momentum. */
    for (int i = 0; i < w->body_count; i++) {
        MuBody *b = &w->bodies[i];
        if (!b->alive || b->type == MU_BODY_STATIC) continue;
        b->position = mv_add(b->position, mv_scale(mv_add(b->velocity, b->pseudo_velocity), dt));
        b->angle += (b->angular_velocity + b->pseudo_angular) * dt;
        b->rot = mrot(b->angle);
        b->pseudo_velocity = (MuVec2){0.f, 0.f};
        b->pseudo_angular = 0.f;
        b->force = (MuVec2){0.f, 0.f};
        b->torque = 0.f;
        mu_body_update_aabb(b);
    }

    save_contacts(w);
}
