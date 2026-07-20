#include "ui_rom_list.h"
#include "ui_style.h"
#include "ui_chrome.h"
#include "thumbnail.h"
#include "lvgl_glue.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/* Real archive.org catalogs run 500-2800+ entries (confirmed during
 * planning) — far too many to create one LVGL widget per row. Instead we
 * keep a small fixed pool of row widgets and relabel them as the user
 * scrolls through the underlying RomList, recycled-list style. */
#define ROWS_VISIBLE 9
#define THUMB_DEBOUNCE_MS 400
#define MARQUEE_DEBOUNCE_MS 200
#define MAX_FILTER_ITEMS 4096

static lv_obj_t *g_rows[ROWS_VISIBLE];
static lv_obj_t *g_row_labels[ROWS_VISIBLE];
static lv_obj_t *g_empty_label;
static lv_obj_t *g_thumb_img;
static lv_obj_t *g_info_label;
static lv_obj_t *g_page_label;
static lv_obj_t *g_container;
static RomList g_list;
static int g_selected;     /* position within the effective (filtered or not) list */
static int g_window_start; /* position of the item shown in g_rows[0] */
static const char *g_thumb_repo;
static int g_thumb_pending_idx = -1; /* index waiting on the debounce timer, -1 = none */
static uint32_t g_thumb_dirty_tick;
static ui_rom_list_back_cb_t g_on_back;
static lv_group_t *g_group;

/* Every row scrolling its (possibly overflowing) name continuously was a
 * real perf problem, not just visual noise: this driver only supports
 * full-screen redraw+flush (see lvgl_glue.c), so ANY row's marquee
 * animation ticking forces a full 640x480 software redraw + memcpy +
 * present every 16ms — with up to 9 rows animating at once this kept the
 * screen redrawing continuously even at rest, which is what read as
 * general sluggishness/low fps. Now only the selected row ever animates,
 * and only after it's been selected for a bit (matches the thumbnail
 * debounce pattern already used for the same "don't redo work on every
 * transient keystroke" reason). */
static int g_marquee_row = -1;    /* visible row index (0..ROWS_VISIBLE-1) currently animating, -1 = none */
static int g_marquee_pending_row; /* visible row index waiting on the debounce, -1 = none */
static uint32_t g_marquee_dirty_tick;

/* search filter: a subset of g_list, by position */
static int g_filter_idx[MAX_FILTER_ITEMS];
static int g_filter_count;
static bool g_filter_active;
static lv_obj_t *g_search_overlay;
static lv_group_t *g_search_group;
static lv_obj_t *g_kb;
static uint32_t g_kb_last_sel = LV_BUTTONMATRIX_BUTTON_NONE;

/* The keyboard's default per-button FOCUSED/FOCUS_KEY style (driven by
 * lv_group's object-level focus state cascading down to whichever button
 * lv_buttonmatrix considers "selected") wasn't visibly rendering on-device
 * despite matching the same style-selector pattern that worked for our own
 * row buttons — unconfirmed why, and not worth more guessing given this
 * device's history of state/style surprises. LV_STATE_CHECKED sidesteps
 * the whole question: it's read directly from a per-button ctrl bit we set
 * ourselves (lv_buttonmatrix_set_button_ctrl), not derived from object
 * focus state, so it's rendered unconditionally by lv_buttonmatrix's own
 * draw code — a more direct, verifiable mechanism. */
