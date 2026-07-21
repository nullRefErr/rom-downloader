#ifndef FAVORITES_H
#define FAVORITES_H

#include <stdbool.h>

/* Per-emulator favourites, persisted as a plain newline-separated list of
 * rom display names at /mnt/SDCARD/App/RomDownloader/favorites_<code>.txt.
 * Flat text (not JSON/db) on purpose — a handful of names per system that
 * a human might even edit by hand. */

/* Load the favourites for `emu_code` into memory, replacing whatever was
 * loaded before. Call once when a rom-list screen is built. */
void favorites_load(const char *emu_code);

/* Is this rom display name currently favourited? */
bool favorites_is(const char *display_name);

/* Toggle favourite status of `display_name` and persist immediately. */
void favorites_toggle(const char *display_name);

#endif
