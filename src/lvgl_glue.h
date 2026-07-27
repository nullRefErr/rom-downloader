#ifndef LVGL_GLUE_H
#define LVGL_GLUE_H

#include <SDL2/SDL.h>
#include "lvgl.h"

/* Sets up an LVGL display (w x h, matches lv_conf.h's RGB565 color depth)
 * that flushes into a streaming SDL texture on `ren` + SDL_RenderCopy +
 * SDL_RenderPresent (the only rendering path confirmed to actually reach
 * the screen on this device's MMIYOO SDL2 driver — see README/plan), and a
 * keypad-type indev fed by lvgl_glue_feed_key(). Returns the lv_group_t
 * that widgets should be added to for keypad navigation to reach them. */
lv_group_t *lvgl_glue_init(SDL_Renderer *ren, int w, int h);

/* Feed one SDL key event's keycode into the LVGL keypad indev. Unmapped
 * keys (anything not in the confirmed A/B/D-pad/Start set) are ignored. */
void lvgl_glue_feed_key(SDL_Keycode sym, SDL_bool pressed);

/* Same, but without the navigation click. Used for synthesised auto-repeat
 * while a direction is held: clicking ~11 times a second would just smear
 * into a drone. */
void lvgl_glue_feed_key_quiet(SDL_Keycode sym, SDL_bool pressed);

/* Forces an immediate render+flush of whatever's currently marked dirty,
 * bypassing lv_timer_handler()'s normal scheduling. Needed before a
 * blocking call (e.g. the synchronous wget-based archive.org fetch) so a
 * "Loading..." label actually reaches the screen before the block starts,
 * instead of sitting queued until the next timer_handler() call after the
 * blocking call already returned. */
void lvgl_glue_force_redraw(void);

/* Repoints the single keypad indev at a different group — used to hand
 * control to a temporary group (e.g. the search keyboard overlay) and
 * back to a screen's normal group afterward. */
void lvgl_glue_set_active_group(lv_group_t *group);

#endif
