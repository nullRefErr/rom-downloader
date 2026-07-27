/* Phase 3: archive.org ROM list per emulator. Reuses the exact SDL2/LVGL
 * setup validated in Phases 1-2 — see lvgl_glue.c for the device-specific
 * rendering/input quirks (texture-copy-only rendering, LockTexture not
 * UpdateTexture, raw LV_KEY_UP/DOWN not NEXT/PREV). */
#include <SDL2/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include "lvgl.h"
#include "lvgl_glue.h"
#include "ui_emu_select.h"
#include "ui_rom_list.h"
#include "ui_update.h"
#include "ui_chrome.h"
#include "ui_style.h"
#include "net_archive.h"
#include "net_update.h"
#include "download_queue.h"
#include "auth.h"
#include "ui_login.h"
#include "env.h"
#include "sound.h"
#include "net_login.h"

/* Select button — opens the region/favourites filter overlay on the rom
 * list. This SDL keycode is the reasonable-default assumption for Select on
 * this device (its exact mapping wasn't in the Phase 1 keylog, only the
 * face/d-pad/Start/Menu buttons were); every keydown is logged, so it's
 * trivial to confirm/correct against a real Select press. */
#define KEY_SELECT SDLK_RCTRL

/* Holding a direction does nothing on its own here: this device's input
 * driver delivers one press and one release, with no auto-repeat in
 * between, so a held d-pad moved the selection exactly once. Synthesised
 * below rather than relying on the driver or on LVGL's long-press handling,
 * which never sees a sustained PRESSED state through our event queue.
 * Directions only — repeating A would fire a download per tick, and
 * repeating B would walk back through screens. */
#define KEY_REPEAT_DELAY_MS 350 /* hold this long before it starts */
#define KEY_REPEAT_RATE_MS  90  /* then one step this often */

static bool key_repeats(SDL_Keycode k) {
    return k == SDLK_UP || k == SDLK_DOWN || k == SDLK_LEFT || k == SDLK_RIGHT;
}

static FILE *g_log;
static lv_group_t *g_group;

typedef enum { SCREEN_UPDATE, SCREEN_EMU_SELECT, SCREEN_ROM_LIST } Screen;
static Screen g_screen;

static void logmsg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fputc('\n', g_log);
    fflush(g_log);
}

static void show_emu_select(void);

static void on_back_to_emu_select(void) {
    ui_rom_list_destroy();
    lv_obj_clean(lv_screen_active());
    show_emu_select();
}

static void on_emu_selected(const EmuEntry *emu) {
    /* net_archive_fetch() blocks on wget for a few seconds — show a
     * loading label and force it to actually reach the screen (LVGL would
     * otherwise just queue the redraw and flush it after the blocking call
     * already returned, i.e. never visibly show "loading" at all). */
    lv_obj_clean(lv_screen_active());
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
    lv_obj_t *loading = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_color(loading, lv_color_hex(0xffffff), 0);
    char buf[64];
    snprintf(buf, sizeof(buf), "Loading %s...", emu->label);
    lv_label_set_text(loading, buf);
    lv_obj_center(loading);
    lvgl_glue_force_redraw();

    logmsg("fetching %s (%s.%s)...", emu->code, emu->archive_id, emu->ext);
    RomList list = net_archive_fetch(emu->archive_id, emu->ext);
    logmsg("fetch done: ok=%d count=%d restricted=%d", list.ok, list.count, list.restricted);

    lv_obj_clean(lv_screen_active());
    ui_rom_list_build(g_group, emu, list, on_back_to_emu_select);
    g_screen = SCREEN_ROM_LIST;
}

static void show_emu_select(void) {
    ui_emu_select_build(g_group, on_emu_selected);
    g_screen = SCREEN_EMU_SELECT;
}

