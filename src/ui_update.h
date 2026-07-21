#ifndef UI_UPDATE_H
#define UI_UPDATE_H

#include "lvgl.h"
#include "net_update.h"

typedef enum {
    UI_UPDATE_ACTIVE,   /* still prompting/downloading — caller does nothing */
    UI_UPDATE_FINISHED, /* declined, or failed and shown — caller should proceed to its normal next screen */
    UI_UPDATE_RESTART,  /* new binary swapped in — caller must tear down SDL and execv the same binary path */
} UiUpdateStatus;

/* Builds the "Update Available" prompt screen. Only call this when
 * info->update_available is true — the caller is expected to skip this
 * screen entirely otherwise. */
void ui_update_build(lv_group_t *group, const UpdateCheckResult *info);

/* Call every tick while this screen is active. */
UiUpdateStatus ui_update_tick(void);

#endif
