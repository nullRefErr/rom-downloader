#include "ui_settings.h"
#include "ui_chrome.h"
#include "ui_style.h"
#include "lvgl_glue.h"
#include "settings.h"
#include "i18n.h"
#include "auth.h"
#include "net_login.h"
#include "ui_login.h"
#include "download.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

#define ROMS_ROOT "/mnt/SDCARD/Roms"
#define STATUS_MS 3000

enum { ROW_LANGUAGE, ROW_SOUND, ROW_SOURCES, ROW_ACCOUNT, ROW_CLEANUP, ROW_COUNT };

static lv_obj_t *g_rows[ROW_COUNT];
static lv_obj_t *g_labels[ROW_COUNT];
static lv_obj_t *g_container;
static lv_group_t *g_group;
static ui_settings_back_cb_t g_on_back;
static int g_selected;
static bool g_rebuild_pending; /* language changed: rebuild outside the event handler */
static bool g_back_pending;    /* leaving: same, see key_event_cb */
static bool g_sources_pending;  /* opening the source editor: also deferred */



static void refresh_rows(void) {
    const Settings *s = settings_get();
    char buf[256];

    int li = i18n_lang_index(s->lang);
    snprintf(buf, sizeof(buf), "%s:  %s", T("settings_language"),
             i18n_lang_name(li < 0 ? 0 : li));
    lv_label_set_text(g_labels[ROW_LANGUAGE], buf);

    snprintf(buf, sizeof(buf), "%s:  %s", T("settings_sound"), s->sound ? T("on") : T("off"));
    lv_label_set_text(g_labels[ROW_SOUND], buf);

    if (auth_available()) {
        char who[160];
        auth_account(who, sizeof(who));
        if (who[0]) snprintf(buf, sizeof(buf), T("settings_signed_in_as_fmt"), who);
        else snprintf(buf, sizeof(buf), "%s", T("login_ok"));
        size_t l = strlen(buf);
        snprintf(buf + l, sizeof(buf) - l, "   [%s]", T("settings_sign_out"));
    } else {
        snprintf(buf, sizeof(buf), "%s   [%s]", T("settings_signed_out"), T("settings_sign_in"));
    }
    lv_label_set_text(g_labels[ROW_ACCOUNT], buf);

    lv_label_set_text(g_labels[ROW_SOURCES], T("settings_sources"));
    lv_label_set_text(g_labels[ROW_CLEANUP], T("settings_cleanup"));

    for (int i = 0; i < ROW_COUNT; i++) {
        lv_state_t st = LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY;
        if (i == g_selected) lv_obj_add_state(g_rows[i], st);
        else lv_obj_remove_state(g_rows[i], st);
    }
}

static void activate(void) {
    const Settings *s = settings_get();

    switch (g_selected) {
        case ROW_LANGUAGE: {
            int li = i18n_lang_index(s->lang);
            if (li < 0) li = 0;
            li = (li + 1) % i18n_lang_count();
            settings_set_lang(i18n_lang_code(li));
            i18n_set_lang(i18n_lang_code(li));
            /* Rebuilt from the tick, not here: this runs inside an LVGL key
             * event, and tearing the screen down mid-dispatch is the exact
             * thing that froze the app once before. */
            g_rebuild_pending = true;
            break;
        }
        case ROW_SOUND:
            settings_set_sound(!s->sound);
            refresh_rows();
            break;
        case ROW_SOURCES:
            g_sources_pending = true; /* screen swap, so never from in here */
            break;
        case ROW_ACCOUNT:
            if (auth_available()) {
                net_login_sign_out();
                ui_chrome_toast(T("settings_signed_out_done"), STATUS_MS);
                refresh_rows();
            } else {
                ui_login_open(g_group);
            }
            break;
        case ROW_CLEANUP: {
            unsigned long long freed = 0;
            int n = download_cleanup_parts(ROMS_ROOT, &freed);
            char msg[160];
            if (n == 0) {
                snprintf(msg, sizeof(msg), "%s", T("settings_cleanup_none"));
            } else {
                char sz[32];
                util_format_size(freed, sz, sizeof(sz));
                snprintf(msg, sizeof(msg), T("settings_cleanup_done_fmt"), n, sz);
            }
            ui_chrome_toast(msg, STATUS_MS);
            break;
        }
        default:
            break;
    }
}

