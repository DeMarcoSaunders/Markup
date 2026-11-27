#ifndef MU_INPUT_INTERNAL_H
#define MU_INPUT_INTERNAL_H

#include "../include/markup/mu_input.h"

typedef struct MuInputSeat {
    uint32_t pressed_target_id;
    MuSeatWheelFn wheel;
    void *wheel_user;
    MuSeatKeyFn key;
    void *key_user;
    MuSeatTouchFn touch;
    void *touch_user;
    MuPresentedFn on_presented;
    void *on_presented_user;
    MuClipboardBridge clipboard;
    MuImeCallbacks ime;
    MuRect page_ime_caret;
    bool page_ime_caret_valid;
    MuMarkupImeCaretFn markup_caret;
    void *markup_caret_user;
    char clipboard_buf[4096];
} MuInputSeat;

void mu_input_context_init(MuContext *ctx);
void mu_input_context_shutdown(MuContext *ctx);
void mu_input_reset_page_ime_caret(MuContext *ctx);

#endif