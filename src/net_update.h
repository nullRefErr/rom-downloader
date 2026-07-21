#ifndef NET_UPDATE_H
#define NET_UPDATE_H

#include <stdbool.h>

typedef struct {
    char latest_tag[32];      /* e.g. "v1.3.0" */
    char download_url[512];   /* browser_download_url of the "romdownloader" asset */
    unsigned long size;       /* asset size in bytes, for the progress bar */
    bool update_available;    /* latest_tag differs from APP_VERSION AND a matching asset was found */
    bool ok;                  /* false on any network/parse failure — treat as "no update", never blocks startup */
} UpdateCheckResult;

/* Blocking check against GitHub's releases API — same wget+cJSON pattern
 * as net_archive_fetch(), just a much smaller response. Safe to call on
 * every launch: a bad/missing network just yields ok=false. */
UpdateCheckResult net_update_check(void);

#endif
