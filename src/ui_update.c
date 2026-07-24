#include "ui_update.h"
#include "ui_chrome.h"
#include "download.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Same "give the user a moment to actually read it" reasoning as the
 * download status toast in ui_rom_list.c. */
#define STATUS_MSG_MS 2000

typedef enum { ST_PROMPT, ST_DOWNLOADING, ST_FAILED_WAIT, ST_RESTART, ST_FINISHED } InternalState;

static InternalState g_state;
static lv_obj_t *g_info_label;
static lv_obj_t *g_bar;
static char g_download_url[512];
static unsigned long g_download_size;
static uint32_t g_msg_tick;

static void show_failure(const char *msg) {
    lv_obj_add_flag(g_bar, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(g_info_label, msg);
    lv_obj_set_style_text_color(g_info_label, lv_color_hex(0xff4136), 0);
    g_msg_tick = lv_tick_get();
    g_state = ST_FAILED_WAIT;
}

static void key_event_cb(lv_event_t *e) {
    if (g_state != ST_PROMPT) return; /* ignore input mid-download / mid-message */
    uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_ENTER) {
        g_state = ST_DOWNLOADING;
        lv_label_set_text(g_info_label, "Downloading update...");
        lv_bar_set_value(g_bar, 0, LV_ANIM_OFF);
        lv_obj_remove_flag(g_bar, LV_OBJ_FLAG_HIDDEN);
        download_start_url(g_download_url, ".", "romdownloader.new", g_download_size);
    } else if (key == LV_KEY_ESC) {
        g_state = ST_FINISHED;
    }
}

void ui_update_build(lv_group_t *group, const UpdateCheckResult *info) {
    g_state = ST_PROMPT;
    snprintf(g_download_url, sizeof(g_download_url), "%s", info->download_url);
    g_download_size = info->size;
    download_reset();

    ui_chrome_build("Update Available", "A: Download & Install   B: Ignore");

    lv_obj_t *screen = lv_screen_active();

    g_info_label = lv_label_create(screen);
    lv_obj_set_style_text_color(g_info_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(g_info_label, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_align(g_info_label, LV_TEXT_ALIGN_CENTER, 0);
    char msg[64];
    snprintf(msg, sizeof(msg), "Version %s is ready", info->latest_tag);
    lv_label_set_text(g_info_label, msg);
    lv_obj_align(g_info_label, LV_ALIGN_CENTER, 0, -20);

    g_bar = lv_bar_create(screen);
    lv_obj_set_size(g_bar, 300, 16);
    lv_obj_align(g_bar, LV_ALIGN_CENTER, 0, 30);
    lv_bar_set_range(g_bar, 0, 100);
    lv_obj_add_flag(g_bar, LV_OBJ_FLAG_HIDDEN);

    lv_group_add_obj(group, screen);
    lv_obj_add_event_cb(screen, key_event_cb, LV_EVENT_KEY, NULL);
}

UiUpdateStatus ui_update_tick(void) {
    if (g_state == ST_DOWNLOADING) {
        float progress = 0.0f;
        DownloadState st = download_poll(&progress);
        if (st == DOWNLOAD_RUNNING) {
            lv_bar_set_value(g_bar, (int32_t)(progress * 100), LV_ANIM_OFF);
        } else if (st == DOWNLOAD_DONE) {
            download_reset();
            /* Do NOT rename romdownloader.new over romdownloader here: the SD
             * card is FAT32 (confirmed: vfat mount), which has no inode
             * indirection, so replacing the currently-EXECUTING binary in
             * place fails/corrupts — that's exactly what produced the
             * "Update failed" in the field (download succeeded, install
             * didn't). Instead leave the new binary as romdownloader.new and
             * exit; launch.sh swaps it in on the next start, when nothing is
             * executing romdownloader. */
            chmod("romdownloader.new", 0755); /* launch.sh re-chmods too, belt and suspenders */
            g_state = ST_RESTART;
        } else if (st == DOWNLOAD_FAILED) {
            download_reset();
            show_failure("Update download failed");
        }
    } else if (g_state == ST_FAILED_WAIT) {
        if (lv_tick_elaps(g_msg_tick) >= STATUS_MSG_MS) g_state = ST_FINISHED;
    }

    if (g_state == ST_RESTART) return UI_UPDATE_RESTART;
    if (g_state == ST_FINISHED) return UI_UPDATE_FINISHED;
    return UI_UPDATE_ACTIVE;
}
