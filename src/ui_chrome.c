#include "ui_chrome.h"

void ui_chrome_build(const char *title, const char *hints) {
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *header = lv_label_create(screen);
    lv_label_set_text(header, title);
    lv_obj_set_style_text_color(header, lv_color_hex(0xffffff), 0);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 8, 4);

    lv_obj_t *footer = lv_label_create(screen);
    lv_label_set_text(footer, hints);
    lv_obj_set_style_text_color(footer, lv_color_hex(0x999999), 0);
    lv_obj_set_style_text_font(footer, &lv_font_montserrat_16, 0);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_LEFT, 8, -4);
}
