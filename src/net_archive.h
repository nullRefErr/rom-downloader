#ifndef NET_ARCHIVE_H
#define NET_ARCHIVE_H

typedef struct {
    char *name;        /* filename as listed in the archive.org item, e.g. "Foo (USA).7z" */
    unsigned long size; /* bytes, 0 if archive.org didn't report one */
} RomEntry;

typedef struct {
    RomEntry *items;
    int count;
    int ok;         /* 0: fetch failed, or the source has no files[] (dark/flagged item) */
    int restricted; /* 1: the item is listable but archive.org only serves its files to
                     * logged-in accounts, so every download would 401. Distinct from
                     * ok==0 so the UI can say why instead of "source unavailable". */
} RomList;

/* Fetches https://archive.org/metadata/{identifier} via the system `wget`
 * (same approach as the Phase 1 network smoke test), parses the `files[]`
 * array with cJSON, and returns entries whose name ends in `.ext`
 * (case-insensitive). A missing/absent `files` key (archive.org dark or
 * flagged items omit it entirely, not an empty array) is reported via
 * RomList.ok == 0, not a crash or a silently empty-but-"ok" list. */
RomList net_archive_fetch(const char *identifier, const char *ext);

void net_archive_free(RomList *list);

#endif
