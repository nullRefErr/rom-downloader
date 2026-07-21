/* Phase 3: archive.org ROM list per emulator. Reuses the exact SDL2/LVGL
 * setup validated in Phases 1-2 — see lvgl_glue.c for the device-specific
 * rendering/input quirks (texture-copy-only rendering, LockTexture not
 * UpdateTexture, raw LV_KEY_UP/DOWN not NEXT/PREV). */
#include <SDL2/SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include "lvgl.h"
#include "lvgl_glue.h"
#include "ui_emu_select.h"
#include "ui_rom_list.h"
#include "ui_update.h"
#include "ui_style.h"
#include "net_archive.h"
#include "net_update.h"

static FILE *g_log;
static lv_group_t *g_group;
static char **g_argv;

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
    logmsg("fetch done: ok=%d count=%d", list.ok, list.count);

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
    g_argv = argv;
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

    show_update_check();
    logmsg("update check / emu select screen built OK");

    int running = 1;
    unsigned frame = 0;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = 0;
            if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
                SDL_Keycode kc = ev.key.keysym.sym;
                SDL_bool pressed = (ev.type == SDL_KEYDOWN) ? SDL_TRUE : SDL_FALSE;
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
                        if (ui_rom_list_search_is_open()) ui_rom_list_search_backspace();
                        else ui_rom_list_open_search();
                    }
                }
                lvgl_glue_feed_key(kc, pressed);
            }
        }
        lv_tick_inc(16);
        lv_timer_handler();
        if (g_screen == SCREEN_ROM_LIST) ui_rom_list_tick();
        if (g_screen == SCREEN_UPDATE) {
            UiUpdateStatus st = ui_update_tick();
            if (st == UI_UPDATE_FINISHED) {
                lv_obj_clean(lv_screen_active());
                show_emu_select();
            } else if (st == UI_UPDATE_RESTART) {
                /* romdownloader.new already renamed over romdownloader by
                 * ui_update.c — SDL_Quit() first so the mmiyoo driver's
                 * device fds (framebuffer, input) are closed cleanly
                 * before execv, which carries open fds forward into the
                 * new process image and would otherwise fight the freshly
                 * exec'd binary's own SDL_Init() for the same devices. */
                logmsg("update: installed, restarting into new binary");
                fclose(g_log);
                SDL_DestroyRenderer(ren);
                SDL_DestroyWindow(win);
                SDL_Quit();
                execv(g_argv[0], g_argv);
                /* only reached if execv itself failed to launch */
                _exit(1);
            }
        }
        if (frame % 120 == 0) logmsg("heartbeat: frame=%u", frame);
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
