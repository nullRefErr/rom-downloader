#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include "lvgl.h"
#include <stdbool.h>

typedef void (*ui_settings_back_cb_t)(void);

/* Settings screen: language, navigation sounds, archive.org account, and
 * clearing unfinished downloads. Reached with Select from the system list.
 *
 * Changing the language rebuilds this screen immediately, so the change is
 * visible at once rather than after a restart; every other screen is built
 * fresh when it is opened, so they pick it up on the way back. */
void ui_settings_build(lv_group_t *group, ui_settings_back_cb_t on_back);

/* Call every frame while this screen may be active. */
void ui_settings_tick(void);

/* True when the user picked the sources row; main.c owns screen switching. */
bool ui_settings_wants_sources(void);

#endif
