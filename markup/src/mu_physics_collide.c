#include "mu_physics_internal.h"

#include <string.h>

/*
 * Narrowphase collision.
 *
 * Produces a manifold of at most two points, which is what a 2D solver needs to stop a
 * box rotating on a flat surface: one point can only resist translation, so a single
 * contact under a resting box lets it rock. Polygon-polygon therefore clips an incident
 * face against the reference face rather than reporting the deepest point alone.
 *
 * Every contact point carries a feature id built from the indices that generated it.
 * That id is what lets the solver find the same point next step and reuse its
 * accumulated impulse, which is what makes stacks settle instead of sinking.
 */

/* --- Shape setup ------------------------------------------------------------ */

bool mu_shape_init_polygon(MuShape *s, const MuVec2 *verts, int count, MuVec2 *out_centroid) {
    if (!s || !verts || count < 3 || count > MU_POLY_MAX_VERTS) return false;

    /* Signed area and centroid, via the standard polygon formulas. */
    float area2 = 0.f;
    MuVec2 centroid = {0.f, 0.f};
    for (int i = 0; i < count; i++) {
        MuVec2 p = verts[i];
        MuVec2 q = verts[(i + 1) % count];
        float cross = mv_cross(p, q);
        area2 += cross;
        centroid = mv_add(centroid, mv_scale(mv_add(p, q), cross));
    }
    if (fabsf(area2) < 1e-8f) return false; /* degenerate */

    bool ccw = area2 > 0.f;
    centroid = mv_scale(centroid, 1.f / (3.f * area2));

    /* Store centred on the centroid: mass properties and rotation are about the centre
     * of mass, so keeping the origin there removes a parallel-axis term everywhere. */
    for (int i = 0; i < count; i++) {
        int src = ccw ? i : (count - 1 - i); /* normalise winding to CCW */
        s->verts[i] = mv_sub(verts[src], centroid);
    }
    s->count = count;
    s->type = MU_SHAPE_POLYGON;
    s->radius = 0.f;

    for (int i = 0; i < count; i++) {
        MuVec2 edge = mv_sub(s->verts[(i + 1) % count], s->verts[i]);
        /* Outward normal for CCW winding. */
        s->normals[i] = mv_normalize((MuVec2){edge.y, -edge.x});
    }

    if (out_centroid) *out_centroid = centroid;
    return true;
}

void mu_shape_mass(const MuShape *s, float density, float *out_mass, float *out_inertia) {
    float mass = 0.f, inertia = 0.f;
    if (!s) {
        if (out_mass) *out_mass = 0.f;
        if (out_inertia) *out_inertia = 0.f;
        return;
    }

    if (s->type == MU_SHAPE_CIRCLE) {
        mass = density * 3.14159265358979f * s->radius * s->radius;
        inertia = 0.5f * mass * s->radius * s->radius;
    } else {
        /* Integrate over the triangle fan from the origin, which is the centroid. */
        float area = 0.f, num = 0.f;
        for (int i = 0; i < s->count; i++) {
            MuVec2 p = s->verts[i];
            MuVec2 q = s->verts[(i + 1) % s->count];
            float cross = mv_cross(p, q);
            area += 0.5f * cross;
            num += cross * (mv_dot(p, p) + mv_dot(p, q) + mv_dot(q, q));
        }
        mass = density * area;
        inertia = density * num / 12.f;
    }

    if (out_mass) *out_mass = mass;
    if (out_inertia) *out_inertia = inertia;
}

void mu_body_update_aabb(MuBody *b) {
    if (!b) return;
    if (b->shape.type == MU_SHAPE_CIRCLE) {
        float r = b->shape.radius;
        b->aabb = (MuRect){b->position.x - r, b->position.y - r, 2.f * r, 2.f * r};
        return;
    }

    MuVec2 first = mv_add(b->position, mrot_apply(b->rot, b->shape.verts[0]));
    float x0 = first.x, x1 = first.x, y0 = first.y, y1 = first.y;
    for (int i = 1; i < b->shape.count; i++) {
        MuVec2 p = mv_add(b->position, mrot_apply(b->rot, b->shape.verts[i]));
        if (p.x < x0) x0 = p.x;
        if (p.x > x1) x1 = p.x;
        if (p.y < y0) y0 = p.y;
        if (p.y > y1) y1 = p.y;
    }
    b->aabb = (MuRect){x0, y0, x1 - x0, y1 - y0};
}

/* --- Circle vs circle ------------------------------------------------------- */

static bool collide_circle_circle(const MuBody *a, const MuBody *b, MuManifold *out) {
    MuVec2 d = mv_sub(b->position, a->position);
    float r = a->shape.radius + b->shape.radius;
    float dist_sq = mv_len_sq(d);
    if (dist_sq > r * r) return false;

    float dist = sqrtf(dist_sq);
    /* Concentric circles have no defined normal; pick one so the solver can push apart. */
    out->normal = dist > 1e-6f ? mv_scale(d, 1.f / dist) : (MuVec2){0.f, -1.f};
    out->count = 1;
    out->points[0].penetration = r - dist;
    out->points[0].position =
        mv_add(a->position, mv_scale(out->normal, a->shape.radius - out->points[0].penetration * 0.5f));
    out->points[0].id = 0;
    return true;
}

