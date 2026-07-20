#ifndef UI_EMU_SELECT_H
#define UI_EMU_SELECT_H

#include "lvgl.h"
#include "emu_table.h"

typedef void (*ui_emu_select_cb_t)(const EmuEntry *emu);

/* Builds the emulator-select list on the active screen. D-pad Up/Down move
 * the highlight among available entries (dead archive.org sources are
 * shown grayed out and skipped, confirmed intended with the user), A
 * invokes on_select with the highlighted entry. */
void ui_emu_select_build(lv_group_t *group, ui_emu_select_cb_t on_select);

#endif
