#ifndef UI_KB_H
#define UI_KB_H

#include "lvgl.h"

/* The on-screen keyboard, once. Three screens need one — search in the rom
 * list, the source editor, and the login form — and all three had grown their
 * own copy of the same create-and-style block plus the same highlight
 * routine. They only ever differed in parent, textarea and event callback.
 *
 * The styling here is not cosmetic detail: both of these were on-device bug
 * fixes that any new copy would have to rediscover.
 *  - The selected key is marked with LV_STATE_CHECKED, not the default
 *    FOCUSED/FOCUS_KEY styles. Those did not visibly render on this device
 *    (unconfirmed why, and this device has a history of style/state
 *    surprises); CHECKED reads a per-button ctrl bit we set ourselves, which
 *    lv_buttonmatrix's own draw code honours unconditionally.
 *  - That highlight is green, not white. The default theme already draws the
 *    special keys (Enter/Space/Caps/Backspace) in a light colour, so a white
 *    highlight was indistinguishable from them and you could not tell which
 *    key was selected while typing (reported via Reddit).
 */
lv_obj_t *ui_kb_create(lv_obj_t *parent, lv_obj_t *textarea, lv_event_cb_t cb);

/* Moves the highlight to whichever key is selected now. Call from the owning
 * screen's tick. `last_sel` is the caller's own state — one per keyboard —
 * and ui_kb_create() seeds it, so pass the same variable back in here. */
void ui_kb_update_highlight(lv_obj_t *kb, uint32_t *last_sel);

#endif
