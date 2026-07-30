#include "net_archive.h"
#include "util.h"
#include "cJSON.h"
#include "auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *fetch_url(const char *url) {
    char cmd[1100], url_q[900];
    util_shell_quote(url, url_q, sizeof(url_q));
    /* -T 10: this runs synchronously on the main loop (main.c redraws
     * "Loading ..." and then blocks), so without a bound a half-open
     * connection freezes the whole app for BusyBox wget's 900s default —
     * unrecoverable on a handheld with no task switcher. net_update.c and
     * net_login.c already bound theirs; this copy was the one that didn't.
     * No -q, stderr to the log: otherwise a typo'd source url and a dead
     * wifi both surface as the same "Source unavailable right now". */
    snprintf(cmd, sizeof(cmd), "wget -T 10 -O - %s 2>>log.txt", url_q);
    return util_run_capture(cmd, NULL);
}

static int ends_with_ci(const char *s, const char *suffix) {
    size_t ls = strlen(s), lf = strlen(suffix);
    if (lf > ls) return 0;
    for (size_t i = 0; i < lf; i++) {
        char a = s[ls - lf + i], b = suffix[i];
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if (a != b) return 0;
    }
    return 1;
}

RomList net_archive_fetch(const char *list_url, const char *ext) {
    RomList result = {0};

    char *json = fetch_url(list_url);
    if (!json) return result; /* ok=0: network/wget failure */

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root) return result; /* ok=0: unparseable response */

    /* archive.org can move an item into its "logged-in users only" access
     * tier: metadata (and therefore the file list) stays public, but every
     * actual download answers 401 Unauthorized. That's exactly what happened
     * to the PlayStation source — the app happily listed thousands of roms
     * that could never be fetched, and each attempt surfaced only a generic
     * "Download failed". Detect it here, at the fetch, so the UI can say what
     * is really wrong; the marker is membership of the "loggedin" collection
     * (verified: the restricted PS item has it, every working source does
     * not). Checked before files[] so it reports even when both apply. */
    cJSON *meta = cJSON_GetObjectItemCaseSensitive(root, "metadata");
    cJSON *collection = meta ? cJSON_GetObjectItemCaseSensitive(meta, "collection") : NULL;
    if (cJSON_IsString(collection) && collection->valuestring) {
        if (strcmp(collection->valuestring, "loggedin") == 0) result.restricted = 1;
    } else if (cJSON_IsArray(collection)) {
        cJSON *c;
        cJSON_ArrayForEach(c, collection) {
            if (cJSON_IsString(c) && c->valuestring && strcmp(c->valuestring, "loggedin") == 0) {
                result.restricted = 1;
                break;
            }
        }
    }

    if (result.restricted && !auth_available()) {
        /* Anonymous: deliberately do NOT return the file list, since every
         * one of those downloads would 401 — listing them only invites
         * failures. With cookies configured the item is reachable, so fall
         * through and list it normally. */
        cJSON_Delete(root);
        return result; /* ok=0, restricted=1 */
    }

    cJSON *files = cJSON_GetObjectItemCaseSensitive(root, "files");
    if (!cJSON_IsArray(files)) {
        /* dark or flagged item: archive.org omits "files" entirely rather
         * than returning an empty array — this is the expected, permanent
         * "source unavailable" state for some entries, not a bug. */
        cJSON_Delete(root);
        return result;
    }

    int cap = 64;
    result.items = malloc((size_t)cap * sizeof(RomEntry));
    if (!result.items) {
        cJSON_Delete(root);
        return result;
    }

    char dotext[16];
    snprintf(dotext, sizeof(dotext), ".%s", ext);

    cJSON *file;
    cJSON_ArrayForEach(file, files) {
        cJSON *name = cJSON_GetObjectItemCaseSensitive(file, "name");
        if (!cJSON_IsString(name) || !name->valuestring) continue;
        if (!ends_with_ci(name->valuestring, dotext)) continue;

        if (result.count == cap) {
            cap *= 2;
            RomEntry *nitems = realloc(result.items, (size_t)cap * sizeof(RomEntry));
            if (!nitems) break;
            result.items = nitems;
        }
        cJSON *size = cJSON_GetObjectItemCaseSensitive(file, "size");
        unsigned long size_bytes = 0;
        if (cJSON_IsString(size) && size->valuestring) {
            size_bytes = strtoul(size->valuestring, NULL, 10);
        } else if (cJSON_IsNumber(size)) {
            size_bytes = (unsigned long)size->valuedouble;
        }

        result.items[result.count].name = strdup(name->valuestring);
        result.items[result.count].size = size_bytes;
        result.count++;
    }

    cJSON_Delete(root);
    result.ok = 1;
    return result;
}

void net_archive_free(RomList *list) {
    for (int i = 0; i < list->count; i++) free(list->items[i].name);
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->ok = 0;
}