/* Blocking GitHub Releases check (small JSON response, 5s timeout ceiling
 * inside net_update_check) — same "Loading..." pattern as
 * on_emu_selected()'s archive.org fetch. Skips straight to the emulator
 * list whenever there's nothing to show, so a declined/failed/no-update
 * check never adds a screen the user has to click past. */
static void show_update_check(void) {
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
    lv_obj_t *checking = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_color(checking, lv_color_hex(0xffffff), 0);
    lv_label_set_text(checking, "Checking for updates...");
    lv_obj_center(checking);
    lvgl_glue_force_redraw();

    UpdateCheckResult upd = net_update_check();
    logmsg("update check: ok=%d available=%d latest=%s", upd.ok, upd.update_available, upd.latest_tag);

    lv_obj_clean(lv_screen_active());
    if (upd.ok && upd.update_available) {
        ui_update_build(g_group, &upd);
        g_screen = SCREEN_UPDATE;
    } else {
        show_emu_select();
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    g_log = fopen("log.txt", "a");
    if (!g_log) g_log = stderr;
    logmsg("=== romdownloader phase3 start ===");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        logmsg("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    logmsg("SDL video driver: %s", SDL_GetCurrentVideoDriver());

    SDL_Window *win = SDL_CreateWindow("romdownloader",
                                        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                        640, 480, SDL_WINDOW_FULLSCREEN);
    if (!win) {
        logmsg("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) {
        logmsg("SDL_CreateRenderer failed: %s", SDL_GetError());
        return 1;
    }
    SDL_RendererInfo rinfo;
    if (SDL_GetRendererInfo(ren, &rinfo) == 0) {
        logmsg("window+renderer created OK, using driver: %s", rinfo.name);
    }

    lv_init();
    ui_style_init();
    g_group = lvgl_glue_init(ren, 640, 480);
    if (!g_group) {
        logmsg("lvgl_glue_init failed: %s", SDL_GetError());
        return 1;
    }
    auth_init();   /* optional archive.org cookies, see auth.h */
    queue_reset(); /* once at startup — the queue outlives individual screens */
    logmsg("lvgl init OK");

    /* splash */
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);
    lv_obj_t *splash = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_color(splash, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(splash, &lv_font_montserrat_24, 0);
    lv_label_set_text(splash, "Rom Downloader\nBy AEY");
    lv_obj_set_style_text_align(splash, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(splash);
    lvgl_glue_force_redraw();
    SDL_Delay(1200);
    lv_obj_clean(lv_screen_active());

    sound_init(); /* silent no-op if audio can't be opened — never fatal */

    /* If credentials were left in .env, establish the session before the user
     * goes looking for a restricted source, so it just works. Only when there
     * is no usable session already — a stored cookie is preferred and costs
     * nothing, this path is the renewal for when it has expired. */
    if (!auth_available() && env_exists()) {
        char email[256], password[256];
        if (env_load(email, sizeof(email), password, sizeof(password))) {
            lv_obj_t *note = lv_label_create(lv_screen_active());
            lv_obj_set_style_text_color(note, lv_color_hex(0xffffff), 0);
            lv_label_set_text(note, "Signing in to archive.org...");
            lv_obj_center(note);
            lvgl_glue_force_redraw();
            LoginResult lr = net_login(email, password);
            logmsg("env sign-in: ok=%d (%s)", lr.ok, lr.message);
            lv_obj_clean(lv_screen_active());
        }
    }

    show_update_check();
    logmsg("update check / emu select screen built OK");

    int running = 1;
    unsigned frame = 0;
    SDL_Keycode held = 0;
    Uint32 held_since = 0, last_repeat = 0;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
                SDL_Keycode kc = ev.key.keysym.sym;
                SDL_bool pressed = (ev.type == SDL_KEYDOWN) ? SDL_TRUE : SDL_FALSE;
                /* Ignore any auto-repeat the driver does produce, so it
                 * can't stack with the one synthesised below. */
                if (ev.type == SDL_KEYDOWN && ev.key.repeat) continue;
                if (ev.type == SDL_KEYDOWN && key_repeats(kc)) {
                    held = kc;
                    held_since = SDL_GetTicks();
                    last_repeat = 0;
                } else if (ev.type == SDL_KEYUP && kc == held) {
                    held = 0;
                }
                if (ev.type == SDL_KEYDOWN) {
                    logmsg("KEYDOWN sym=%d (0x%x) name=%s", (int)kc, (int)kc, SDL_GetKeyName(kc));
                    /* Start on device, Return on host. Was Menu/ESC, but
                     * that made the device's own MENU+X screenshot
                     * shortcut unusable — holding Menu quit the app before
                     * X could be pressed (reported directly). Menu is now
                     * unclaimed by the app entirely. */
                    if (kc == SDLK_RETURN) running = 0;
                    if (kc == SDLK_LALT && g_screen == SCREEN_ROM_LIST) { /* Y */
                        /* Y opens search; once search is open, reaching the
                         * on-screen keyboard's own tiny backspace key by
                         * D-pad felt too tedious (reported directly) — Y
                         * doubles as a direct backspace shortcut instead. */
                        if (ui_login_is_open()) ui_login_backspace();
                        else if (ui_rom_list_search_is_open()) ui_rom_list_search_backspace();
                        else ui_rom_list_open_search();
                    }
                    if (kc == SDLK_LSHIFT) { /* X */
                        if (ui_login_is_open()) ui_login_clear_field();
                        else if (g_screen == SCREEN_ROM_LIST) ui_rom_list_toggle_favorite();
                    }
                    if (kc == KEY_SELECT && g_screen == SCREEN_ROM_LIST) { /* Select */
                        ui_rom_list_open_filter();
                    }
                }
                lvgl_glue_feed_key(kc, pressed);
            }
        }
        if (held) {
            Uint32 now = SDL_GetTicks();
            if (now - held_since >= KEY_REPEAT_DELAY_MS &&
                now - last_repeat >= KEY_REPEAT_RATE_MS) {
                last_repeat = now;
                /* a full press/release pair, since the indev consumes
                 * discrete events rather than a sustained state */
                lvgl_glue_feed_key_quiet(held, SDL_TRUE);
                lvgl_glue_feed_key_quiet(held, SDL_FALSE);
            }
        }

        lv_tick_inc(16);
        lv_timer_handler();
        /* Drive the download queue every frame EXCEPT on the update screen,
         * where ui_update.c owns the (shared, single) download state machine
         * directly — letting the queue poll it too would race for the DONE
         * transition. The queue is empty during the startup update prompt
         * anyway, but gating it here keeps that guarantee explicit. */
        if (g_screen != SCREEN_UPDATE) queue_tick();
        ui_login_tick();
        if (g_screen == SCREEN_ROM_LIST) ui_rom_list_tick();
        if (g_screen == SCREEN_UPDATE) {
            UiUpdateStatus st = ui_update_tick();
            if (st == UI_UPDATE_FINISHED) {
                lv_obj_clean(lv_screen_active());
                show_emu_select();
            } else if (st == UI_UPDATE_RESTART) {
                /* The new binary is staged as romdownloader.new (NOT swapped
                 * in yet — can't replace a running exe on FAT32). Exit
                 * cleanly; launch.sh's loop sees romdownloader.new, swaps it
                 * over romdownloader (safe now, nothing is executing it) and
                 * relaunches. We do NOT execv here — that would just re-run
                 * the OLD binary, since the swap hasn't happened. */
                logmsg("update: staged romdownloader.new, exiting for launch.sh to swap+relaunch");
                running = 0;
            }
        }
        if (frame % 120 == 0) {
            logmsg("heartbeat: frame=%u", frame);
            ui_chrome_refresh_status(); /* ~2s cadence: Wi-Fi + free space */
        }
        frame++;
        SDL_Delay(16);
    }

    logmsg("=== romdownloader phase3 exit ===");
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    fclose(g_log);
    return 0;
}
