#include "lvgl_glue.h"
#include <stdio.h>
#include <string.h>

/* Full-screen render buffer (640x480x2 bytes RGB565 = 600KB). Confirmed on
 * real hardware: this device's custom MMIYOO SDL2 backend's UpdateTexture
 * ignores the sub-rect it's given and just remembers a pointer to whatever
 * buffer was last passed — LVGL's default PARTIAL mode reuses one small
 * scratch buffer across many chunks, so by the time the actual GPU copy
 * runs, that pointer only ever contains the LAST chunk's data. FULL mode
 * renders the whole screen into one persistent buffer per refresh, which
 * this driver's assumptions actually match (see README/plan). */
static SDL_Renderer *g_ren;
static SDL_Texture *g_tex;
static lv_display_t *g_disp;
static lv_indev_t *g_indev;
static uint8_t g_lv_buf[640 * 480 * 2];
static int g_flush_calls;

#define KEY_QUEUE_SIZE 16
static struct {
    uint32_t key;
    lv_indev_state_t state;
} g_key_queue[KEY_QUEUE_SIZE];
static int g_key_head, g_key_tail;

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    (void)area;
    bool is_last = lv_display_flush_is_last(disp);
    if (is_last) {
        /* SDL_UpdateTexture on this device's MMIYOO backend just remembers
         * a pointer to whatever buffer it's given (MMIYOO_UpdateTexture
         * ignores the sub-rect and doesn't copy) — confirmed working
         * instead via SDL_LockTexture, which returns the TEXTURE's own
         * persistent buffer to write into directly, exactly like the
         * proven-working Phase 1 solid-color path did. */
        void *pixels;
        int pitch;
        int lockret = SDL_LockTexture(g_tex, NULL, &pixels, &pitch);
        if (lockret == 0) {
            memcpy(pixels, px_map, (size_t)pitch * 480);
            SDL_UnlockTexture(g_tex);
        }
        if (g_flush_calls == 0) {
            FILE *f = fopen("log.txt", "a");
            if (f) {
                fprintf(f, "flush_cb: LockTexture ret=%d pitch=%d\n", lockret, pitch);
                fclose(f);
            }
        }
        g_flush_calls++;
        SDL_RenderCopy(g_ren, g_tex, NULL, NULL);
        SDL_RenderPresent(g_ren);
    }
    lv_display_flush_ready(disp);
}

static uint32_t sdl_key_to_lv(SDL_Keycode sym) {
    /* button->SDL keycode mapping confirmed on real hardware (Phase 1
     * log.txt). Raw LV_KEY_UP/DOWN, not LV_KEY_NEXT/PREV: every screen in
     * this app owns a single focused object and handles Up/Down itself
     * (see ui_emu_select.c, ui_rom_list.c) rather than relying on lv_group
     * cycling between many widgets — the ROM list needs custom scrolling
     * over a recycled row pool (500-2800 entries, can't be one widget per
     * row), so both screens share that pattern for consistency. NEXT/PREV
     * would just be consumed by lv_group's own navigation before reaching
     * either screen's key handler. */
    switch (sym) {
        case SDLK_DOWN: return LV_KEY_DOWN;
        case SDLK_UP: return LV_KEY_UP;
        case SDLK_LEFT: return LV_KEY_LEFT;     /* page back in the ROM list */
        case SDLK_RIGHT: return LV_KEY_RIGHT;   /* page forward */
        case SDLK_SPACE: return LV_KEY_ENTER;   /* A */
        case SDLK_RETURN: return LV_KEY_ENTER;  /* Start */
        case SDLK_LCTRL: return LV_KEY_ESC;     /* B */
        default: return 0;
    }
}

void lvgl_glue_feed_key(SDL_Keycode sym, SDL_bool pressed) {
    uint32_t key = sdl_key_to_lv(sym);
    if (key == 0) return;
    int next = (g_key_head + 1) % KEY_QUEUE_SIZE;
    if (next == g_key_tail) return; /* queue full, drop */
    g_key_queue[g_key_head].key = key;
    g_key_queue[g_key_head].state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    g_key_head = next;
}

static void read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    if (g_key_tail == g_key_head) {
        data->continue_reading = false;
        return;
    }
    data->key = g_key_queue[g_key_tail].key;
    data->state = g_key_queue[g_key_tail].state;
    g_key_tail = (g_key_tail + 1) % KEY_QUEUE_SIZE;
    data->continue_reading = (g_key_tail != g_key_head);
}

lv_group_t *lvgl_glue_init(SDL_Renderer *ren, int w, int h) {
    g_ren = ren;
    g_tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!g_tex) return NULL; /* caller logs SDL_GetError() */

    g_disp = lv_display_create(w, h);
    lv_display_set_color_format(g_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(g_disp, g_lv_buf, NULL, sizeof(g_lv_buf), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(g_disp, flush_cb);

    /* Deliberately NOT lv_group_set_default(group): that makes every
     * newly-created focusable widget (e.g. every lv_list button) join the
     * group automatically, which stole focus onto the first list button
     * instead of the screen's own container that our key_event_cb is
     * attached to — confirmed on-device (Up/Down/A all silently did
     * nothing, indev was receiving keys fine per the log, they just went
     * to the wrong object). Each screen explicitly adds exactly the one
     * object it wants focused via lv_group_add_obj(). */
    lv_group_t *group = lv_group_create();

    g_indev = lv_indev_create();
    lv_indev_set_type(g_indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(g_indev, read_cb);
    lv_indev_set_group(g_indev, group);

    return group;
}

void lvgl_glue_force_redraw(void) {
    lv_refr_now(g_disp);
}

void lvgl_glue_set_active_group(lv_group_t *group) {
    lv_indev_set_group(g_indev, group);
}
