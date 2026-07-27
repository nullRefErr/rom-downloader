#include "ui_rom_list.h"
#include "ui_style.h"
#include "ui_chrome.h"
#include "thumbnail.h"
#include "lvgl_glue.h"
#include "download.h"
#include "download_queue.h"
#include "favorites.h"
#include "filter.h"
#include "ui_login.h"
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
static lv_obj_t *g_downloaded_label;
static lv_obj_t *g_download_bar;
static lv_obj_t *g_download_status_label;
static lv_obj_t *g_container;
static RomList g_list;
static int g_selected;     /* position within the effective (filtered or not) list */
static int g_window_start; /* position of the item shown in g_rows[0] */
static const EmuEntry *g_emu;
static char g_roms_dir[300]; /* /mnt/SDCARD/Roms/<emu->code> */
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

/* Confusion in practice: a user downloaded a rom successfully, backed out,
 * and reported "couldn't find the game" — turned out the emulator's game
 * list just needed a fresh open (the already-cached view they were looking
 * at hadn't picked up reset_list.sh's cache-clear yet), not a real bug. The
 * status message hiding itself instantly on DONE didn't help — give the
 * user a moment of explicit confirmation instead of an instant disappear. */
#define STATUS_MSG_MS 2000
static bool g_status_msg_pending;
static uint32_t g_status_msg_tick;

/* Combined filter: a subset of g_list (by position) matching ALL active
 * criteria at once — search text AND region AND favourites-only. Each is
 * independently toggleable; g_filter_active is true if any is engaged. */
static int g_filter_idx[MAX_FILTER_ITEMS];
static int g_filter_count;
static bool g_filter_active;
static char g_query[64];         /* current search text ("" = none) */
static RegionFilter g_region;    /* REGION_ALL = no region constraint */
static bool g_fav_only;          /* show only favourited roms */
static lv_obj_t *g_search_overlay;
static lv_group_t *g_search_group;
static lv_obj_t *g_kb;
static uint32_t g_kb_last_sel = LV_BUTTONMATRIX_BUTTON_NONE;

/* download confirmation overlay, opened on A before a download actually
 * starts — see open_download_confirm() below */
static bool g_confirm_close_pending;
static bool g_confirm_close_apply;
static lv_obj_t *g_confirm_overlay;
static lv_group_t *g_confirm_group;
static void start_download(void);

