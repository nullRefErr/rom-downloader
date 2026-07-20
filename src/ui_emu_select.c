#include "ui_emu_select.h"
#include "ui_style.h"
#include "ui_chrome.h"
#include <stdio.h>
#include <stdbool.h>

/* Own key handling instead of relying on lv_group's native NEXT/PREV
 * cycling — the ROM list screen needs custom Up/Down handling to scroll a
 * virtualized/recycled row pool (can't put 500-2800 widgets in a group),
 * so both screens share one pattern: a single focused object per screen
 * that receives raw LV_KEY_UP/DOWN/ENTER via LV_EVENT_KEY. See
 * lvgl_glue.c's key mapping. */

static lv_obj_t *g_buttons[16];
static int g_available_idx[16]; /* EMU_TABLE index for each selectable button */
static int g_available_count;
static int g_selected; /* index into g_available_idx/g_buttons */
static ui_emu_select_cb_t g_on_select;

static void set_highlight(int pos, bool on) {
    if (pos < 0 || pos >= g_available_count) return;
    /* LV_STATE_FOCUS_KEY (not just FOCUSED) is what the default theme
     * actually uses for a visible keypad-navigation highlight — confirmed
     * by checking how lv_group's native focus applies state (lv_obj.c);
     * FOCUSED alone was invisible on-device. */
    lv_state_t state = LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY;
    if (on) lv_obj_add_state(g_buttons[pos], state);
    else lv_obj_remove_state(g_buttons[pos], state);
}

static void key_event_cb(lv_event_t *e) {
    uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_DOWN) {
        if (g_selected + 1 < g_available_count) {
            set_highlight(g_selected, false);
            g_selected++;
            set_highlight(g_selected, true);
        }
    } else if (key == LV_KEY_UP) {
        if (g_selected > 0) {
            set_highlight(g_selected, false);
            g_selected--;
            set_highlight(g_selected, true);
        }
    } else if (key == LV_KEY_ENTER) {
        if (g_on_select && g_available_count > 0) {
            g_on_select(&EMU_TABLE[g_available_idx[g_selected]]);
        }
    }
}

void ui_emu_select_build(lv_group_t *group, ui_emu_select_cb_t on_select) {
    g_on_select = on_select;
    g_available_count = 0;
    g_selected = 0;

    ui_chrome_build("Rom Downloader By AEY", "Up/Down: Move   A: Select   Start: Quit");

    lv_obj_t *list = lv_list_create(lv_screen_active());
    lv_obj_set_size(list, LV_PCT(100), 480 - UI_CHROME_HEADER_H - UI_CHROME_FOOTER_H);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, UI_CHROME_HEADER_H);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    for (int i = 0; i < EMU_TABLE_COUNT; i++) {
        const EmuEntry *e = &EMU_TABLE[i];
        char label[64];
        if (e->available) {
            snprintf(label, sizeof(label), "%s", e->label);
        } else {
            snprintf(label, sizeof(label), "%s (unavailable)", e->label);
        }
        lv_obj_t *btn = lv_list_add_button(list, NULL, label);
        lv_obj_set_style_text_align(btn, LV_TEXT_ALIGN_LEFT, 0);
        if (e->available && g_available_count < (int)(sizeof(g_buttons) / sizeof(g_buttons[0]))) {
            ui_style_apply_row(btn);
            g_buttons[g_available_count] = btn;
            g_available_idx[g_available_count] = i;
            g_available_count++;
        } else if (!e->available) {
            /* dead archive.org source: deliberately muted, not just the
             * default theme's disabled dimming (which reads too close to
             * the normal-row look now that rows have a solid background) */
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x000000), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(btn, lv_color_hex(0x555555), 0);
            lv_obj_add_state(btn, LV_STATE_DISABLED);
        }
    }

    if (g_available_count > 0) set_highlight(0, true);

    lv_group_add_obj(group, list);
    lv_obj_add_event_cb(list, key_event_cb, LV_EVENT_KEY, NULL);
}
