#include "../include/markup/mu_anim.h"

#include <stdlib.h>
#include <string.h>

/* --- The node animator ----------------------------------------------------- */

typedef struct AnimEntry {
    uint32_t node_id;
    MuSpring x, y, w, h;
} AnimEntry;

struct MuAnimator {
    AnimEntry *items;
    int count;
    int cap;
};

MuAnimator *mu_anim_create(void) {
    return (MuAnimator *)calloc(1, sizeof(MuAnimator));
}

void mu_anim_destroy(MuAnimator *a) {
    if (!a) return;
    free(a->items);
    free(a);
}

static AnimEntry *anim_find(const MuAnimator *a, uint32_t id) {
    if (!a || id == 0u) return NULL;
    for (int i = 0; i < a->count; i++)
        if (a->items[i].node_id == id) return &a->items[i];
    return NULL;
}

/** Swap with the last and shrink. Order carries no meaning here. */
static void anim_remove_at(MuAnimator *a, int i) {
    a->items[i] = a->items[a->count - 1];
    a->count--;
}

bool mu_anim_to(MuAnimator *a, MuNode *node, MuRect target, float response, float damping) {
    if (!a || !node || node->id == 0u) return false;

    AnimEntry *e = anim_find(a, node->id);
    if (!e) {
        if (a->count == a->cap) {
            int cap = a->cap ? a->cap * 2 : 8;
            AnimEntry *next = (AnimEntry *)realloc(a->items, (size_t)cap * sizeof(AnimEntry));
            if (!next) return false;
            a->items = next;
            a->cap = cap;
        }
        e = &a->items[a->count++];
        e->node_id = node->id;
        /* Seeded from where the node is now — but only this once. From here the springs
         * are the authority, so re-aiming does not yank the value back. */
        mu_spring_init(&e->x, node->bounds.x, response, damping);
        mu_spring_init(&e->y, node->bounds.y, response, damping);
        mu_spring_init(&e->w, node->bounds.w, response, damping);
        mu_spring_init(&e->h, node->bounds.h, response, damping);
    } else {
        mu_spring_reshape(&e->x, response, damping);
        mu_spring_reshape(&e->y, response, damping);
        mu_spring_reshape(&e->w, response, damping);
        mu_spring_reshape(&e->h, response, damping);
    }

    mu_spring_set_target(&e->x, target.x);
    mu_spring_set_target(&e->y, target.y);
    mu_spring_set_target(&e->w, target.w);
    mu_spring_set_target(&e->h, target.h);
    return true;
}

void mu_anim_set(MuAnimator *a, MuNode *node, MuRect bounds) {
    if (!node) return;
    mu_anim_cancel(a, node);
    node->bounds = bounds;
}

void mu_anim_cancel(MuAnimator *a, const MuNode *node) {
    if (!a || !node) return;
    AnimEntry *e = anim_find(a, node->id);
    if (e) anim_remove_at(a, (int)(e - a->items));
}

bool mu_anim_active(const MuAnimator *a, const MuNode *node) {
    return node && anim_find(a, node->id) != NULL;
}

int mu_anim_count(const MuAnimator *a) {
    return a ? a->count : 0;
}

int mu_anim_advance(MuContext *ctx, MuAnimator *a, float dt) {
    if (!ctx || !a) return 0;

    int moving = 0;
    for (int i = 0; i < a->count;) {
        AnimEntry *e = &a->items[i];

        /* Resolving by id is what makes this safe against a node destroyed mid-flight:
         * it simply stops being found, and the entry goes with it. */
        MuNode *n = mu_context_find_id(ctx, NULL, e->node_id);
        if (!n) {
            anim_remove_at(a, i);
            continue;
        }

        bool mx = mu_spring_step(&e->x, dt);
        bool my = mu_spring_step(&e->y, dt);
        bool mw = mu_spring_step(&e->w, dt);
        bool mh = mu_spring_step(&e->h, dt);

        /* Assigned directly rather than through mu_node_set_bounds, which would mark the
         * subtree layout-dirty every frame for a layout that is not going to run. Damage
         * tracking compares bounds against last frame, so the move is discovered anyway. */
        n->bounds = (MuRect){e->x.value, e->y.value, e->w.value, e->h.value};

        if (mx || my || mw || mh) {
            moving++;
            i++;
        } else {
            /* Finished, and the springs have snapped exactly onto the target, so the
             * final bounds are exact rather than a hair short of it. */
            anim_remove_at(a, i);
        }
    }
    return moving;
}
