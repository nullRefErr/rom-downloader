# Changelog

## v1.4.0

- **archive.org sign-in.** Some sources (PlayStation right now) are only served to logged-in accounts, so anonymous downloads failed with a bare "Download failed". You can now sign in with your own account from inside the app, and the app says plainly when a source needs one. Credentials can also be left in a `.env` file so the session renews itself; see `.env.example` — note it stores your password in plain text on the card, and the cookie-only route in `archive_cookies.txt.example` stays the safer option.
- **Wi-Fi indicator actually reflects the connection.** It previously showed connected at all times, because a switched-off interface keeps its status entry with zeroed values. It now reports the real state and a link strength that moves with the signal, and free SD space has its own icon so it no longer reads as a connection speed.
- Downloads that were interrupted no longer get stuck failing forever, and a resumed file is checked against its expected size before being accepted, so a partial transfer can't be reported as a finished rom.
- Failures now record the actual reason to `log.txt` instead of being silently discarded.

## v1.3.3

- **Auto-update now actually installs.** The updater downloaded the new build fine but couldn't swap it in — the SD card is FAT32, which can't replace a running executable in place, so the install step failed ("Update failed"). The new binary is now staged and swapped in by `launch.sh` on the next start, when nothing is running it.
  - Note: because the broken part *was* the updater's install step, existing installs (v1.3.2 and earlier) must install this version once by hand (extract the zip to the SD card). From v1.3.3 on, in-app updates apply on their own.

## v1.3.2

- **Added 5 new console systems**: Sega Game Gear (`GG`), Sega Master System (`MS`), PC Engine (`PCE`), Atari 2600 (`2600`), and Atari 7800 (`7800`) are now fully supported with working Archive.org collections.

## v1.3.1

Fixes on top of the v1.3.0 feature set (v1.3.0 was pulled before release):

- **Downloads fixed on BusyBox wget**: an unsupported flag made every download fail the instant it started; now uses a supported form and logs the real failure reason to `log.txt`.
- **No more freeze after a successful download**: the game-list cache refresh now runs in the background instead of blocking the UI (was noticeable on systems with a large existing library).
- **Clearer on-screen keyboard**: the focused key is now green, distinct from the special keys.

### Features (from v1.3.0)

- **Disk space indicator**: free SD space shown in the header, updated after each download.
- **Wi-Fi indicator**: connection status shown as a Wi-Fi symbol in the header (green connected, red disconnected).
- **Download queue**: queue multiple roms — they download one after another in the background and keep going even if you back out of the list.
- **Resume**: an interrupted download (wifi drop, app closed) picks up where it left off instead of restarting.
- **Region filter**: filter the list by USA / Europe / Japan / World, via the Select button.
- **Favourites**: mark roms with X (shown with a `*`), and filter to favourites-only. Saved per system.

## v1.2.0

- **Self-updater**: the app now checks GitHub Releases on startup and, if a newer version is published, shows an "Update Available" prompt — press A to download and install in place, B to skip. No manual SD card copying needed for future updates.
- All 7 systems enabled (Game Boy, Game Boy Color, Game Boy Advance, NES, SNES rejoin PlayStation and Genesis) with working archive.org sources.
- Fixed a shell-quoting bug that silently failed downloads for any rom title containing an apostrophe (150+ in the PS1 catalog, 300+ in Genesis).

## v1.1.0

- Download confirmation dialog: A now opens a "Download this rom?" prompt (name + size) before starting — A confirms, B cancels.
- Fixed the green "Downloaded" label showing twice, stacked, right after a download finished.
- Quit is now bound to Start instead of Menu, freeing Menu for the device's own MENU+X screenshot shortcut.
- README: added end-user install instructions and real screenshots.

## v1.0.0

- Initial release: browse and search PlayStation and Genesis catalogs from archive.org, preview box art, and download straight into the right `Roms/<system>/` folder.
- Virtualized, searchable ROM list with box-art preview and paging.
- Background download with progress bar and a persistent "Downloaded" indicator.
- Automatic game-list refresh so new roms show up immediately in the emulator's own list.
