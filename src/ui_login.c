#include "ui_login.h"
#include "ui_kb.h"
#include "ui_overlay.h"
#include "i18n.h"
#include "ui_chrome.h"
#include "lvgl_glue.h"
#include "net_login.h"
#include "env.h"
#include <stdio.h>

/* Same "give the user a moment to read it" delay as the download toast. */
#define RESULT_MS 2500      /* success: brief confirmation */
#define RESULT_FAIL_MS 5000 /* failure: three lines to read, needs longer */

typedef enum { ST_EMAIL, ST_PASSWORD, ST_SUBMIT, ST_RESULT, ST_CLOSING } LoginStep;

static UiOverlay g_ov;
static lv_group_t *g_restore_group;
static lv_obj_t *g_email_ta;
static lv_obj_t *g_pass_ta;
static lv_obj_t *g_kb;
static lv_obj_t *g_status;
static LoginStep g_step;
static uint32_t g_result_tick;
static bool g_result_ok;
static uint32_t g_kb_last_sel = LV_BUTTONMATRIX_BUTTON_NONE;

/* Same reason as everywhere else in this app: the focused key's default
 * styling doesn't render reliably on this device, so drive it from a ctrl
 * bit we set ourselves. Green, matching the search keyboard, because the
 * theme already paints the special keys a light shade. */

static void set_status(const char *text, uint32_t color) {
    lv_label_set_text(g_status, text);
    lv_obj_set_style_text_color(g_status, lv_color_hex(color), 0);
}

/* Deferred, never synchronous from inside the keyboard's own handler —
 * deleting an object mid-dispatch froze the whole app once already. */
static void kb_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CANCEL) {
        g_step = ST_CLOSING;
        return;
    }
    if (code != LV_EVENT_READY) return;

    if (g_step == ST_EMAIL) {
        g_step = ST_PASSWORD;
        lv_keyboard_set_textarea(g_kb, g_pass_ta);
        set_status(T("login_hint_password"), 0x888888);
    } else if (g_step == ST_PASSWORD) {
        g_step = ST_SUBMIT; /* the blocking request runs in the tick, not here */
    }
}

