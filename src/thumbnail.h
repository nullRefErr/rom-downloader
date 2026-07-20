#ifndef THUMBNAIL_H
#define THUMBNAIL_H

#include "lvgl.h"
#include <stdbool.h>

/* Best-effort box-art fetch from libretro-thumbnails (raw.githubusercontent.com),
 * matched by exact filename against `rom_name` (extension stripped) — most
 * titles won't have an exact match, that's expected and handled by hiding
 * `img` rather than erroring. `thumb_repo` is the libretro-thumbnails repo
 * name for the system (e.g. "Sony_-_PlayStation"); pass NULL to skip.
 *
 * Split into a fast, no-network path and a slow, blocking-on-wget path so
 * callers can check the cache immediately on every selection change (near
 * instant — the "cover art loads slowly" complaint was mostly this: even
 * cache hits waited out the same debounce meant for network requests) and
 * only debounce the actual network fetch. */

/* Loads from the on-disk cache only, no network. Returns true (and shows
 * `img`) on a cache hit, false (leaves `img` hidden) on a miss. */
bool thumbnail_try_cache(lv_obj_t *img, const char *thumb_repo, const char *rom_name);

/* Fetches over the network (blocking), caches to disk, and displays.
 * Call only after debouncing — see ui_rom_list.c. */
void thumbnail_fetch_network(lv_obj_t *img, const char *thumb_repo, const char *rom_name);

#endif