/* region/favourites filter overlay (Select button) — see open_filter() */
static bool g_filter_close_pending;
static bool g_filter_close_apply;
static lv_obj_t *g_filter_overlay;
static lv_group_t *g_filter_group;
static lv_obj_t *g_filter_region_label;
static lv_obj_t *g_filter_fav_label;
static int g_filter_sel;             /* 0 = region row, 1 = favourites row */
static RegionFilter g_filter_tmp_region;
static bool g_filter_tmp_fav;
static void apply_filters(void);

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
            const char *dn = display_name(g_list.items[idx].name);
            /* "* " prefix marks a favourite. A real star glyph (U+2605)
             * isn't in the Montserrat font range LVGL bakes in, and there's
             * no LV_SYMBOL_STAR, so a plain ASCII asterisk is the reliable
             * choice that always renders. */
            if (favorites_is(dn)) {
                char rowbuf[256];
                snprintf(rowbuf, sizeof(rowbuf), "* %s", dn);
                lv_label_set_text(g_row_labels[i], rowbuf);
            } else {
                lv_label_set_text(g_row_labels[i], dn);
            }
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

        if (download_file_exists(g_roms_dir, display_name(g_list.items[idx].name))) {
            lv_label_set_text(g_downloaded_label, "Downloaded");
            lv_obj_remove_flag(g_downloaded_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_downloaded_label, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_add_flag(g_empty_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(g_info_label, "");
        lv_obj_add_flag(g_downloaded_label, LV_OBJ_FLAG_HIDDEN);
        /* empty state — must be handled here, not just at build time: a
         * filter (search/region/fav-only) can collapse a populated catalog
         * to zero rows, and without this the user saw a blank list plus the
         * previous selection's stale box art and no explanation. */
        lv_label_set_text(g_empty_label,
                          g_filter_active ? "No matching roms" :
                          g_list.restricted ? "Needs an archive.org account.\nPress A to sign in." :
                          (g_list.ok ? "No roms" : "Source unavailable right now"));
        lv_obj_remove_flag(g_empty_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_thumb_img, LV_OBJ_FLAG_HIDDEN);
    }

    char pagebuf[48];
    int page = count > 0 ? (g_selected / ROWS_VISIBLE) + 1 : 0;
    int pages = count > 0 ? (count + ROWS_VISIBLE - 1) / ROWS_VISIBLE : 0;
    /* prefix a compact active-filter tag so it's obvious the list is
     * scoped (region name and/or "*" for favourites-only) */
    char tag[24] = "";
    if (g_region != REGION_ALL) snprintf(tag, sizeof(tag), "%s", region_label(g_region));
    if (g_fav_only) { size_t l = strlen(tag); snprintf(tag + l, sizeof(tag) - l, "%s*", l ? " " : ""); }
    if (tag[0]) snprintf(pagebuf, sizeof(pagebuf), "%s  %d/%d", tag, page, pages);
    else snprintf(pagebuf, sizeof(pagebuf), "%d/%d", page, pages);
    lv_label_set_text(g_page_label, pagebuf);

    /* Cache check is fast (local file, no network) so it happens on every
     * selection change immediately — only the actual network fetch (on a
     * cache miss) is debounced, in ui_rom_list_tick(). Previously the
     * cache check was ALSO debounced, so even a re-visited, already-cached
     * cover waited out the same 400ms delay meant to stop a wget-per-
     * keystroke storm while scrolling — that's what read as "still slow"
     * even though the file was already on disk. */
    int idx = count > 0 ? eff_data_idx(g_selected) : -1;
    bool cached = idx >= 0 && thumbnail_try_cache(g_thumb_img, g_emu->thumb_repo, g_list.items[idx].name);
    if (cached) {
        g_thumb_pending_idx = -1;
    } else {
        g_thumb_pending_idx = idx;
        g_thumb_dirty_tick = lv_tick_get();
    }
}

/* Rebuild g_filter_idx from the combined state of all three filters
 * (search text g_query, region g_region, favourites-only g_fav_only). An
 * item survives only if it passes every active criterion. When nothing is
 * engaged, g_filter_active is false and the full list is used directly
 * (eff_count/eff_data_idx). */
static void apply_filters(void) {
    bool has_query = (g_query[0] != '\0');
    bool has_region = (g_region != REGION_ALL);
    g_filter_active = has_query || has_region || g_fav_only;

    g_filter_count = 0;
    if (g_filter_active) {
        char qlow[64];
        int i;
        for (i = 0; g_query[i] && i < (int)sizeof(qlow) - 1; i++) {
            char c = g_query[i];
            qlow[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 'a' - 'A') : c;
        }
        qlow[i] = '\0';

        for (int di = 0; di < g_list.count && g_filter_count < MAX_FILTER_ITEMS; di++) {
            const char *n = display_name(g_list.items[di].name);

            if (has_region && !region_matches(n, g_region)) continue;
            if (g_fav_only && !favorites_is(n)) continue;
            if (has_query) {
                char namelow[256]; /* comfortably covers the longest real leaf names */
                int j;
                for (j = 0; n[j] && j < (int)sizeof(namelow) - 1; j++) {
                    char c = n[j];
                    namelow[j] = (c >= 'A' && c <= 'Z') ? (char)(c + 'a' - 'A') : c;
                }
                namelow[j] = '\0';
                if (!strstr(namelow, qlow)) continue;
            }
            g_filter_idx[g_filter_count++] = di;
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
        snprintf(g_query, sizeof(g_query), "%s", lv_textarea_get_text(ta));
        apply_filters();
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
    if (g_search_overlay || g_confirm_overlay || g_filter_overlay) return;

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
    /* Focused key uses a vivid green fill, NOT white — the default keyboard
     * theme already renders the special keys (Enter/Space/Caps/Backspace) in
     * a light colour, so a white highlight was indistinguishable from them
     * and you couldn't tell which key was selected while typing (reported via
     * Reddit). Green matches the app accent and collides with nothing. */
    lv_obj_set_style_bg_color(g_kb, lv_color_hex(0x2ecc40), LV_PART_ITEMS | LV_STATE_CHECKED);
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

/* Same deferred-close reasoning as the search keyboard above: don't delete
 * the overlay from inside its own LV_EVENT_KEY handler. */
static void confirm_key_cb(lv_event_t *e) {
    uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_ENTER) {
        g_confirm_close_pending = true;
        g_confirm_close_apply = true;
    } else if (key == LV_KEY_ESC) {
        g_confirm_close_pending = true;
        g_confirm_close_apply = false;
    }
}

static void process_pending_confirm_close(void) {
    if (!g_confirm_close_pending) return;
    g_confirm_close_pending = false;
    bool apply = g_confirm_close_apply;

    lvgl_glue_set_active_group(g_group);
    if (g_confirm_overlay) {
        lv_obj_delete(g_confirm_overlay);
        g_confirm_overlay = NULL;
    }
    if (g_confirm_group) {
        lv_group_delete(g_confirm_group);
        g_confirm_group = NULL;
    }
    if (apply) start_download();
}

static void open_download_confirm(void) {
    if (g_confirm_overlay) return;
    int count = eff_count();
    if (count == 0) return;
    int idx = eff_data_idx(g_selected);

    g_confirm_overlay = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_confirm_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_confirm_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_confirm_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_confirm_overlay, 0, 0);
    lv_obj_remove_flag(g_confirm_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g_confirm_overlay, confirm_key_cb, LV_EVENT_KEY, NULL);

    lv_obj_t *box = lv_obj_create(g_confirm_overlay);
    lv_obj_set_size(box, LV_PCT(90), 160);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0x555555), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    char sizebuf[32];
    format_size(g_list.items[idx].size, sizebuf, sizeof(sizebuf));
    char msg[220];
    snprintf(msg, sizeof(msg), "Download this rom?\n\n%s\n%s",
             display_name(g_list.items[idx].name), sizebuf);

    lv_obj_t *label = lv_label_create(box);
    lv_label_set_text(label, msg);
    lv_obj_set_style_text_color(label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *hint = lv_label_create(box);
    lv_label_set_text(hint, "A: Confirm   B: Cancel");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);

    g_confirm_group = lv_group_create();
    lv_group_add_obj(g_confirm_group, g_confirm_overlay);
    lvgl_glue_set_active_group(g_confirm_group);
}

static void start_download(void) {
    int count = eff_count();
    if (count == 0) return;
    int idx = eff_data_idx(g_selected);
    RomEntry *item = &g_list.items[idx];
    if (download_file_exists(g_roms_dir, display_name(item->name))) return; /* already have it */

    /* Enqueue rather than start directly — the queue (queue_tick, driven
     * from the main loop) owns the download lifecycle now, so multiple
     * roms can be lined up and each still completes even if the user backs
     * out mid-download. queue_add dedups and rejects already-have items;
     * the progress UI is filled in from queue status in update_download(). */
    queue_add(g_emu->archive_id, item->name, g_roms_dir, display_name(item->name), item->size);
    g_status_msg_pending = false; /* a stale DONE/FAILED message shouldn't linger over a new one */
}

/* ---- region / favourites filter overlay (Select) ---- */

static void render_filter_overlay(void) {
    char rbuf[48], fbuf[48];
    /* "> " marks the row the D-pad is on; Left/Right changes that row's
     * value. Kept text-only (no focus styling) for the same reason the
     * search keyboard does — this device's per-widget focus styling has
     * been unreliable, an explicit marker always renders. */
    snprintf(rbuf, sizeof(rbuf), "%sRegion:  < %s >",
             g_filter_sel == 0 ? "> " : "  ", region_label(g_filter_tmp_region));
    snprintf(fbuf, sizeof(fbuf), "%sFavourites only:  %s",
             g_filter_sel == 1 ? "> " : "  ", g_filter_tmp_fav ? "On" : "Off");
    lv_label_set_text(g_filter_region_label, rbuf);
    lv_label_set_text(g_filter_fav_label, fbuf);
}

static void filter_key_cb(lv_event_t *e) {
    uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_UP || key == LV_KEY_DOWN) {
        g_filter_sel ^= 1; /* only two rows */
        render_filter_overlay();
    } else if (key == LV_KEY_LEFT || key == LV_KEY_RIGHT) {
        if (g_filter_sel == 0) {
            int r = (int)g_filter_tmp_region + (key == LV_KEY_RIGHT ? 1 : -1);
            if (r < 0) r = REGION_COUNT - 1;
            if (r >= REGION_COUNT) r = 0;
            g_filter_tmp_region = (RegionFilter)r;
        } else {
            g_filter_tmp_fav = !g_filter_tmp_fav;
        }
        render_filter_overlay();
    } else if (key == LV_KEY_ENTER) {
        g_filter_close_pending = true;
        g_filter_close_apply = true;
    } else if (key == LV_KEY_ESC) {
        g_filter_close_pending = true;
        g_filter_close_apply = false;
    }
}

