#ifndef EMU_TABLE_H
#define EMU_TABLE_H

/* One rom source. Fields are owned arrays rather than pointers to literals
 * because these are loaded from sources.json at runtime — the compiled-in
 * table below is only the fallback used when that file is absent. */
typedef struct {
    char code[16];       /* Onion Roms/<code> folder name */
    char label[48];      /* shown in the UI */

    /* Exactly one of these identifies where the list comes from:
     *   archive_id — an archive.org item; metadata and downloads use its
     *                well-known endpoints.
     *   url        — any URL returning {"files":[{"name":..,"size":..}]}.
     *                That is a subset of what archive.org's metadata API
     *                already returns, so one parser serves both and someone
     *                self-hosting only has to publish a small JSON file. */
    char archive_id[96];
    char url[300];
    char base[300];      /* download prefix for a url source; derived from
                          * url's directory when left empty */

    char ext[8];         /* rom file extension to list */
    char thumb_repo[80]; /* libretro-thumbnails repo, empty = no box art */
    int available;       /* 0 = known dead, shown greyed out and unselectable */
} EmuEntry;

/* Built-in defaults. sources.c copies these when sources.json is missing. */
extern const EmuEntry EMU_DEFAULTS[];
extern const int EMU_DEFAULTS_COUNT;

#endif
