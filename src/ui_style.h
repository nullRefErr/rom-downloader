#ifndef UI_STYLE_H
#define UI_STYLE_H

#include "lvgl.h"

/* Shared list-row look for both ui_emu_select.c and ui_rom_list.c — the
 * default theme's focus indicator (a subtle border on top of its own
 * blue-ish button background) wasn't distinct enough on-device per user
 * feedback ("blue on blue"). These styles strip the default theme
 * entirely and use a high-contrast selected look instead. */
void ui_style_init(void);

/* Strips default styling from `row` and attaches the shared normal/
 * selected looks — selected shows whenever LV_STATE_FOCUSED|FOCUS_KEY are
 * both set (toggle those states as before, the style just follows). */
void ui_style_apply_row(lv_obj_t *row);

#endif
