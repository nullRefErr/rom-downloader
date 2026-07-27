#ifndef UI_LOGIN_H
#define UI_LOGIN_H

#include "lvgl.h"
#include <stdbool.h>

/* archive.org sign-in overlay: the user types their OWN email and password
 * on the device. Reachable from a source that needs an account, which is
 * exactly where the wall is hit.
 *
 * Keyboard checkmark moves email -> password -> submit; B cancels. The
 * password field is masked. Nothing is stored except the session cookies
 * the server hands back (see net_login.h). */
void ui_login_open(lv_group_t *restore_group);

/* Call every frame; drives the deferred teardown and the submit step. */
void ui_login_tick(void);

bool ui_login_is_open(void);

/* Empties the focused field in one press — clearing a prefilled password
 * character by character on a d-pad is not a reasonable ask. */
void ui_login_clear_field(void);

/* Deletes the character before the cursor in the focused field, so the Y
 * button can double as backspace like it does in search. */
void ui_login_backspace(void);

#endif
