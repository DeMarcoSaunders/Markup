#ifndef MU_INPUT_INTERNAL_H
#define MU_INPUT_INTERNAL_H

#include "../include/markup/mu_core.h"

void mu_input_context_init(MuContext *ctx);
void mu_input_context_shutdown(MuContext *ctx);
void mu_input_reset_page_ime_caret(MuContext *ctx);

#endif