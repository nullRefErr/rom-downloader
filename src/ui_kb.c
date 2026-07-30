#include "ui_kb.h"

lv_obj_t *ui_kb_create(lv_obj_t *parent, lv_obj_t *textarea, lv_event_cb_t cb) {
    lv_obj_t *kb = lv_keyboard_create(parent);
    lv_keyboard_set_textarea(kb, textarea);
    lv_obj_add_event_cb(kb, cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb, cb, LV_EVENT_CANCEL, NULL);

    lv_obj_set_style_bg_color(kb, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x222222), LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, lv_color_hex(0xffffff), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x2ecc40), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(kb, lv_color_hex(0x000000), LV_PART_ITEMS | LV_STATE_CHECKED);

    return kb;
}

void ui_kb_update_highlight(lv_obj_t *kb, uint32_t *last_sel) {
    if (!kb || !last_sel) return;

    uint32_t sel = lv_buttonmatrix_get_selected_button(kb);
    if (sel == *last_sel) return;

    if (*last_sel != LV_BUTTONMATRIX_BUTTON_NONE) {
        lv_buttonmatrix_clear_button_ctrl(kb, *last_sel, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
    if (sel != LV_BUTTONMATRIX_BUTTON_NONE) {
        lv_buttonmatrix_set_button_ctrl(kb, sel, LV_BUTTONMATRIX_CTRL_CHECKED);
    }
    *last_sel = sel;
}
