#include "net_archive.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *fetch_url(const char *url) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "wget -q -O - '%s' 2>/dev/null", url);
    FILE *p = popen(cmd, "r");
    if (!p) return NULL;

    size_t cap = 65536, len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        pclose(p);
        return NULL;
    }
    size_t n;
    while ((n = fread(buf + len, 1, cap - len, p)) > 0) {
        len += n;
        if (len == cap) {
            cap *= 2;
            char *nbuf = realloc(buf, cap);
            if (!nbuf) {
                free(buf);
                pclose(p);
                return NULL;
            }
            buf = nbuf;
        }
    }
    int status = pclose(p);
    if (status != 0 || len == 0) {
        free(buf);
        return NULL;
    }
    buf[len] = '\0';
    return buf;
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

RomList net_archive_fetch(const char *identifier, const char *ext) {
    RomList result = {0};

    char url[256];
    snprintf(url, sizeof(url), "https://archive.org/metadata/%s", identifier);

    char *json = fetch_url(url);
    if (!json) return result; /* ok=0: network/wget failure */

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root) return result; /* ok=0: unparseable response */

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
