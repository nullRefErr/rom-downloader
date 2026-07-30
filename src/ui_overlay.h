#ifndef UI_OVERLAY_H
#define UI_OVERLAY_H

#include "lvgl.h"
#include <stdbool.h>

/* A modal panel over a screen: its own container, its own nav group, and a
 * deferred teardown. Search, the download confirmation, the region filter, the
 * source editor and the login form are all this same thing, and each had grown
 * its own four globals and its own copy of the teardown.
 *
 * The deferral is the load-bearing part, not a style choice. Deleting an
 * overlay from inside its own LVGL event handler corrupts LVGL's mid-dispatch
 * state and froze the whole app — twice, once for search and once for the
 * language switch. So handlers only ever set a flag, and the screen's tick
 * does the actual teardown. Keeping that in one place is the point of this
 * file: a sixth overlay written later inherits the fix instead of the bug. */
typedef struct {
    lv_obj_t   *obj;   /* the container; NULL when closed */
    lv_group_t *group; /* the overlay's own nav group */
    bool        close_pending;
    bool        close_apply;
} UiOverlay;

/* Creates the full-screen black container. The caller parents its own widgets
 * to ov->obj and then calls ui_overlay_focus() with whichever one takes the
 * D-pad — all five overlays built the identical container by hand. */
void ui_overlay_open(UiOverlay *ov);

/* Gives the overlay its own nav group with `target` focused, and makes that
 * group active so the D-pad drives the overlay instead of the screen behind. */
void ui_overlay_focus(UiOverlay *ov, lv_obj_t *target);

/* Call from an event handler. Safe there precisely because it only sets flags. */
void ui_overlay_request_close(UiOverlay *ov, bool apply);

/* Call from the screen's tick. False when nothing was pending. When true, the
 * flag is consumed, *apply says whether the close was a confirm or a cancel,
 * and the overlay is STILL ALIVE — read anything you need out of its widgets
 * now (the search box and the source editor both do), then call
 * ui_overlay_close(). Splitting it this way is deliberate: an all-in-one
 * teardown would have to free before or after the caller's work, and the five
 * call sites genuinely need both orders. */
bool ui_overlay_take_close(UiOverlay *ov, bool *apply);

/* Destroys the overlay and its group and hands focus back to `restore_to`.
 * Idempotent, so a screen teardown can call it without checking. */
void ui_overlay_close(UiOverlay *ov, lv_group_t *restore_to);

#endif
