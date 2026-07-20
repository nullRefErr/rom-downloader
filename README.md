# rom-downloader

Native GUI ROM downloader app for Onion OS (Miyoo Mini Plus).

## Screenshots

| System select | ROM list + box art |
|---|---|
| ![System select](docs/screenshots/emu-select.png) | ![ROM list](docs/screenshots/rom-list.png) |

## Installation

1. Download `rom-downloader-<version>.zip` from the
   [Releases](https://github.com/nullRefErr/rom-downloader/releases) page.
2. Extract it onto your SD card's root — it contains an `App/RomDownloader/`
   folder that merges into your existing `App/` folder (same layout Onion
   itself uses for every app).
3. Reboot the device (or rescan Apps), then launch **Rom Downloader By AEY**
   from the Apps menu.
4. Pick a system (currently PlayStation and Genesis — the only two
   archive.org source collections that are alive; others are grayed out),
   browse or press Y to search, press A on a rom to confirm and download.
   It lands directly in `Roms/<system>/` and shows up in the emulator's
   game list without a manual rescan.
