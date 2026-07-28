#include "ui_style.h"
#include <stdbool.h>

static lv_style_t style_row;
static lv_style_t style_row_selected;
static bool g_inited;

void ui_style_init(void) {
    if (g_inited) return;
    g_inited = true;

    /* pure black bg / white text normally, fully inverted (white bg /
     * black text) on the selected row — requested explicitly over the
     * previous orange-highlight look for maximum readability/contrast. */
    lv_style_init(&style_row);
    lv_style_set_bg_color(&style_row, lv_color_hex(0x000000));
    lv_style_set_bg_opa(&style_row, LV_OPA_COVER);
    lv_style_set_text_color(&style_row, lv_color_hex(0xffffff));
    /* Must be one of the generated ui fonts: this style is applied to every
     * list row, so pinning it to the stock ASCII-only face made accented and
     * Japanese text render as boxes in the rows while the rest of the screen
     * was fine — the row style silently overrode LV_FONT_DEFAULT. */
    lv_style_set_text_font(&style_row, &lv_font_ui_22);
    lv_style_set_border_width(&style_row, 1);
    lv_style_set_border_color(&style_row, lv_color_hex(0x333333));
    lv_style_set_radius(&style_row, 0);
    lv_style_set_pad_hor(&style_row, 10);
    lv_style_set_pad_ver(&style_row, 6);
    lv_style_set_width(&style_row, LV_PCT(100));

    lv_style_init(&style_row_selected);
    lv_style_set_bg_color(&style_row_selected, lv_color_hex(0xffffff));
    lv_style_set_bg_opa(&style_row_selected, LV_OPA_COVER);
    lv_style_set_text_color(&style_row_selected, lv_color_hex(0x000000));
    lv_style_set_border_width(&style_row_selected, 0);
}

void ui_style_apply_row(lv_obj_t *row) {
    lv_obj_remove_style_all(row);
    lv_obj_add_style(row, &style_row, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_style(row, &style_row_selected, LV_PART_MAIN | LV_STATE_FOCUSED | LV_STATE_FOCUS_KEY);
}
