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

    int sig = wifi_get_signal();
    /* green when connected, red when the interface is down / no stats —
     * a single symbol, not signal bars, keeps it legible at header size */
    const char *wifi_color = sig >= 0 ? "2ECC40" : "FF4136";

    StorageInfo s = storage_get_free_space("/mnt/SDCARD");
    char text[96];
    if (s.ok) {
        snprintf(text, sizeof(text), "#%s " LV_SYMBOL_WIFI "#  %.1fG (%.0f%%)",
                 wifi_color, s.free_bytes / (1024.0 * 1024 * 1024), s.free_percent);
    } else {
        snprintf(text, sizeof(text), "#%s " LV_SYMBOL_WIFI "#", wifi_color);
    }
    lv_label_set_text(g_status_label, text);
    lv_obj_align(g_status_label, LV_ALIGN_TOP_RIGHT, -8, 6); /* re-align, width changed */
}