static void process_pending_filter_close(void) {
    if (!g_filter_close_pending) return;
    g_filter_close_pending = false;
    bool apply = g_filter_close_apply;

    lvgl_glue_set_active_group(g_group);
    if (g_filter_overlay) {
        lv_obj_delete(g_filter_overlay);
        g_filter_overlay = NULL;
    }
    if (g_filter_group) {
        lv_group_delete(g_filter_group);
        g_filter_group = NULL;
    }
    g_filter_region_label = NULL;
    g_filter_fav_label = NULL;

    if (apply) {
        g_region = g_filter_tmp_region;
        g_fav_only = g_filter_tmp_fav;
        apply_filters();
    }
}

void ui_rom_list_open_filter(void) {
    if (g_search_overlay || g_confirm_overlay || g_filter_overlay) return;

    g_filter_tmp_region = g_region;
    g_filter_tmp_fav = g_fav_only;
    g_filter_sel = 0;

    g_filter_overlay = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_filter_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_filter_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_filter_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_filter_overlay, 0, 0);
    lv_obj_remove_flag(g_filter_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g_filter_overlay, filter_key_cb, LV_EVENT_KEY, NULL);

    lv_obj_t *box = lv_obj_create(g_filter_overlay);
    lv_obj_set_size(box, LV_PCT(90), 180);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0x555555), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(box);
    lv_label_set_text(title, "Filters");
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    g_filter_region_label = lv_label_create(box);
    lv_obj_set_style_text_color(g_filter_region_label, lv_color_hex(0xffffff), 0);
    lv_obj_align(g_filter_region_label, LV_ALIGN_TOP_LEFT, 8, 44);

    g_filter_fav_label = lv_label_create(box);
    lv_obj_set_style_text_color(g_filter_fav_label, lv_color_hex(0xffffff), 0);
    lv_obj_align(g_filter_fav_label, LV_ALIGN_TOP_LEFT, 8, 76);

    lv_obj_t *hint = lv_label_create(box);
    lv_label_set_text(hint, "Up/Down: Row  Left/Right: Change  A: Apply  B: Cancel");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);

    render_filter_overlay();

    g_filter_group = lv_group_create();
    lv_group_add_obj(g_filter_group, g_filter_overlay);
    lvgl_glue_set_active_group(g_filter_group);
}

