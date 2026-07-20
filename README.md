# rom-downloader

Native GUI ROM downloader app for Onion OS (Miyoo Mini Plus). See
`/Users/eren/.claude/plans/compressed-herding-twilight.md` for the full plan.

## Status: Phase 2 in progress — LVGL emulator-select screen working on real hardware

Phase 1 (packaging + SDL2 window/input/network smoke test) is done — see
"Phase 1 findings" below. Phase 2 (LVGL UI) has the emulator-select screen
rendering and keypad-navigable on real hardware: `src/lvgl_glue.c` wires
LVGL into the SDL2/MMIYOO setup, `src/ui_emu_select.c` builds an `lv_list`
from `src/emu_table.c`'s static table (PS/Genesis selectable, GB/GBC/GBA/
NES/SNES grayed out per confirmed-dead archive.org sources), and D-pad
Up/Down + A(select)/B(back) navigate it.

### Phase 2 findings — two more device-specific gotchas, both confirmed on hardware

1. **This driver's `SDL_UpdateTexture` doesn't actually copy pixel data —
   it just remembers a pointer.** `MMIYOO_UpdateTexture` (same
   `SDL_render_mmiyoo.c` source as the Phase 1 finding) ignores the
   sub-rect it's given and calls `update_texture()`, which stores whatever
   buffer pointer you passed it — it never memcpy's anything. LVGL's
   default `LV_DISPLAY_RENDER_MODE_PARTIAL` reuses ONE small scratch buffer
   across many chunks (rendering the screen in strips), so by the time the
   actual GPU blit runs (asynchronously, see below), that remembered
   pointer only ever contains the LAST strip's pixels — the rest is
   whatever garbage happened to be there. Fix, in `lvgl_glue.c`:
   - `LV_DISPLAY_RENDER_MODE_FULL` with one persistent full-screen buffer
     (`g_lv_buf`, 640×480×2 bytes) — one flush per refresh, not many strips.
   - Flush via `SDL_LockTexture`/`memcpy`/`SDL_UnlockTexture`, **not**
     `SDL_UpdateTexture` — `MMIYOO_LockTexture` returns the texture's own
     persistent internal buffer to write into directly (this is the exact
     mechanism Phase 1's manual solid-color fill used, which is why that
     one worked from the start).
   - The actual hardware blit+flip happens on a **background pthread**
     (`video_handler` in `SDL_video_mmiyoo.c`, watching
     `gfx.action == GFX_ACTION_FLIP`), started automatically inside
     `SDL_Init(SDL_INIT_VIDEO)` — not something we need to set up ourselves,
     but worth knowing it's async, not synchronous inside `RenderPresent`.
2. **LVGL's keypad indev does NOT move group focus on `LV_KEY_UP`/`DOWN`.**
   Verified in `lv_indev.c`: only `LV_KEY_NEXT`/`LV_KEY_PREV` call
   `lv_group_focus_next`/`_prev`; `UP`/`DOWN`/`LEFT`/`RIGHT` are meant to be
   consumed by the focused widget itself (sliders, rollers — not plain
   buttons). Mapping D-pad Down/Up to `LV_KEY_UP`/`DOWN` compiled fine, ran
   fine, keys were confirmed arriving in the log — and still did nothing,
   because nothing ever consumed them. Fixed in `lvgl_glue.c`'s
   `sdl_key_to_lv()`: Down → `LV_KEY_NEXT`, Up → `LV_KEY_PREV`.

Only the 2 currently-available table entries (PS, Genesis) are added to the
keypad nav group — confirmed with the user this is the intended behavior,
not a bug: dead archive.org sources show grayed out but are deliberately
unreachable by navigation, not selectable-but-erroring.

## Phase 1 findings

`src/main.c`'s first version just opened an SDL2 window, drew a solid color
that changed on every button press, logged every button press (as SDL
keycodes) to `log.txt`, and did one `wget` request to archive.org. This
validated the whole toolchain/packaging story and surfaced:

