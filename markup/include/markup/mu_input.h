#ifndef MU_INPUT_H
#define MU_INPUT_H

#include "mu_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pointer buttons (0 = primary). */
enum {
    MU_BUTTON_LEFT = 0,
    MU_BUTTON_MIDDLE = 1,
    MU_BUTTON_RIGHT = 2,
};

typedef enum MuMod {
    MU_MOD_SHIFT = 1u << 0,
    MU_MOD_CTRL = 1u << 1,
    MU_MOD_ALT = 1u << 2,
    MU_MOD_META = 1u << 3,
} MuMod;

typedef enum MuCursor {
    MU_CURSOR_DEFAULT = 0,
    MU_CURSOR_POINTER,
    MU_CURSOR_TEXT,
    MU_CURSOR_CROSSHAIR,
    MU_CURSOR_WAIT,
    MU_CURSOR_NOT_ALLOWED,
} MuCursor;

typedef struct MuPointerEvent {
    MuVec2 position;
    int button;
    bool pressed;
    bool released;
    bool drag; /* primary down + moved or repeat tick while captured */
    uint32_t modifiers;
} MuPointerEvent;

typedef struct MuKeyEvent {
    int key;
    bool pressed;
    bool released;
    bool repeat;
    uint32_t modifiers;
} MuKeyEvent;

typedef struct MuWheelEvent {
    MuVec2 position;
    float delta_x;
    float delta_y;
    uint32_t modifiers;
} MuWheelEvent;

typedef enum MuTouchPhase {
    MU_TOUCH_DOWN = 0,
    MU_TOUCH_MOVE,
    MU_TOUCH_UP,
    MU_TOUCH_CANCEL,
} MuTouchPhase;

typedef struct MuTouchEvent {
    int id;
    MuVec2 position;
    MuTouchPhase phase;
    uint32_t modifiers;
} MuTouchEvent;

/** Key ids align with Raylib KeyboardKey where noted (raylib bridge). */
enum {
    MU_KEY_UNKNOWN = 0,
    MU_KEY_SPACE = 32,
    MU_KEY_ESCAPE = 256,
    MU_KEY_ENTER = 257,
    MU_KEY_TAB = 258,
    MU_KEY_BACKSPACE = 259,
    MU_KEY_DELETE = 261,
    MU_KEY_RIGHT = 262,
    MU_KEY_LEFT = 263,
    MU_KEY_DOWN = 264,
    MU_KEY_UP = 265,
    MU_KEY_PAGE_UP = 266,
    MU_KEY_PAGE_DOWN = 267,
    MU_KEY_HOME = 268,
    MU_KEY_END = 269,
    MU_KEY_F1 = 290,
    MU_KEY_F2 = 291,
    MU_KEY_F3 = 292,
    MU_KEY_F4 = 293,
    MU_KEY_F5 = 294,
    MU_KEY_F6 = 295,
    MU_KEY_F7 = 296,
    MU_KEY_F8 = 297,
    MU_KEY_F9 = 298,
    MU_KEY_F10 = 299,
    MU_KEY_F11 = 300,
    MU_KEY_F12 = 301,
    /* Letter / digit physical keys when no text input (ASCII uppercase). */
    MU_KEY_A = 65,
    MU_KEY_Z = 90,
    MU_KEY_0 = 48,
    MU_KEY_9 = 57,
};

typedef bool (*MuSeatWheelFn)(MuContext *ctx, const MuWheelEvent *ev, void *user);
typedef bool (*MuSeatKeyFn)(MuContext *ctx, const MuKeyEvent *ev, void *user);
typedef bool (*MuSeatTouchFn)(MuContext *ctx, const MuTouchEvent *ev, void *user);

typedef struct MuImeCallbacks {
    void (*set_preedit)(void *user, const char *utf8, int caret_begin, int caret_end);
    void (*commit)(void *user, const char *utf8);
    void (*cancel)(void *user);
    void *user;
} MuImeCallbacks;

typedef struct MuClipboardBridge {
    bool (*set_text)(void *user, const char *utf8);
    const char *(*get_text)(void *user);
    void *user;
} MuClipboardBridge;

void mu_modal_push(MuContext *ctx, uint32_t modal_root_id);
void mu_modal_pop(MuContext *ctx);
uint32_t mu_modal_top(const MuContext *ctx);

void mu_input_clear_hovers(MuContext *ctx, MuNode *subtree);
MuNode *mu_input_node_at(MuContext *ctx, MuVec2 position);
void mu_input_update_hover(MuContext *ctx, MuVec2 mouse);

/** Held modifier state for the seat (updated by the host each frame). */
void mu_input_set_modifiers(MuContext *ctx, uint32_t mods);
uint32_t mu_input_modifiers(const MuContext *ctx);

/** Cursor while pointer is over `hovered_id` (see mu_node_set_cursor). */
MuCursor mu_input_cursor(const MuContext *ctx);

void mu_node_set_cursor(MuNode *node, MuCursor cursor);

void mu_input_set_wheel_handler(MuContext *ctx, MuSeatWheelFn fn, void *user);
void mu_input_set_key_handler(MuContext *ctx, MuSeatKeyFn fn, void *user);
void mu_input_set_touch_handler(MuContext *ctx, MuSeatTouchFn fn, void *user);

void mu_input_set_clipboard(MuContext *ctx, const MuClipboardBridge *bridge);
bool mu_clipboard_set_text(MuContext *ctx, const char *text);
const char *mu_clipboard_get_text(MuContext *ctx);

void mu_input_set_ime(MuContext *ctx, const MuImeCallbacks *ime);

/**
 * Page / host-authored caret in page coordinates (e.g. a web view's selection).
 * Cleared at each `mu_frame_begin` unless set again that frame.
 */
void mu_input_set_ime_caret_rect(MuContext *ctx, MuRect rect);

/** Resolver for focused Markup textinput caret (installed by `mu_widgets_basic_register`). */
typedef bool (*MuMarkupImeCaretFn)(MuContext *ctx, MuRect *out, void *user);
void mu_input_set_markup_ime_caret(MuContext *ctx, MuMarkupImeCaretFn fn, void *user);

/** Caret for the OS IME: page rect when set, else focused Markup textinput, else false. */
bool mu_input_get_ime_caret_rect(const MuContext *ctx, MuRect *out);

void mu_input_dispatch_pointer(MuContext *ctx, const MuPointerEvent *ev);
void mu_input_dispatch_key(MuContext *ctx, const MuKeyEvent *ev);
void mu_input_dispatch_char(MuContext *ctx, unsigned int codepoint);
/** Seat wheel: optional handler, then default scroll-widget routing. */
void mu_input_dispatch_wheel(MuContext *ctx, const MuWheelEvent *ev);
void mu_input_dispatch_touch(MuContext *ctx, const MuTouchEvent *ev);

/** Caret rect for one Markup textinput node (does not include page-authored caret). */
void mu_textinput_get_caret_rect(const MuNode *input, MuRect *out);

void mu_focus_set(MuContext *ctx, uint32_t node_id);
void mu_focus_advance_tab(MuContext *ctx);

typedef void (*MuPresentedFn)(MuContext *ctx, void *user, uint64_t frame_index, float dt_seconds);
void mu_context_set_on_presented(MuContext *ctx, MuPresentedFn fn, void *user);
/** Call after the host has presented the frame (SDL_RenderPresent, etc.). */
void mu_frame_presented(MuContext *ctx);

#ifdef __cplusplus
}
#endif

#endif