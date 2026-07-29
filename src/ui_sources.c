#include "ui_sources.h"
#include "ui_chrome.h"
#include "ui_style.h"
#include "lvgl_glue.h"
#include "sources.h"
#include "i18n.h"
#include <stdio.h>
#include <string.h>

#define ROWS_VISIBLE 7
#define STATUS_MS 2500

static lv_obj_t *g_rows[ROWS_VISIBLE];
static lv_obj_t *g_labels[ROWS_VISIBLE];
static lv_obj_t *g_status;
static lv_obj_t *g_container;
static lv_group_t *g_group;
static ui_sources_back_cb_t g_on_back;
static int g_selected;
static int g_window_start;
static uint32_t g_status_tick;
static bool g_status_showing;
static bool g_back_pending;

/* Edit overlay: a single field plus the on-screen keyboard. Torn down from
 * the tick, never from inside its own event handler — the pattern this
 * codebase settled on after mid-dispatch teardown froze the app. */
static lv_obj_t *g_edit_overlay;
static lv_group_t *g_edit_group;
static lv_obj_t *g_edit_ta;
static lv_obj_t *g_edit_kb;
static int g_edit_index = -1;
static bool g_edit_close_pending;
static bool g_edit_close_apply;
static uint32_t g_kb_last_sel = LV_BUTTONMATRIX_BUTTON_NONE;

static void set_status(const char *text) {
    lv_label_set_text(g_status, text);
    lv_obj_remove_flag(g_status, LV_OBJ_FLAG_HIDDEN);
    g_status_showing = true;
    g_status_tick = lv_tick_get();
}