- The `MMIYOO` SDL2 render driver on this device (source:
  [XK9274/sdl2_miyoo](https://github.com/XK9274/sdl2_miyoo),
  `sdl2/src/render/mmiyoo/SDL_render_mmiyoo.c`) implements
  `QueueFillRects`/`QueueDrawPoints`/`QueueGeometry`/`QueueCopyEx` as hard
  no-ops. `SDL_RenderClear`+`SDL_SetRenderDrawColor` never reach the
  screen, silently — no error, no crash, `RenderPresent` returns fine every
  frame. Only a texture copy (`QueueCopy` → `GFX_Copy`) actually draws.
- `SDL_CreateWindow` needs `SDL_WINDOW_FULLSCREEN` — no window manager on
  this device, a plain windowed surface has nothing to flip to.
- Request the renderer with `SDL_RENDERER_ACCELERATED` explicitly — this
  build registers two render drivers, `software` (index 0, never reaches
  the screen) and `MMIYOO` (index 1, the one that does);
  `SDL_CreateRenderer(win, -1, 0)` picks `software`.

### SDL2: linked against the system's own copy, nothing bundled

First attempt bundled DraStic's `libSDL2-2.0.so.0` — it turned out to be
tightly coupled to DraStic itself (calls into `dtr_*`/`detour_*`/`volume_*`
symbols from a `libdtr.so` shim, apparently a runtime binary-patching/hook
mechanism), and crashed with SIGSEGV inside `SDL_Init()` when used from a
different app. Confirmed on-device via `crash.txt`.

Fixed by linking against `.tmp_update/lib/parasyte/libSDL2-2.0.so.0` instead
— the SDL2 build Onion's own MainUI (`parasyte`) uses to render its menus.
Verified via `readelf --dyn-syms` to have no DraStic-specific undefined
symbols. `third_party/parasyte-libSDL2-2.0.so.0` (copied from the user's SD
card) is the link-time stand-in; at runtime `launch.sh` points
`LD_LIBRARY_PATH` at `/mnt/SDCARD/.tmp_update/lib/parasyte` directly — no
`lib/` folder shipped in the app package at all. Its own dependencies
(`libEGL.so.1`, `libGLESv2.so.2`, and the vendor `libmi_*.so` SigmaStar GPU
SDK libs) aren't on the SD card either — they live in the device's internal
firmware and are only reachable by running code on-device, which is exactly
why MainUI's own copy was the safer choice: it's proven to already resolve
all of that correctly, right now, unlike anything hand-picked from GitHub.

## Build

- `make host` — native build for quick iteration on this machine (uses
  Homebrew SDL2). Run `build/romdownloader-host` directly, press ESC to quit.
- `make device` — cross-compiles for the Miyoo Mini Plus via Docker
  (`shauninman/union-miyoomini-toolchain`, image tag `miyoomini-toolchain`,
  built separately — see `toolchain/build-device.sh` header comment). Output
  goes straight into `package/App/RomDownloader/romdownloader`. Compiles
  `src/*.c` plus vendored LVGL (`third_party/lvgl/src/**/*.c`, excluding
  `drivers/` — that's platform glue for other OSes/toolkits we don't use,
  we provide our own in `src/lvgl_glue.c`).

## Vendored LVGL (`third_party/lvgl/`, `third_party/lv_conf.h`)

v9.5.0 source only (no docs/examples/tests), `src/libs/` kept even though
we don't use most of it — LVGL's own master header unconditionally
`#include`s some of those headers regardless of which `LV_USE_*` flags are
on, deleting it breaks the build. Config: RGB565 color depth (matches the
device's native format), `LV_USE_OS` none (single-threaded, we drive
`lv_timer_handler()` from our own loop), otherwise template defaults.

## Testing on device

SSH is enabled on the dev's device (Tweaks → Network → SSH), credentials
`onion`/`onion` — this is the fast path. **IP changes across sessions**
(DHCP / different networks — seen both a home-router range and a
phone-hotspot range) — always ask the user for the current one shown in
Tweaks → Network, don't assume a previously-used IP still works:

```
sshpass -p onion scp package/App/RomDownloader/romdownloader onion@<device-ip>:/mnt/SDCARD/App/RomDownloader/romdownloader.new
sshpass -p onion ssh onion@<device-ip> 'mv .../romdownloader.new .../romdownloader; chmod +x .../romdownloader'
```

Deploy to a `.new` name and `mv` over the original, not a direct `scp` to
the final name — if the app is currently running on the device, overwriting
its executable in place fails with `ETXTBSY` ("Failure" from scp); `mv`
doesn't have that problem since it just repoints the directory entry.

Then have the user launch it via the real Apps menu (SSH-launched runs
behave differently — MainUI doesn't yield focus the same way, seen to exit
on their own after a few seconds even with no crash) — SSH is for shipping
the binary and reading `log.txt`/`crash.txt` back afterward, not for
representative test runs. Read logs with:

```
sshpass -p onion ssh onion@<device-ip> 'cd /mnt/SDCARD/App/RomDownloader && cat log.txt crash.txt'
```

Fallback if SSH isn't available: copy the `App/` folder onto the SD card
directly (mounts as a FAT32 volume, e.g. `/Volumes/UNTITLED` on macOS —
clean up the `._*` AppleDouble files `cp` leaves behind, they're harmless
but clutter the card), same log files apply.

On-device test checklist: Apps → "ROM Downloader (smoketest)" → D-pad
Down/Up moves the highlighted selection between PlayStation and Genesis
(the only two currently-available entries) → A selects → B/Menu exits.
