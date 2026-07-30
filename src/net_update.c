#include "net_update.h"
#include "version.h"
#include "cJSON.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL "https://api.github.com/repos/nullRefErr/rom-downloader/releases/latest"
#define ASSET_NAME "romdownloader"

static char *fetch_url(const char *url) {
    char cmd[1100], url_q[900];
    util_shell_quote(url, url_q, sizeof(url_q));
    /* -T 5 caps how long a bad or absent network can stall startup (this
     * runs on every launch); -U is required or GitHub answers 403. */
    snprintf(cmd, sizeof(cmd), "wget -q -T 5 -O - -U 'rom-downloader' %s 2>/dev/null", url_q);
    return util_run_capture(cmd, NULL);
}

/* Numeric X.Y.Z compare, not string inequality — a naive "!=" would flag
 * "update available" whenever the running build's APP_VERSION is AHEAD of
 * the last published tag too (true right after bumping version.h for a
 * release that hasn't been cut yet), and accepting that "update" would
 * actually downgrade the user to the older published binary. */
static int version_cmp(const char *a, const char *b) {
    while (*a || *b) {
        int na = 0, nb = 0;
        while (*a >= '0' && *a <= '9') na = na * 10 + (*a++ - '0');
        while (*b >= '0' && *b <= '9') nb = nb * 10 + (*b++ - '0');
        if (na != nb) return na < nb ? -1 : 1;
        if (*a == '.') a++;
        if (*b == '.') b++;
    }
    return 0;
}

UpdateCheckResult net_update_check(void) {
    UpdateCheckResult result = {0};

    char *json = fetch_url(API_URL);
    if (!json) return result; /* ok=false: offline/no network — not an error state to show */

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root) return result;

    cJSON *tag = cJSON_GetObjectItemCaseSensitive(root, "tag_name");
    if (!cJSON_IsString(tag) || !tag->valuestring) {
        cJSON_Delete(root);
        return result;
    }
    snprintf(result.latest_tag, sizeof(result.latest_tag), "%s", tag->valuestring);

    cJSON *assets = cJSON_GetObjectItemCaseSensitive(root, "assets");
    cJSON *asset;
    cJSON_ArrayForEach(asset, assets) {
        cJSON *name = cJSON_GetObjectItemCaseSensitive(asset, "name");
        if (!cJSON_IsString(name) || !name->valuestring) continue;
        if (strcmp(name->valuestring, ASSET_NAME) != 0) continue;

        cJSON *url = cJSON_GetObjectItemCaseSensitive(asset, "browser_download_url");
        if (cJSON_IsString(url) && url->valuestring) {
            snprintf(result.download_url, sizeof(result.download_url), "%s", url->valuestring);
        }
        cJSON *size = cJSON_GetObjectItemCaseSensitive(asset, "size");
        if (cJSON_IsNumber(size)) result.size = (unsigned long)size->valuedouble;
        break;
    }

    cJSON_Delete(root);

    /* tag_name is "vX.Y.Z", APP_VERSION is "X.Y.Z" */
    const char *latest = result.latest_tag;
    if (latest[0] == 'v' || latest[0] == 'V') latest++;

    result.ok = true;
    result.update_available = result.download_url[0] != '\0' && version_cmp(APP_VERSION, latest) < 0;
    return result;
}