static void update_kb_highlight(void) {
    if (!g_edit_kb) return;
    uint32_t sel = lv_buttonmatrix_get_selected_button(g_edit_kb);
    if (sel == g_kb_last_sel) return;
    if (g_kb_last_sel != LV_BUTTONMATRIX_BUTTON_NONE) {
        lv_buttonmatrix_clear_button_ctrl(g_edit_kb, g_kb_last_sel, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
    if (sel != LV_BUTTONMATRIX_BUTTON_NONE) {
        lv_buttonmatrix_set_button_ctrl(g_edit_kb, sel, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
    g_kb_last_sel = sel;
}

static void redraw(void) {
    int count = sources_count();
    for (int i = 0; i < ROWS_VISIBLE; i++) {
        int pos = g_window_start + i;
        if (pos < count) {
            const EmuEntry *e = sources_get(pos);
            char buf[420];
            /* Show where it points, since that is the thing being managed.
             * A disabled source is marked rather than hidden — it still has a
             * value worth seeing and re-enabling. */
            snprintf(buf, sizeof(buf), "%s%-18s %s",
                     e->available ? "" : "- ", e->label, sources_text(pos));
            lv_label_set_text(g_labels[i], buf);
            lv_obj_remove_flag(g_rows[i], LV_OBJ_FLAG_HIDDEN);

            lv_state_t st = LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY;
            if (pos == g_selected) lv_obj_add_state(g_rows[i], st);
            else lv_obj_remove_state(g_rows[i], st);
        } else {
            lv_obj_add_flag(g_rows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void edit_kb_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        g_edit_close_pending = true;
        g_edit_close_apply = true;
    } else if (code == LV_EVENT_CANCEL) {
        g_edit_close_pending = true;
        g_edit_close_apply = false;
    }
}

static void open_edit(int index) {
    if (g_edit_overlay) return;
    const EmuEntry *e = sources_get(index);
    if (!e) return;

    g_edit_index = index;
    g_kb_last_sel = LV_BUTTONMATRIX_BUTTON_NONE;

    g_edit_overlay = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_edit_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_edit_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_edit_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_edit_overlay, 0, 0);
    lv_obj_remove_flag(g_edit_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(g_edit_overlay);
    lv_label_set_text(title, e->label);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    g_edit_ta = lv_textarea_create(g_edit_overlay);
    lv_textarea_set_one_line(g_edit_ta, true);
    lv_textarea_set_text(g_edit_ta, sources_text(index));
    lv_obj_set_size(g_edit_ta, LV_PCT(96), 34);
    lv_obj_align(g_edit_ta, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_set_style_bg_color(g_edit_ta, lv_color_hex(0x111111), 0);
    lv_obj_set_style_text_color(g_edit_ta, lv_color_hex(0xffffff), 0);

    lv_obj_t *hint = lv_label_create(g_edit_overlay);
    lv_label_set_text(hint, T("sources_edit_hint"));
    lv_obj_set_style_text_font(hint, &lv_font_ui_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_set_width(hint, LV_PCT(96));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 68);

    g_edit_kb = lv_keyboard_create(g_edit_overlay);
    lv_keyboard_set_textarea(g_edit_kb, g_edit_ta);
    lv_obj_add_event_cb(g_edit_kb, edit_kb_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(g_edit_kb, edit_kb_event_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_set_style_bg_color(g_edit_kb, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_edit_kb, lv_color_hex(0x222222), LV_PART_ITEMS);
    lv_obj_set_style_text_color(g_edit_kb, lv_color_hex(0xffffff), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(g_edit_kb, lv_color_hex(0x2ecc40), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(g_edit_kb, lv_color_hex(0x000000), LV_PART_ITEMS | LV_STATE_CHECKED);

    g_edit_group = lv_group_create();
    lv_group_add_obj(g_edit_group, g_edit_kb);
    lvgl_glue_set_active_group(g_edit_group);
}

static void process_edit_close(void) {
    if (!g_edit_close_pending) return;
    g_edit_close_pending = false;

    if (g_edit_close_apply && g_edit_ta && g_edit_index >= 0) {
        const char *text = lv_textarea_get_text(g_edit_ta);
        if (text && *text) {
            sources_set(g_edit_index, text);
            set_status(T("sources_saved"));
        }
    }

    lvgl_glue_set_active_group(g_group);
    g_edit_kb = NULL;
    g_edit_ta = NULL;
    if (g_edit_overlay) {
        lv_obj_delete(g_edit_overlay);
        g_edit_overlay = NULL;
    }
    if (g_edit_group) {
        lv_group_delete(g_edit_group);
        g_edit_group = NULL;
    }
    g_edit_index = -1;
    redraw();
}

bool ui_sources_editing(void) { return g_edit_overlay != NULL; }

void ui_sources_backspace(void) {
    if (g_edit_ta) lv_textarea_delete_char(g_edit_ta);
}

void ui_sources_clear_field(void) {
    if (g_edit_ta) lv_textarea_set_text(g_edit_ta, "");
}

void ui_sources_toggle_available(void) {
    if (g_edit_overlay) return;
    const EmuEntry *e = sources_get(g_selected);
    if (!e) return;
    sources_set_available(g_selected, !e->available);
    redraw();
}

void ui_sources_reset_defaults(void) {
    if (g_edit_overlay) return;
    sources_reset();
    g_selected = 0;
    g_window_start = 0;
    redraw();
    set_status(T("sources_reset_done"));
}

static void key_event_cb(lv_event_t *e) {
    uint32_t key = lv_event_get_key(e);
    int count = sources_count();

    if (key == LV_KEY_DOWN) {
        if (g_selected + 1 < count) {
            g_selected++;
            if (g_selected - g_window_start >= ROWS_VISIBLE) g_window_start++;
            redraw();
        }
    } else if (key == LV_KEY_UP) {
        if (g_selected > 0) {
            g_selected--;
            if (g_selected < g_window_start) g_window_start--;
            redraw();
        }
    } else if (key == LV_KEY_ENTER) {
        open_edit(g_selected);
    } else if (key == LV_KEY_ESC) {
        g_back_pending = true; /* deferred, see ui_settings.c for why */
    }
}

void ui_sources_build(lv_group_t *group, ui_sources_back_cb_t on_back) {
    g_group = group;
    g_on_back = on_back;
    g_status_showing = false;
    g_back_pending = false;
    g_edit_overlay = NULL;
    g_edit_group = NULL;
    g_edit_close_pending = false;
    g_edit_index = -1;
    if (g_selected >= sources_count()) g_selected = 0;

    ui_chrome_build(T("sources_title"), T("hint_sources"));

    lv_obj_t *screen = lv_screen_active();

    g_container = lv_obj_create(screen);
    lv_obj_set_size(g_container, 624, 480 - UI_CHROME_HEADER_H - UI_CHROME_FOOTER_H - 26);
    lv_obj_align(g_container, LV_ALIGN_TOP_LEFT, 6, UI_CHROME_HEADER_H + 4);
    lv_obj_set_style_bg_opa(g_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_container, 0, 0);
    lv_obj_set_flex_flow(g_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_container, 4, 0);
    lv_obj_set_style_pad_all(g_container, 0, 0);

    for (int i = 0; i < ROWS_VISIBLE; i++) {
        lv_obj_t *btn = lv_button_create(g_container);
        lv_obj_set_size(btn, LV_PCT(100), 40);
        ui_style_apply_row(btn);
        lv_obj_t *label = lv_label_create(btn);
        lv_obj_set_style_text_font(label, &lv_font_ui_14, 0); /* URLs are long */
        lv_obj_set_width(label, LV_PCT(100));
        lv_obj_set_height(label, 22);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
        g_rows[i] = btn;
        g_labels[i] = label;
    }

    g_status = lv_label_create(screen);
    lv_obj_set_style_text_color(g_status, lv_color_hex(0x2ecc40), 0);
    lv_obj_set_style_text_font(g_status, &lv_font_ui_16, 0);
    lv_obj_align(g_status, LV_ALIGN_BOTTOM_LEFT, 8, -UI_CHROME_FOOTER_H - 2);
    lv_obj_add_flag(g_status, LV_OBJ_FLAG_HIDDEN);

    redraw();

    lv_group_add_obj(group, g_container);
    lv_obj_add_event_cb(g_container, key_event_cb, LV_EVENT_KEY, NULL);
}

void ui_sources_tick(void) {
    process_edit_close();
    update_kb_highlight();

    if (g_back_pending) {
        g_back_pending = false;
        if (g_on_back) g_on_back();
        return;
    }
    if (g_status_showing && lv_tick_elaps(g_status_tick) >= STATUS_MS) {
        g_status_showing = false;
        lv_obj_add_flag(g_status, LV_OBJ_FLAG_HIDDEN);
    }
}