void ui_rom_list_toggle_favorite(void) {
    if (g_search_overlay || g_confirm_overlay || g_filter_overlay) return;
    int count = eff_count();
    if (count == 0) return;
    int idx = eff_data_idx(g_selected);
    favorites_toggle(display_name(g_list.items[idx].name));
    /* if favourites-only is on, toggling off the selected rom drops it from
     * the list — apply_filters rebuilds; otherwise just refresh the star */
    if (g_fav_only) apply_filters();
    else redraw();
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
            /* first Back clears every active filter, second one leaves */
            g_query[0] = '\0';
            g_region = REGION_ALL;
            g_fav_only = false;
            apply_filters();
        } else if (g_on_back) {
            g_on_back();
        }
    } else if (key == LV_KEY_ENTER) {
        {   /* temporary diagnostic: which branch does A take here? */
            FILE *lf = fopen("log.txt", "a");
            if (lf) { fprintf(lf, "romlist: ENTER count=%d restricted=%d\n", count, g_list.restricted); fclose(lf); }
        }
        if (count == 0 && g_list.restricted) {
            /* The wall the user just hit — offer the fix right here rather
             * than hiding sign-in behind a settings screen they'd have to
             * know about. */
            {
                FILE *lf = fopen("log.txt", "a");
                if (lf) { fprintf(lf, "romlist: opening sign-in\n"); fclose(lf); }
            }
            ui_login_open(g_group);
        } else if (count > 0) {
            int idx = eff_data_idx(g_selected);
            if (!download_file_exists(g_roms_dir, display_name(g_list.items[idx].name))) {
                open_download_confirm();
            }
        }
    }
}

