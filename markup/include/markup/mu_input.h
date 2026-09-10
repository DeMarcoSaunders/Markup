#ifndef MU_INPUT_H
#define MU_INPUT_H

#include "mu_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MuPointerEvent {
    MuVec2 position;
    int button;
    bool pressed;
    bool released;
    bool drag; /* primary down + moved or repeat tick while captured */
} MuPointerEvent;

typedef struct MuKeyEvent {
    int key;
    bool pressed;
    bool repeat;
} MuKeyEvent;

/** Backend-neutral key ids (values match Raylib KeyboardKey for the raylib bridge). */
enum {
    MU_KEY_UNKNOWN = 0,
    MU_KEY_BACKSPACE = 259,
    MU_KEY_TAB = 258,
    MU_KEY_DELETE = 261,
    MU_KEY_RIGHT = 262,
    MU_KEY_LEFT = 263,
    MU_KEY_HOME = 268,
    MU_KEY_END = 269,
};

void mu_modal_push(MuContext *ctx, uint32_t modal_root_id);
void mu_modal_pop(MuContext *ctx);
uint32_t mu_modal_top(const MuContext *ctx);

void mu_input_clear_hovers(MuContext *ctx, MuNode *subtree);
MuNode *mu_input_node_at(MuContext *ctx, MuVec2 position);
void mu_input_update_hover(MuContext *ctx, MuVec2 mouse);
void mu_input_dispatch_pointer(MuContext *ctx, const MuPointerEvent *ev);
void mu_input_dispatch_key(MuContext *ctx, const MuKeyEvent *ev);
void mu_input_dispatch_char(MuContext *ctx, unsigned int codepoint);
void mu_focus_set(MuContext *ctx, uint32_t node_id);
void mu_focus_advance_tab(MuContext *ctx);

#ifdef __cplusplus
}
#endif

#endif
