# Changelog

## v1.3.0

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
