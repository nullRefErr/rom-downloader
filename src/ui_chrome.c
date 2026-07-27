#include "ui_chrome.h"
#include "wifi.h"
#include "storage.h"
#include <stdio.h>

/* Recreated by every ui_chrome_build() (lv_obj_clean destroys the previous
 * one). ui_chrome_refresh_status() guards against it being stale/NULL. */
static lv_obj_t *g_status_label;

void ui_chrome_build(const char *title, const char *hints) {
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *header = lv_label_create(screen);
    lv_label_set_text(header, title);
    lv_obj_set_style_text_color(header, lv_color_hex(0xffffff), 0);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 8, 4);

    g_status_label = lv_label_create(screen);
    lv_label_set_recolor(g_status_label, true); /* colour just the Wi-Fi glyph */
    lv_obj_set_style_text_font(g_status_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g_status_label, lv_color_hex(0xaaaaaa), 0);
    lv_obj_align(g_status_label, LV_ALIGN_TOP_RIGHT, -8, 6);
    ui_chrome_refresh_status();

    lv_obj_t *footer = lv_label_create(screen);
    lv_label_set_text(footer, hints);
    lv_obj_set_style_text_color(footer, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_16, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_LEFT, 8, -4);
}

void ui_chrome_refresh_status(void) {
    if (!g_status_label) return;

    /* Two separate readouts, each with its own icon. They used to render as
     * a Wi-Fi glyph immediately followed by "12.4G (45%)" — the free SD
     * space — which reads as a connection speed ("12.4 Gbps") rather than
     * storage, and was reported as "the speed never changes". The SD-card
     * icon disambiguates it, and the Wi-Fi side now carries a real link
     * quality that actually moves as the signal changes. */
    int sig = wifi_get_signal();
    char wifi_part[48];
    if (sig >= 0) {
        snprintf(wifi_part, sizeof(wifi_part), "#2ECC40 " LV_SYMBOL_WIFI "# %d%%", sig);
    } else {
        snprintf(wifi_part, sizeof(wifi_part), "#FF4136 " LV_SYMBOL_WIFI " off#");
    }

    StorageInfo s = storage_get_free_space("/mnt/SDCARD");
    char text[128];
    if (s.ok) {
        snprintf(text, sizeof(text), "%s   " LV_SYMBOL_SD_CARD " %.1fG",
                 wifi_part, s.free_bytes / (1024.0 * 1024 * 1024));
    } else {
        snprintf(text, sizeof(text), "%s", wifi_part);
    }
    lv_label_set_text(g_status_label, text);
    lv_obj_align(g_status_label, LV_ALIGN_TOP_RIGHT, -8, 6); /* re-align, width changed */
}
