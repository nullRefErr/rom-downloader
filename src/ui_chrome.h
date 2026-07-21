#ifndef UI_CHROME_H
#define UI_CHROME_H

#include "lvgl.h"

#define UI_CHROME_HEADER_H 34
#define UI_CHROME_FOOTER_H 28

/* Same look on every screen: black background, a title bar at the top,
 * and a button-hint bar at the bottom ("A: Select  B: Back" etc). Call
 * once per screen build, after lv_obj_clean(lv_screen_active()). Also
 * creates the top-right status readout (Wi-Fi symbol + free SD space). */
void ui_chrome_build(const char *title, const char *hints);

/* Refresh the top-right status readout (re-reads Wi-Fi + free space).
 * Cheap; safe to call periodically from the main loop. No-op if no chrome
 * has been built yet on the active screen. */
void ui_chrome_refresh_status(void);

#endif