/* --- Circle vs polygon ------------------------------------------------------ */

/** `poly` is the polygon body, `circ` the circle. `flip` reverses the output normal. */
static bool collide_polygon_circle(const MuBody *poly, const MuBody *circ, MuManifold *out, bool flip) {
    const MuShape *s = &poly->shape;
    float radius = circ->shape.radius;

    /* Work in the polygon's local space: one transform instead of N. */
    MuVec2 center = mrot_unapply(poly->rot, mv_sub(circ->position, poly->position));

    int best = 0;
    float best_sep = -1e30f;
    for (int i = 0; i < s->count; i++) {
        float sep = mv_dot(s->normals[i], mv_sub(center, s->verts[i]));
        if (sep > radius) return false; /* separating axis found */
        if (sep > best_sep) {
            best_sep = sep;
            best = i;
        }
    }

    MuVec2 v1 = s->verts[best];
    MuVec2 v2 = s->verts[(best + 1) % s->count];
    MuVec2 local_normal;
    MuVec2 local_point;

    if (best_sep < 1e-6f) {
        /* Centre is inside: push out along the least-penetrating face. */
        local_normal = s->normals[best];
        local_point = mv_scale(mv_add(v1, v2), 0.5f);
    } else {
        /* Which Voronoi region of the face the centre falls in. */
        float u1 = mv_dot(mv_sub(center, v1), mv_sub(v2, v1));
        float u2 = mv_dot(mv_sub(center, v2), mv_sub(v1, v2));
        if (u1 <= 0.f) {
            if (mv_len_sq(mv_sub(center, v1)) > radius * radius) return false;
            local_normal = mv_normalize(mv_sub(center, v1));
            local_point = v1;
        } else if (u2 <= 0.f) {
            if (mv_len_sq(mv_sub(center, v2)) > radius * radius) return false;
            local_normal = mv_normalize(mv_sub(center, v2));
            local_point = v2;
        } else {
            local_normal = s->normals[best];
            if (mv_dot(mv_sub(center, v1), local_normal) > radius) return false;
            local_point = mv_scale(mv_add(v1, v2), 0.5f);
        }
    }

    MuVec2 world_normal = mrot_apply(poly->rot, local_normal);
    float sep = mv_dot(mv_sub(center, local_point), local_normal);

    out->count = 1;
    out->points[0].penetration = radius - sep;
    out->points[0].position =
        mv_sub(circ->position, mv_scale(world_normal, radius - out->points[0].penetration * 0.5f));
    out->points[0].id = (uint32_t)best;
    /* Normal must run from a to b; it currently points polygon -> circle. */
    out->normal = flip ? mv_neg(world_normal) : world_normal;
    return true;
}

/* --- Polygon vs polygon ----------------------------------------------------- */

/** Greatest separation of `b` from any face of `a`, and which face. */
static float max_separation(const MuBody *a, const MuBody *b, int *out_face) {
    const MuShape *sa = &a->shape;
    const MuShape *sb = &b->shape;
    float best = -1e30f;
    int best_face = 0;

    for (int i = 0; i < sa->count; i++) {
        MuVec2 n_world = mrot_apply(a->rot, sa->normals[i]);
        MuVec2 n_in_b = mrot_unapply(b->rot, n_world);

        /* Support point of b in the direction opposing the face normal. */
        float min_proj = 1e30f;
        int min_idx = 0;
        for (int j = 0; j < sb->count; j++) {
            float proj = mv_dot(sb->verts[j], n_in_b);
            if (proj < min_proj) {
                min_proj = proj;
                min_idx = j;
            }
        }

        MuVec2 v_world = mv_add(a->position, mrot_apply(a->rot, sa->verts[i]));
        MuVec2 support_world = mv_add(b->position, mrot_apply(b->rot, sb->verts[min_idx]));
        float sep = mv_dot(mv_sub(support_world, v_world), n_world);

        if (sep > best) {
            best = sep;
            best_face = i;
        }
    }
    *out_face = best_face;
    return best;
}

/** Face of `b` most anti-parallel to `normal` — the one that will be clipped. */
static int incident_face(const MuBody *b, MuVec2 normal) {
    MuVec2 n_local = mrot_unapply(b->rot, normal);
    int best = 0;
    float best_dot = 1e30f;
    for (int i = 0; i < b->shape.count; i++) {
        float d = mv_dot(b->shape.normals[i], n_local);
        if (d < best_dot) {
            best_dot = d;
            best = i;
        }
    }
    return best;
}

