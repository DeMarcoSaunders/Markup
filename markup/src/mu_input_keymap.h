#ifndef MU_INPUT_KEYMAP_H
#define MU_INPUT_KEYMAP_H

/** Map an OS-neutral scancode/sym integer to MU_KEY_* (0 if unknown). */
int mu_key_from_sdl_sym(long sym);
int mu_key_from_raylib_key(int key);

#endif