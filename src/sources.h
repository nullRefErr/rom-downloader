#ifndef SOURCES_H
#define SOURCES_H

#include "emu_table.h"
#include <stddef.h>

/* The live rom-source list.
 *
 * Read from sources.json next to the app, falling back to the compiled-in
 * defaults when that file is missing or unreadable — the same arrangement as
 * lang.json, and for the same reason: this is data that goes stale on someone
 * else's schedule. Sources get taken down (five did during development) or
 * move behind a login (the PlayStation one did), and repointing one should
 * not require a new release. It also lets someone list a collection they host
 * themselves instead of the defaults.
 *
 * Editable from Settings, and saved back to sources.json. */

void sources_load(void);
void sources_save(void);

int sources_count(void);
const EmuEntry *sources_get(int i);

/* Replace one entry's source. `text` is either an archive.org identifier or a
 * URL — anything containing "://" is treated as a URL. Persists immediately. */
void sources_set(int i, const char *text);

/* The source as one editable string: the URL if there is one, else the
 * archive.org identifier. */
const char *sources_text(int i);

void sources_set_available(int i, int available);

/* Restores the compiled-in defaults and deletes sources.json. */
void sources_reset(void);

/* Where a listing is fetched from: the archive.org metadata endpoint, or the
 * configured URL as-is. */
void sources_list_url(const EmuEntry *e, char *out, size_t outsz);

/* Prefix that a file name is appended to for downloading. */
void sources_download_base(const EmuEntry *e, char *out, size_t outsz);

#endif