typedef struct ClipVertex {
    MuVec2 v;
    uint32_t id;
} ClipVertex;

/** Clip a segment against a half-space; returns how many points survive. */
static int clip_segment(ClipVertex in[2], ClipVertex out[2], MuVec2 normal, float offset, uint32_t edge_id) {
    int count = 0;
    float d0 = mv_dot(normal, in[0].v) - offset;
    float d1 = mv_dot(normal, in[1].v) - offset;

    if (d0 <= 0.f) out[count++] = in[0];
    if (d1 <= 0.f) out[count++] = in[1];

    if (d0 * d1 < 0.f && count < 2) {
        float t = d0 / (d0 - d1);
        out[count].v = mv_add(in[0].v, mv_scale(mv_sub(in[1].v, in[0].v), t));
        out[count].id = edge_id;
        count++;
    }
    return count;
}

static bool collide_polygon_polygon(const MuBody *a, const MuBody *b, MuManifold *out) {
    int face_a = 0, face_b = 0;
    float sep_a = max_separation(a, b, &face_a);
    if (sep_a > 0.f) return false;
    float sep_b = max_separation(b, a, &face_b);
    if (sep_b > 0.f) return false;

    /* Pick the reference face, preferring a's unless b is clearly better. The tolerance
     * makes the choice sticky: a near-tied pair that alternated faces between steps
     * would change its feature ids each time and throw away the accumulated impulses,
     * which is exactly what warm starting depends on. Separations are <= 0 here, so
     * scaling by k_rel < 1 moves the threshold toward zero, i.e. favours a. */
    const float k_rel = 0.98f;
    const float k_abs = 0.001f;
    const MuBody *ref = a;
    const MuBody *inc = b;
    int ref_face = face_a;
    bool flip = false;
    if (sep_b > k_rel * sep_a + k_abs) {
        ref = b;
        inc = a;
        ref_face = face_b;
        flip = true;
    }

    MuVec2 ref_normal = mrot_apply(ref->rot, ref->shape.normals[ref_face]);
    int inc_face = incident_face(inc, ref_normal);

    ClipVertex incident[2];
    incident[0].v = mv_add(inc->position, mrot_apply(inc->rot, inc->shape.verts[inc_face]));
    incident[1].v =
        mv_add(inc->position, mrot_apply(inc->rot, inc->shape.verts[(inc_face + 1) % inc->shape.count]));
    incident[0].id = (uint32_t)((ref_face << 8) | inc_face);
    incident[1].id = (uint32_t)((ref_face << 8) | ((inc_face + 1) % inc->shape.count));

    MuVec2 rv1 = mv_add(ref->position, mrot_apply(ref->rot, ref->shape.verts[ref_face]));
    MuVec2 rv2 =
        mv_add(ref->position, mrot_apply(ref->rot, ref->shape.verts[(ref_face + 1) % ref->shape.count]));
    MuVec2 tangent = mv_normalize(mv_sub(rv2, rv1));

    ClipVertex clipped1[2], clipped2[2];
    /* Trim to the reference face's extent along its own tangent. */
    if (clip_segment(incident, clipped1, mv_neg(tangent), -mv_dot(tangent, rv1),
                     (uint32_t)(ref_face << 16 | 1)) < 2)
        return false;
    if (clip_segment(clipped1, clipped2, tangent, mv_dot(tangent, rv2), (uint32_t)(ref_face << 16 | 2)) < 2)
        return false;

    /* Keep whatever is behind the reference face. */
    float ref_offset = mv_dot(ref_normal, rv1);
    int count = 0;
    for (int i = 0; i < 2; i++) {
        float sep = mv_dot(ref_normal, clipped2[i].v) - ref_offset;
        if (sep <= 0.f) {
            out->points[count].penetration = -sep;
            out->points[count].position = clipped2[i].v;
            out->points[count].id = clipped2[i].id | (flip ? 0x80000000u : 0u);
            count++;
        }
    }
    if (count == 0) return false;

    out->count = count;
    out->normal = flip ? mv_neg(ref_normal) : ref_normal;
    return true;
}

/* --- Dispatch --------------------------------------------------------------- */

bool mu_collide(const MuBody *a, const MuBody *b, MuManifold *out) {
    if (!a || !b || !out) return false;
    memset(out, 0, sizeof(*out));

    if (a->shape.type == MU_SHAPE_CIRCLE && b->shape.type == MU_SHAPE_CIRCLE)
        return collide_circle_circle(a, b, out);
    if (a->shape.type == MU_SHAPE_POLYGON && b->shape.type == MU_SHAPE_CIRCLE)
        return collide_polygon_circle(a, b, out, false);
    if (a->shape.type == MU_SHAPE_CIRCLE && b->shape.type == MU_SHAPE_POLYGON)
        return collide_polygon_circle(b, a, out, true);
    return collide_polygon_polygon(a, b, out);
}