void ui_login_open(lv_group_t *restore_group) {
    if (g_ov.obj) return;
    g_restore_group = restore_group;
    g_step = ST_EMAIL;
    g_kb_last_sel = LV_BUTTONMATRIX_BUTTON_NONE;

    ui_overlay_open(&g_ov);

    lv_obj_t *title = lv_label_create(g_ov.obj);
    lv_label_set_text(title, T("login_title"));
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    g_email_ta = lv_textarea_create(g_ov.obj);
    lv_textarea_set_one_line(g_email_ta, true);
    lv_textarea_set_placeholder_text(g_email_ta, T("login_email"));
    lv_obj_set_size(g_email_ta, LV_PCT(94), 34);
    lv_obj_align(g_email_ta, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_set_style_bg_color(g_email_ta, lv_color_hex(0x111111), 0);
    lv_obj_set_style_text_color(g_email_ta, lv_color_hex(0xffffff), 0);

    g_pass_ta = lv_textarea_create(g_ov.obj);
    lv_textarea_set_one_line(g_pass_ta, true);
    lv_textarea_set_password_mode(g_pass_ta, true); /* never show the password on screen */
    lv_textarea_set_placeholder_text(g_pass_ta, T("login_password"));
    lv_obj_set_size(g_pass_ta, LV_PCT(94), 34);
    lv_obj_align(g_pass_ta, LV_ALIGN_TOP_MID, 0, 66);
    lv_obj_set_style_bg_color(g_pass_ta, lv_color_hex(0x111111), 0);
    lv_obj_set_style_text_color(g_pass_ta, lv_color_hex(0xffffff), 0);

    g_status = lv_label_create(g_ov.obj);
    lv_obj_set_style_text_font(g_status, &lv_font_ui_14, 0);
    lv_obj_set_width(g_status, LV_PCT(94));
    lv_label_set_long_mode(g_status, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(g_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(g_status, LV_ALIGN_TOP_MID, 0, 104);
    set_status(T("login_hint_email"), 0x888888);

    /* Prefill from .env if it's there. The overlay is normally only reached
     * when the startup auto-sign-in did not get us a session — i.e. exactly
     * when something in these values needs correcting — so retyping an email
     * on a d-pad to fix a password typo would be wasted effort. X clears the
     * field you're on, for the case where the stored value is the wrong one. */
    {
        char email[256], password[256];
        if (env_load(email, sizeof(email), password, sizeof(password))) {
            lv_textarea_set_text(g_email_ta, email);
            lv_textarea_set_text(g_pass_ta, password);
            set_status(T("login_hint_prefilled"), 0x888888);
        }
    }

    g_kb = ui_kb_create(g_ov.obj, g_email_ta, kb_event_cb);
    g_kb_last_sel = LV_BUTTONMATRIX_BUTTON_NONE;

    ui_overlay_focus(&g_ov, g_kb);
}

bool ui_login_is_open(void) {
    return g_ov.obj != NULL;
}

void ui_login_clear_field(void) {
    if (!g_ov.obj) return;
    lv_textarea_set_text(g_step == ST_EMAIL ? g_email_ta : g_pass_ta, "");
}

void ui_login_backspace(void) {
    if (!g_ov.obj) return;
    lv_textarea_delete_char(g_step == ST_EMAIL ? g_email_ta : g_pass_ta);
}

static void close_now(void) {
    g_kb = NULL;
    g_email_ta = g_pass_ta = g_status = NULL;
    ui_overlay_close(&g_ov, g_restore_group);
}

void ui_login_tick(void) {
    if (!g_ov.obj) return;
    ui_kb_update_highlight(g_kb, &g_kb_last_sel);

    if (g_step == ST_SUBMIT) {
        /* net_login blocks on curl for up to 30s — paint the notice first,
         * exactly like the archive.org fetch and update check do, or the
         * redraw would only land after the call already returned. */
        set_status(T("login_in_progress"), 0xffffff);
        lvgl_glue_force_redraw();

        char email[256], password[256];
        snprintf(email, sizeof(email), "%s", lv_textarea_get_text(g_email_ta));
        snprintf(password, sizeof(password), "%s", lv_textarea_get_text(g_pass_ta));

        LoginResult res = net_login(email, password);

        g_result_ok = res.ok;
        if (res.ok) {
            /* Remember the credentials so the session can be renewed without
             * retyping when the cookies eventually expire. Plain text on the
             * card — see env.h for why that trade is being made. */
            env_save(email, password);
            /* Only wipe it once it has actually worked. Clearing on failure
             * meant a wrong keystroke, or a transient network error, cost a
             * full retype on a d-pad keyboard — reported directly. */
            lv_textarea_set_text(g_pass_ta, "");
            set_status(res.message, 0x2ecc40);
        } else {
            /* Don't leave a failure as a dead end. archive.org accounts are
             * email+password (it offers no Google/Apple sign-in), so the
             * usual cause is a typo — but an account with two-factor auth
             * can't work through this endpoint at all, and for those the
             * browser-cookie route is the way through. Say so here rather
             * than making the user go looking. */
            char msg[256]; /* res.message is 96; the hint adds ~75 */
            snprintf(msg, sizeof(msg), "%s\n%s", res.message, T("login_fail_2fa_hint"));
            set_status(msg, 0xff4136);
        }
        g_step = ST_RESULT;
        g_result_tick = lv_tick_get();
        return;
    }

    if (g_step == ST_RESULT &&
        lv_tick_elaps(g_result_tick) >= (g_result_ok ? RESULT_MS : RESULT_FAIL_MS)) {
        if (g_result_ok) {
            g_step = ST_CLOSING;
        } else {
            /* Stay open on failure with the password still filled in, so a
             * retry is one keypress rather than a full retype. Closing here
             * meant a transient error cost the whole entry again. */
            g_step = ST_PASSWORD;
            lv_keyboard_set_textarea(g_kb, g_pass_ta);
            set_status(T("login_hint_password"), 0x888888);
        }
    }
    if (g_step == ST_CLOSING) close_now();
}
