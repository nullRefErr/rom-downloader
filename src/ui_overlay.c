#include "ui_overlay.h"
#include "lvgl_glue.h"

void ui_overlay_open(UiOverlay *ov) {
    if (!ov) return;
    ov->obj = lv_obj_create(lv_screen_active());
    lv_obj_set_size(ov->obj, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(ov->obj, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ov->obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ov->obj, 0, 0);
    lv_obj_remove_flag(ov->obj, LV_OBJ_FLAG_SCROLLABLE);
}

void ui_overlay_focus(UiOverlay *ov, lv_obj_t *target) {
    if (!ov) return;
    ov->group = lv_group_create();
    lv_group_add_obj(ov->group, target);
    lvgl_glue_set_active_group(ov->group);
}

void ui_overlay_request_close(UiOverlay *ov, bool apply) {
    if (!ov) return;
    ov->close_pending = true;
    ov->close_apply = apply;
}

bool ui_overlay_take_close(UiOverlay *ov, bool *apply) {
    if (!ov || !ov->close_pending) return false;
    ov->close_pending = false;
    if (apply) *apply = ov->close_apply;
    return true;
}

void ui_overlay_close(UiOverlay *ov, lv_group_t *restore_to) {
    if (!ov) return;

    lvgl_glue_set_active_group(restore_to);

    if (ov->obj) {
        lv_obj_delete(ov->obj);
        ov->obj = NULL;
    }
    if (ov->group) {
        lv_group_delete(ov->group);
        ov->group = NULL;
    }
}
