#include "mu_input_internal.h"
#include <stdlib.h>

void mu_input_context_init(MuContext *ctx) {
    if (!ctx || ctx->input_seat) return;
    MuInputSeat *s = (MuInputSeat *)calloc(1, sizeof(MuInputSeat));
    ctx->input_seat = s;
}

void mu_input_context_shutdown(MuContext *ctx) {
    if (!ctx) return;
    free(ctx->input_seat);
    ctx->input_seat = NULL;
}

void mu_input_reset_page_ime_caret(MuContext *ctx) {
    MuInputSeat *s = ctx ? (MuInputSeat *)ctx->input_seat : NULL;
    if (!s) return;
    s->page_ime_caret_valid = false;
}

