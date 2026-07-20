#ifndef UI_ROM_LIST_H
#define UI_ROM_LIST_H

#include "lvgl.h"
#include "net_archive.h"
#include <stdbool.h>

typedef void (*ui_rom_list_back_cb_t)(void);

/* Builds a virtualized ROM list screen: `list` can have thousands of
 * entries, so a fixed small pool of row widgets is recycled and relabeled
 * as the user scrolls, rather than creating one widget per entry. Takes
 * ownership of `list` (frees it internally when the screen is torn down
 * via ui_rom_list_destroy). D-pad Up/Down scrolls, B/ESC calls on_back.
 * `thumb_repo` (may be NULL) is the libretro-thumbnails repo used to
 * best-effort fetch box art for the currently-selected entry. */
void ui_rom_list_build(lv_group_t *group, const char *title, RomList list,
                        const char *thumb_repo, ui_rom_list_back_cb_t on_back);

/* Call once per main loop iteration while this screen may be active —
 * fires the debounced box-art fetch for the current selection once it's
 * been stable for a bit (harmless no-op when this screen isn't active). */
void ui_rom_list_tick(void);

/* Opens the search keyboard overlay (only valid while this screen is the
 * active one — caller, e.g. main.c, is responsible for only calling this
 * when the rom list is actually showing). */
void ui_rom_list_open_search(void);

/* True while the search overlay is open — main.c uses this to decide what
 * the Y button does (open search vs. act as a direct backspace shortcut,
 * see ui_rom_list_search_backspace — navigating the on-screen keyboard
 * grid all the way to its tiny backspace key over D-pad was reported as
 * too tedious). */
bool ui_rom_list_search_is_open(void);

/* Deletes the character before the cursor in the search text box. No-op
 * if search isn't open. */
void ui_rom_list_search_backspace(void);

/* Frees the RomList this screen owns. Call before rebuilding another
 * screen over this one (e.g. from within on_back, before ui_emu_select
 * rebuilds), since lv_obj_clean() only destroys widgets, not our data. */
void ui_rom_list_destroy(void);

#endif
