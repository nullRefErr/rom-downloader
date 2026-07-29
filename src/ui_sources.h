#ifndef UI_SOURCES_H
#define UI_SOURCES_H

#include "lvgl.h"
#include <stdbool.h>

typedef void (*ui_sources_back_cb_t)(void);

/* Source editor, reached from Settings.
 *
 * Lists every system with where its roms currently come from, and lets that
 * be changed on the device — an archive.org item name, or a URL to a listing
 * someone hosts themselves. Sources are taken down or moved behind logins
 * without warning, and until now the only fix was shipping a new build.
 *
 * A edits the selected source, X enables/disables it, Y restores the
 * built-in defaults, B goes back. */
void ui_sources_build(lv_group_t *group, ui_sources_back_cb_t on_back);

/* Call every frame while this screen may be active. */
void ui_sources_tick(void);

/* True while the edit keyboard is up — main.c routes Y to backspace then,
 * matching how search and sign-in behave. */
bool ui_sources_editing(void);
void ui_sources_backspace(void);
void ui_sources_clear_field(void);

/* X outside editing: enable/disable the selected source. A disabled one
 * stays listed but is not offered on the system list. */
void ui_sources_toggle_available(void);

/* Y outside editing: restore the built-in source list. */
void ui_sources_reset_defaults(void);

#endif
