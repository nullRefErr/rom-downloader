#include "ui_sources.h"
#include "ui_kb.h"
#include "ui_overlay.h"
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
static lv_obj_t *g_container;
static lv_group_t *g_group;
static ui_sources_back_cb_t g_on_back;
static int g_selected;
static int g_window_start;
static bool g_back_pending;

/* Edit overlay: a single field plus the on-screen keyboard. Torn down from
 * the tick, never from inside its own event handler — the pattern this
 * codebase settled on after mid-dispatch teardown froze the app. */
static UiOverlay g_edit;
static lv_obj_t *g_edit_ta;
static lv_obj_t *g_edit_kb;
static int g_edit_index = -1;
static uint32_t g_kb_last_sel = LV_BUTTONMATRIX_BUTTON_NONE;



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
        ui_overlay_request_close(&g_edit, true);
    } else if (code == LV_EVENT_CANCEL) {
        ui_overlay_request_close(&g_edit, false);
    }
}

static void open_edit(int index) {
    if (g_edit.obj) return;
    const EmuEntry *e = sources_get(index);
    if (!e) return;

    g_edit_index = index;
    g_kb_last_sel = LV_BUTTONMATRIX_BUTTON_NONE;

    ui_overlay_open(&g_edit);

    lv_obj_t *title = lv_label_create(g_edit.obj);
    lv_label_set_text(title, e->label);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    g_edit_ta = lv_textarea_create(g_edit.obj);
    lv_textarea_set_one_line(g_edit_ta, true);
    lv_textarea_set_text(g_edit_ta, sources_text(index));
    lv_obj_set_size(g_edit_ta, LV_PCT(96), 34);
    lv_obj_align(g_edit_ta, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_set_style_bg_color(g_edit_ta, lv_color_hex(0x111111), 0);
    lv_obj_set_style_text_color(g_edit_ta, lv_color_hex(0xffffff), 0);

    lv_obj_t *hint = lv_label_create(g_edit.obj);
    lv_label_set_text(hint, T("sources_edit_hint"));
    lv_obj_set_style_text_font(hint, &lv_font_ui_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_set_width(hint, LV_PCT(96));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 68);

    g_edit_kb = ui_kb_create(g_edit.obj, g_edit_ta, edit_kb_event_cb);
    g_kb_last_sel = LV_BUTTONMATRIX_BUTTON_NONE;

    ui_overlay_focus(&g_edit, g_edit_kb);
}

static void process_edit_close(void) {
    bool apply;
    if (!ui_overlay_take_close(&g_edit, &apply)) return;

    /* the textarea lives inside the overlay — read it before tearing down */
    if (apply && g_edit_ta && g_edit_index >= 0) {
        const char *text = lv_textarea_get_text(g_edit_ta);
        if (text && *text) {
            sources_set(g_edit_index, text);
            ui_chrome_toast(T("sources_saved"), STATUS_MS);
        }
    }

    g_edit_kb = NULL;
    g_edit_ta = NULL;
    ui_overlay_close(&g_edit, g_group);
    g_edit_index = -1;
    redraw();
}

bool ui_sources_editing(void) { return g_edit.obj != NULL; }

void ui_sources_backspace(void) {
    if (g_edit_ta) lv_textarea_delete_char(g_edit_ta);
}

void ui_sources_clear_field(void) {
    if (g_edit_ta) lv_textarea_set_text(g_edit_ta, "");
}

void ui_sources_toggle_available(void) {
    if (g_edit.obj) return;
    const EmuEntry *e = sources_get(g_selected);
    if (!e) return;
    sources_set_available(g_selected, !e->available);
    redraw();
}

void ui_sources_reset_defaults(void) {
    if (g_edit.obj) return;
    sources_reset();
    g_selected = 0;
    g_window_start = 0;
    redraw();
    ui_chrome_toast(T("sources_reset_done"), STATUS_MS);
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
    g_back_pending = false;
    g_edit = (UiOverlay){0};
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


    redraw();

    lv_group_add_obj(group, g_container);
    lv_obj_add_event_cb(g_container, key_event_cb, LV_EVENT_KEY, NULL);
}

void ui_sources_tick(void) {
    process_edit_close();
    ui_kb_update_highlight(g_edit_kb, &g_kb_last_sel);

    if (g_back_pending) {
        g_back_pending = false;
        if (g_on_back) g_on_back();
        return;
    }
}