static void key_event_cb(lv_event_t *e) {
    uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_DOWN) {
        if (g_selected + 1 < ROW_COUNT) { g_selected++; refresh_rows(); }
    } else if (key == LV_KEY_UP) {
        if (g_selected > 0) { g_selected--; refresh_rows(); }
    } else if (key == LV_KEY_ENTER) {
        activate();
    } else if (key == LV_KEY_ESC) {
        /* Deferred like every other teardown here: going back tears this
         * screen down, and doing that from inside its own key event leaves
         * LVGL dispatching against objects we just freed. The visible
         * symptom was the previous screen's footer surviving the rebuild,
         * so the main menu kept showing hints in the old language after a
         * language change. */
        g_back_pending = true;
    }
}

void ui_settings_build(lv_group_t *group, ui_settings_back_cb_t on_back) {
    g_group = group;
    g_on_back = on_back;
    g_rebuild_pending = false;
    g_back_pending = false;
    g_sources_pending = false;

    ui_chrome_build(T("settings_title"), T("hint_settings"));

    lv_obj_t *screen = lv_screen_active();

    g_container = lv_obj_create(screen);
    lv_obj_set_size(g_container, 620, 480 - UI_CHROME_HEADER_H - UI_CHROME_FOOTER_H - 30);
    lv_obj_align(g_container, LV_ALIGN_TOP_LEFT, 8, UI_CHROME_HEADER_H + 6);
    lv_obj_set_style_bg_opa(g_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_container, 0, 0);
    lv_obj_set_flex_flow(g_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(g_container, 6, 0);
    lv_obj_set_style_pad_all(g_container, 0, 0);

    for (int i = 0; i < ROW_COUNT; i++) {
        lv_obj_t *btn = lv_button_create(g_container);
        lv_obj_set_size(btn, LV_PCT(100), 44);
        ui_style_apply_row(btn);
        lv_obj_t *label = lv_label_create(btn);
        lv_obj_set_width(label, LV_PCT(100));
        lv_obj_set_height(label, 26);
        lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
        g_rows[i] = btn;
        g_labels[i] = label;
    }


    refresh_rows();

    lv_group_add_obj(group, g_container);
    lv_obj_add_event_cb(g_container, key_event_cb, LV_EVENT_KEY, NULL);
}

bool ui_settings_wants_sources(void) {
    if (!g_sources_pending) return false;
    g_sources_pending = false;
    return true;
}

/* The login overlay closes itself on a timer and tells nobody, so the account
 * row underneath kept its pre-login text. That is not just cosmetic: the row
 * still read "Not signed in [Sign in]" while activate() reads auth_available()
 * live, so a second A — the obvious thing to press when the screen says you
 * are not signed in — took the sign-out branch and destroyed the session that
 * had just been created. Re-read the state when the overlay goes away. */
static bool g_login_was_open;

void ui_settings_tick(void) {
    if (ui_login_is_open()) {
        g_login_was_open = true;
    } else if (g_login_was_open) {
        g_login_was_open = false;
        refresh_rows();
    }

    if (g_back_pending) {
        g_back_pending = false;
        if (g_on_back) g_on_back();
        return;
    }
    if (g_rebuild_pending) {
        g_rebuild_pending = false;
        int keep = g_selected;
        lv_obj_clean(lv_screen_active());
        ui_settings_build(g_group, g_on_back);
        g_selected = keep;
        refresh_rows();
        return;
    }
}
