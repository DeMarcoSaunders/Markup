#include "../include/markup/mu_input.h"
#include "mu_input_keymap.h"

int mu_key_from_sdl_sym(long sym) {
    /* SDL3 key symbols — only the subset we name in MU_KEY_*. */
#ifdef SDLK_UNKNOWN
    /* Included from SDL in mu_sdl.c may define these; this TU is backend-neutral
     * and uses numeric constants matching SDL_Keycode values. */
#endif
    if (sym >= 32 && sym <= 126) {
        /* Printable ASCII maps to itself for letters/digits (raylib-style). */
        if ((sym >= 'a' && sym <= 'z')) return (int)sym - 32;
        if ((sym >= 'A' && sym <= 'Z') || (sym >= '0' && sym <= '9')) return (int)sym;
    }
    switch ((unsigned long)sym) {
    case 32u: return MU_KEY_SPACE;
    case 256u: return MU_KEY_ESCAPE;
    case 257u: return MU_KEY_ENTER;
    case 258u: return MU_KEY_TAB;
    case 259u: return MU_KEY_BACKSPACE;
    case 261u: return MU_KEY_DELETE;
    case 262u: return MU_KEY_RIGHT;
    case 263u: return MU_KEY_LEFT;
    case 264u: return MU_KEY_DOWN;
    case 265u: return MU_KEY_UP;
    case 266u: return MU_KEY_PAGE_UP;
    case 267u: return MU_KEY_PAGE_DOWN;
    case 268u: return MU_KEY_HOME;
    case 269u: return MU_KEY_END;
    case 290u: return MU_KEY_F1;
    case 291u: return MU_KEY_F2;
    case 292u: return MU_KEY_F3;
    case 293u: return MU_KEY_F4;
    case 294u: return MU_KEY_F5;
    case 295u: return MU_KEY_F6;
    case 296u: return MU_KEY_F7;
    case 297u: return MU_KEY_F8;
    case 298u: return MU_KEY_F9;
    case 299u: return MU_KEY_F10;
    case 300u: return MU_KEY_F11;
    case 301u: return MU_KEY_F12;
    default: return MU_KEY_UNKNOWN;
    }
}

int mu_key_from_raylib_key(int key) {
    if (key >= MU_KEY_A && key <= MU_KEY_Z) return key;
    if (key >= MU_KEY_0 && key <= MU_KEY_9) return key;
    switch (key) {
    case MU_KEY_SPACE:
    case MU_KEY_ESCAPE:
    case MU_KEY_ENTER:
    case MU_KEY_TAB:
    case MU_KEY_BACKSPACE:
    case MU_KEY_DELETE:
    case MU_KEY_RIGHT:
    case MU_KEY_LEFT:
    case MU_KEY_DOWN:
    case MU_KEY_UP:
    case MU_KEY_PAGE_UP:
    case MU_KEY_PAGE_DOWN:
    case MU_KEY_HOME:
    case MU_KEY_END:
    case MU_KEY_F1:
    case MU_KEY_F2:
    case MU_KEY_F3:
    case MU_KEY_F4:
    case MU_KEY_F5:
    case MU_KEY_F6:
    case MU_KEY_F7:
    case MU_KEY_F8:
    case MU_KEY_F9:
    case MU_KEY_F10:
    case MU_KEY_F11:
    case MU_KEY_F12:
        return key;
    default:
        return MU_KEY_UNKNOWN;
    }
}