void ui_rom_list_build(lv_group_t *group, const EmuEntry *emu, RomList list,
                        ui_rom_list_back_cb_t on_back) {
    g_list = list;
    g_selected = 0;
    g_window_start = 0;
    g_emu = emu;
    snprintf(g_roms_dir, sizeof(g_roms_dir), "/mnt/SDCARD/Roms/%s", emu->code);
    g_thumb_pending_idx = -1;
    g_on_back = on_back;
    g_group = group;
    g_filter_active = false;
    g_filter_count = 0;
    g_query[0] = '\0';
    g_region = REGION_ALL;
    g_fav_only = false;
    g_search_overlay = NULL;
    g_search_group = NULL;
    g_confirm_overlay = NULL;
    g_confirm_group = NULL;
    g_confirm_close_pending = false;
    g_filter_overlay = NULL;
    g_filter_group = NULL;
    g_filter_close_pending = false;
    g_filter_region_label = NULL;
    g_filter_fav_label = NULL;
    g_marquee_row = -1;
    g_marquee_pending_row = -1;
    g_status_msg_pending = false;
    /* NOTE: the download queue is deliberately NOT reset here — it's owned
     * app-wide (queue_reset runs once at startup) so an in-flight download
     * survives leaving and re-entering this screen. */
    favorites_load(emu->code);

    char header[80];
    if (g_list.ok) {
        snprintf(header, sizeof(header), "%s", emu->label);
    } else if (g_list.restricted) {
        snprintf(header, sizeof(header), "%s (account required)", emu->label);
    } else {
        snprintf(header, sizeof(header), "%s (source unavailable)", emu->label);
    }
    ui_chrome_build(header, "A:DL  Y:Search  X:Fav  Sel:Filter  B:Back");

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

    g_downloaded_label = lv_label_create(screen);
    lv_obj_set_style_text_color(g_downloaded_label, lv_color_hex(0x2ecc40), 0); /* green, per request */
    lv_obj_set_style_text_font(g_downloaded_label, &lv_font_montserrat_16, 0);
    lv_obj_align(g_downloaded_label, LV_ALIGN_TOP_RIGHT, -4, UI_CHROME_HEADER_H + 188);
    lv_obj_add_flag(g_downloaded_label, LV_OBJ_FLAG_HIDDEN);

    g_download_bar = lv_bar_create(screen);
    lv_obj_set_size(g_download_bar, 132, 14);
    lv_obj_align(g_download_bar, LV_ALIGN_TOP_RIGHT, -4, UI_CHROME_HEADER_H + 212);
    lv_bar_set_range(g_download_bar, 0, 100);
    lv_obj_add_flag(g_download_bar, LV_OBJ_FLAG_HIDDEN);

    g_download_status_label = lv_label_create(screen);
    lv_obj_set_style_text_color(g_download_status_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(g_download_status_label, &lv_font_montserrat_16, 0);
    lv_obj_align(g_download_status_label, LV_ALIGN_TOP_RIGHT, -4, UI_CHROME_HEADER_H + 230);
    lv_obj_add_flag(g_download_status_label, LV_OBJ_FLAG_HIDDEN);

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
    lv_obj_add_flag(g_empty_label, LV_OBJ_FLAG_HIDDEN); /* redraw() sets text+visibility from eff_count() */

    redraw();

    lv_group_add_obj(group, g_container);
    lv_obj_add_event_cb(g_container, key_event_cb, LV_EVENT_KEY, NULL);
}

/* Mirror the download queue's state into the progress UI. The queue (not
 * this function) owns download_poll, so here we only read status and paint.
 * queue_tick() runs in main.c every frame regardless of screen, so a
 * download that finished while the user was elsewhere is reported via
 * last_result the moment they're back on this screen. */
static void update_download(void) {
    QueueStatus qs;
    queue_get_status(&qs);

    /* consume a completion event first */
    if (qs.last_result == QRESULT_DONE) {
        queue_clear_last_result();
        /* No transient "Downloaded!" toast — redraw() unhides the
         * persistent green "Downloaded" label under the size, and showing
         * both at once duplicated the text on screen (reported directly).
         * The toast is kept only for the FAILED case, which has no
         * persistent indicator of its own. */
        g_status_msg_pending = false;
        lv_obj_add_flag(g_download_status_label, LV_OBJ_FLAG_HIDDEN);
        redraw();                 /* refresh "Downloaded" for the current row */
        ui_chrome_refresh_status(); /* free space dropped */
    } else if (qs.last_result == QRESULT_FAILED) {
        queue_clear_last_result();
        lv_label_set_text(g_download_status_label, "Download failed");
        lv_obj_set_style_text_color(g_download_status_label, lv_color_hex(0xff4136), 0);
        lv_obj_remove_flag(g_download_status_label, LV_OBJ_FLAG_HIDDEN);
        g_status_msg_pending = true;
        g_status_msg_tick = lv_tick_get();
    }

    if (qs.active) {
        lv_bar_set_value(g_download_bar, (int32_t)(qs.progress * 100), LV_ANIM_OFF);
        lv_obj_remove_flag(g_download_bar, LV_OBJ_FLAG_HIDDEN);
        if (!g_status_msg_pending) { /* don't clobber a failed toast still showing */
            char buf[48];
            if (qs.waiting > 0) snprintf(buf, sizeof(buf), "Downloading... (+%d)", qs.waiting);
            else snprintf(buf, sizeof(buf), "Downloading...");
            lv_label_set_text(g_download_status_label, buf);
            lv_obj_set_style_text_color(g_download_status_label, lv_color_hex(0xffffff), 0);
            lv_obj_remove_flag(g_download_status_label, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        lv_obj_add_flag(g_download_bar, LV_OBJ_FLAG_HIDDEN);
        if (!g_status_msg_pending) lv_obj_add_flag(g_download_status_label, LV_OBJ_FLAG_HIDDEN);
    }

    if (g_status_msg_pending && lv_tick_elaps(g_status_msg_tick) >= STATUS_MSG_MS) {
        g_status_msg_pending = false;
        lv_obj_add_flag(g_download_status_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_rom_list_tick(void) {
    process_pending_search_close();
    process_pending_confirm_close();
    process_pending_filter_close();
    update_keyboard_highlight();
    update_marquee();
    update_download();

    if (g_thumb_pending_idx < 0) return;
    if (lv_tick_elaps(g_thumb_dirty_tick) < THUMB_DEBOUNCE_MS) return;

    int idx = g_thumb_pending_idx;
    g_thumb_pending_idx = -1;
    if (idx >= 0 && idx < g_list.count) {
        thumbnail_fetch_network(g_thumb_img, g_emu->thumb_repo, g_list.items[idx].name);
    }
}

void ui_rom_list_destroy(void) {
    g_thumb_pending_idx = -1;
    g_search_close_pending = false;
    g_confirm_close_pending = false;
    g_filter_close_pending = false;
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
    if (g_confirm_overlay) {
        lv_obj_delete(g_confirm_overlay);
        g_confirm_overlay = NULL;
    }
    if (g_confirm_group) {
        lv_group_delete(g_confirm_group);
        g_confirm_group = NULL;
    }
    if (g_filter_overlay) {
        lv_obj_delete(g_filter_overlay);
        g_filter_overlay = NULL;
    }
    if (g_filter_group) {
        lv_group_delete(g_filter_group);
        g_filter_group = NULL;
    }
    g_filter_region_label = NULL;
    g_filter_fav_label = NULL;
    net_archive_free(&g_list);
}