static void update_keyboard_highlight(void) {
    if (!g_kb) return;
    uint32_t sel = lv_buttonmatrix_get_selected_button(g_kb);
    if (sel == g_kb_last_sel) return;
    if (g_kb_last_sel != LV_BUTTONMATRIX_BUTTON_NONE) {
        lv_buttonmatrix_clear_button_ctrl(g_kb, g_kb_last_sel, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
    if (sel != LV_BUTTONMATRIX_BUTTON_NONE) {
        lv_buttonmatrix_set_button_ctrl(g_kb, sel, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
    g_kb_last_sel = sel;
}

static void update_marquee(void) {
    if (g_marquee_pending_row < 0) return;
    if (lv_tick_elaps(g_marquee_dirty_tick) < MARQUEE_DEBOUNCE_MS) return;
    int row = g_marquee_pending_row;
    g_marquee_pending_row = -1;
    lv_label_set_long_mode(g_row_labels[row], LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    g_marquee_row = row;
}

/* archive.org file names can carry an internal path prefix (e.g.
 * "CHD-PSX-EUR/007 - ... .chd" for the PSX collection) — kept as-is in
 * RomEntry.name since Phase 4's download URL needs the full path, but the
 * list should only show the leaf name. */
static const char *display_name(const char *full_name) {
    const char *slash = strrchr(full_name, '/');
    return slash ? slash + 1 : full_name;
}

static int eff_count(void) { return g_filter_active ? g_filter_count : g_list.count; }
static int eff_data_idx(int pos) { return g_filter_active ? g_filter_idx[pos] : pos; }

static void format_size(unsigned long bytes, char *out, size_t outsz) {
    if (bytes >= 1024UL * 1024 * 1024) snprintf(out, outsz, "%.1f GB", bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= 1024UL * 1024) snprintf(out, outsz, "%.1f MB", bytes / (1024.0 * 1024));
    else if (bytes >= 1024) snprintf(out, outsz, "%.1f KB", bytes / 1024.0);
    else if (bytes > 0) snprintf(out, outsz, "%lu B", bytes);
    else out[0] = '\0';
}

static void redraw(void) {
    int count = eff_count();
    int selected_row = -1;
    for (int i = 0; i < ROWS_VISIBLE; i++) {
        int pos = g_window_start + i;
        if (pos < count) {
            int idx = eff_data_idx(pos);
            lv_obj_remove_flag(g_rows[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(g_row_labels[i], display_name(g_list.items[idx].name));
            /* static (no animation) by default — see update_marquee() for
             * why only the selected row, after a delay, gets to scroll */
            lv_label_set_long_mode(g_row_labels[i], LV_LABEL_LONG_MODE_DOTS);
            /* LV_STATE_FOCUS_KEY, not just FOCUSED — see ui_emu_select.c */
            lv_state_t state = LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY;
            if (pos == g_selected) {
                lv_obj_add_state(g_rows[i], state);
                selected_row = i;
            } else {
                lv_obj_remove_state(g_rows[i], state);
            }
        } else {
            lv_obj_add_flag(g_rows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    g_marquee_row = -1;
    g_marquee_pending_row = selected_row;
    g_marquee_dirty_tick = lv_tick_get();

    if (count > 0) {
        int idx = eff_data_idx(g_selected);
        char sizebuf[32];
        format_size(g_list.items[idx].size, sizebuf, sizeof(sizebuf));
        lv_label_set_text(g_info_label, sizebuf);
    } else {
        lv_label_set_text(g_info_label, "");
    }

    char pagebuf[32];
    int page = count > 0 ? (g_selected / ROWS_VISIBLE) + 1 : 0;
    int pages = count > 0 ? (count + ROWS_VISIBLE - 1) / ROWS_VISIBLE : 0;
    snprintf(pagebuf, sizeof(pagebuf), "%d/%d", page, pages);
    lv_label_set_text(g_page_label, pagebuf);

    /* Cache check is fast (local file, no network) so it happens on every
     * selection change immediately — only the actual network fetch (on a
     * cache miss) is debounced, in ui_rom_list_tick(). Previously the
     * cache check was ALSO debounced, so even a re-visited, already-cached
     * cover waited out the same 400ms delay meant to stop a wget-per-
     * keystroke storm while scrolling — that's what read as "still slow"
     * even though the file was already on disk. */
    int idx = count > 0 ? eff_data_idx(g_selected) : -1;
    bool cached = idx >= 0 && thumbnail_try_cache(g_thumb_img, g_thumb_repo, g_list.items[idx].name);
    if (cached) {
        g_thumb_pending_idx = -1;
    } else {
        g_thumb_pending_idx = idx;
        g_thumb_dirty_tick = lv_tick_get();
    }
}

static void apply_filter(const char *query) {
    g_filter_active = (query[0] != '\0');
    g_filter_count = 0;
    if (g_filter_active) {
        char qlow[64];
        int i;
        for (i = 0; query[i] && i < (int)sizeof(qlow) - 1; i++) {
            char c = query[i];
            qlow[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 'a' - 'A') : c;
        }
        qlow[i] = '\0';

        for (int di = 0; di < g_list.count && g_filter_count < MAX_FILTER_ITEMS; di++) {
            char namelow[160];
            const char *n = display_name(g_list.items[di].name);
            int j;
            for (j = 0; n[j] && j < (int)sizeof(namelow) - 1; j++) {
                char c = n[j];
                namelow[j] = (c >= 'A' && c <= 'Z') ? (char)(c + 'a' - 'A') : c;
            }
            namelow[j] = '\0';
            if (strstr(namelow, qlow)) g_filter_idx[g_filter_count++] = di;
        }
    }
    g_selected = 0;
    g_window_start = 0;
    redraw();
}

/* Closing must NOT happen synchronously inside the keyboard's own event
 * handler: deleting an object from within its own callback leaves LVGL's
 * indev dispatch code (still unwinding up the call stack, still holding a
 * pointer to the object we just freed) touching freed memory right after
 * we return — confirmed on-device as the cause of a full app freeze after
 * closing search with B. Deferred instead: the handler only sets a flag,
 * actual cleanup happens in ui_rom_list_tick(), safely outside any event
 * dispatch. */
static bool g_search_close_pending;
static bool g_search_close_apply;

static void keyboard_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        g_search_close_pending = true;
        g_search_close_apply = true;
    } else if (code == LV_EVENT_CANCEL) {
        g_search_close_pending = true;
        g_search_close_apply = false;
    }
}

static void process_pending_search_close(void) {
    if (!g_search_close_pending) return;
    g_search_close_pending = false;

    if (g_search_close_apply && g_search_overlay) {
        lv_obj_t *ta = lv_obj_get_child(g_search_overlay, 0);
        apply_filter(lv_textarea_get_text(ta));
    }
    lvgl_glue_set_active_group(g_group);
    g_kb = NULL;
    if (g_search_overlay) {
        lv_obj_delete(g_search_overlay);
        g_search_overlay = NULL;
    }
    if (g_search_group) {
        lv_group_delete(g_search_group);
        g_search_group = NULL;
    }
}

void ui_rom_list_open_search(void) {
    if (g_search_overlay) return;

    g_search_overlay = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_search_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_search_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_search_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_search_overlay, 0, 0);
    lv_obj_remove_flag(g_search_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ta = lv_textarea_create(g_search_overlay); /* child 0 — see process_pending_search_close() */
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_text(ta, ""); /* always start empty, regardless of any previous search */
    lv_textarea_set_placeholder_text(ta, "Search roms...");
    lv_obj_set_size(ta, LV_PCT(96), 40);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_border_color(ta, lv_color_hex(0x555555), 0);

    g_kb = lv_keyboard_create(g_search_overlay);
    g_kb_last_sel = LV_BUTTONMATRIX_BUTTON_NONE;
    lv_keyboard_set_textarea(g_kb, ta);
    lv_obj_add_event_cb(g_kb, keyboard_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(g_kb, keyboard_event_cb, LV_EVENT_CANCEL, NULL);
    /* the default per-button FOCUSED/FOCUS_KEY style wasn't visibly
     * rendering on-device — see update_keyboard_highlight() for why we use
     * LV_STATE_CHECKED (a directly-controlled ctrl bit) instead. */
    lv_obj_set_style_bg_color(g_kb, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_kb, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_kb, lv_color_hex(0x222222), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(g_kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(g_kb, lv_color_hex(0xffffff), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(g_kb, lv_color_hex(0xffffff), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(g_kb, lv_color_hex(0x000000), LV_PART_ITEMS | LV_STATE_CHECKED);

    g_search_group = lv_group_create();
    lv_group_add_obj(g_search_group, g_kb);
    lvgl_glue_set_active_group(g_search_group);
}

bool ui_rom_list_search_is_open(void) {
    return g_search_overlay != NULL;
}

void ui_rom_list_search_backspace(void) {
    if (!g_search_overlay) return;
    lv_obj_t *ta = lv_obj_get_child(g_search_overlay, 0);
    lv_textarea_delete_char(ta);
}

static void key_event_cb(lv_event_t *e) {
    uint32_t key = lv_event_get_key(e);
    int count = eff_count();
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
    } else if (key == LV_KEY_RIGHT) {
        if (count > 0) {
            g_selected = g_selected + ROWS_VISIBLE < count ? g_selected + ROWS_VISIBLE : count - 1;
            g_window_start = (g_selected / ROWS_VISIBLE) * ROWS_VISIBLE;
            redraw();
        }
    } else if (key == LV_KEY_LEFT) {
        if (count > 0) {
            g_selected = g_selected - ROWS_VISIBLE > 0 ? g_selected - ROWS_VISIBLE : 0;
            g_window_start = (g_selected / ROWS_VISIBLE) * ROWS_VISIBLE;
            redraw();
        }
    } else if (key == LV_KEY_ESC) {
        if (g_filter_active) {
            apply_filter(""); /* first Back clears the search, second one leaves */
        } else if (g_on_back) {
            g_on_back();
        }
    }
    /* LV_KEY_ENTER (download trigger) is Phase 4 — not wired yet. */
}

void ui_rom_list_build(lv_group_t *group, const char *title, RomList list,
                        const char *thumb_repo, ui_rom_list_back_cb_t on_back) {
    g_list = list;
    g_selected = 0;
    g_window_start = 0;
    g_thumb_repo = thumb_repo;
    g_thumb_pending_idx = -1;
    g_on_back = on_back;
    g_group = group;
    g_filter_active = false;
    g_filter_count = 0;
    g_search_overlay = NULL;
    g_search_group = NULL;
    g_marquee_row = -1;
    g_marquee_pending_row = -1;

    char header[80];
    if (g_list.ok) {
        snprintf(header, sizeof(header), "%s", title);
    } else {
        snprintf(header, sizeof(header), "%s (source unavailable)", title);
    }
    ui_chrome_build(header, "Up/Down: Move  Left/Right: Page  Y: Search  A: DL  B: Back");

    lv_obj_t *screen = lv_screen_active();

    /* box art panel, top-right — fixed-size frame so layout doesn't jump
     * around as images of different aspect ratios load in */
    lv_obj_t *thumb_frame = lv_obj_create(screen);
    lv_obj_set_size(thumb_frame, 140, 140);
    lv_obj_align(thumb_frame, LV_ALIGN_TOP_RIGHT, -4, UI_CHROME_HEADER_H + 4);
    lv_obj_set_style_bg_color(thumb_frame, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(thumb_frame, 1, 0);
    lv_obj_set_style_border_color(thumb_frame, lv_color_hex(0x444444), 0);
    lv_obj_set_style_radius(thumb_frame, 4, 0);
    lv_obj_remove_flag(thumb_frame, LV_OBJ_FLAG_SCROLLABLE);

    g_thumb_img = lv_image_create(thumb_frame);
    lv_obj_center(g_thumb_img);
    lv_obj_add_flag(g_thumb_img, LV_OBJ_FLAG_HIDDEN);

    g_info_label = lv_label_create(screen);
    lv_obj_set_style_text_color(g_info_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_style_text_font(g_info_label, &lv_font_montserrat_16, 0);
    lv_obj_align(g_info_label, LV_ALIGN_TOP_RIGHT, -4, UI_CHROME_HEADER_H + 148);

    g_page_label = lv_label_create(screen);
    lv_obj_set_style_text_color(g_page_label, lv_color_hex(0x777777), 0);
    lv_obj_set_style_text_font(g_page_label, &lv_font_montserrat_16, 0);
    lv_obj_align(g_page_label, LV_ALIGN_TOP_RIGHT, -4, UI_CHROME_HEADER_H + 168);

    g_container = lv_obj_create(screen);
    lv_obj_set_size(g_container, 480, 480 - UI_CHROME_HEADER_H - UI_CHROME_FOOTER_H);
    lv_obj_align(g_container, LV_ALIGN_TOP_LEFT, 0, UI_CHROME_HEADER_H);
    lv_obj_set_style_bg_opa(g_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_container, 0, 0);
    lv_obj_set_flex_flow(g_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_container, 2, 0);
    lv_obj_set_style_pad_all(g_container, 0, 0);

    for (int i = 0; i < ROWS_VISIBLE; i++) {
        /* explicit fixed height on both — without it, neither DOTS nor
         * SCROLL_CIRCULAR long-mode has a fixed size to "keep": the label
         * (and the button auto-sizing around it) just wrapped to 2 lines
         * for any name wider than the row instead of clipping/scrolling
         * within one. */
        lv_obj_t *btn = lv_button_create(g_container);
        lv_obj_set_size(btn, LV_PCT(100), 40);
        ui_style_apply_row(btn);
        lv_obj_t *label = lv_label_create(btn);
        lv_obj_set_width(label, LV_PCT(100));
        lv_obj_set_height(label, 24);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
        g_rows[i] = btn;
        g_row_labels[i] = label;
    }

    g_empty_label = lv_label_create(screen);
    lv_obj_set_style_text_color(g_empty_label, lv_color_hex(0xffffff), 0);
    lv_obj_align(g_empty_label, LV_ALIGN_CENTER, -80, 0);
    if (g_list.count == 0) {
        lv_label_set_text(g_empty_label, g_list.ok ? "No matching roms" : "Source unavailable right now");
    } else {
        lv_obj_add_flag(g_empty_label, LV_OBJ_FLAG_HIDDEN);
    }

    redraw();

    lv_group_add_obj(group, g_container);
    lv_obj_add_event_cb(g_container, key_event_cb, LV_EVENT_KEY, NULL);
}

void ui_rom_list_tick(void) {
    process_pending_search_close();
    update_keyboard_highlight();
    update_marquee();

    if (g_thumb_pending_idx < 0) return;
    if (lv_tick_elaps(g_thumb_dirty_tick) < THUMB_DEBOUNCE_MS) return;

    int idx = g_thumb_pending_idx;
    g_thumb_pending_idx = -1;
    if (idx >= 0 && idx < g_list.count) {
        thumbnail_fetch_network(g_thumb_img, g_thumb_repo, g_list.items[idx].name);
    }
}

void ui_rom_list_destroy(void) {
    g_thumb_pending_idx = -1;
    g_search_close_pending = false;
    g_kb = NULL;
    g_kb_last_sel = LV_BUTTONMATRIX_BUTTON_NONE;
    /* safe to delete synchronously here — called from main.c's screen
     * teardown, not from inside an LVGL event callback like the search
     * overlay's own close path has to be. */
    if (g_search_overlay) {
        lv_obj_delete(g_search_overlay);
        g_search_overlay = NULL;
    }
    if (g_search_group) {
        lv_group_delete(g_search_group);
        g_search_group = NULL;
    }
    net_archive_free(&g_list);
}
