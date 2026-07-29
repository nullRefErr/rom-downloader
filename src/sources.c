#include "sources.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOURCES_FILE "sources.json"
#define MAX_SOURCES 32

static EmuEntry g_sources[MAX_SOURCES];
static int g_count;

static void copy_field(char *dst, size_t dstsz, cJSON *obj, const char *key, const char *fallback) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(v) && v->valuestring) snprintf(dst, dstsz, "%s", v->valuestring);
    else snprintf(dst, dstsz, "%s", fallback ? fallback : "");
}

static void load_defaults(void) {
    g_count = EMU_DEFAULTS_COUNT < MAX_SOURCES ? EMU_DEFAULTS_COUNT : MAX_SOURCES;
    for (int i = 0; i < g_count; i++) g_sources[i] = EMU_DEFAULTS[i];
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 256 * 1024) { /* a source list this big isn't ours */
        fclose(f);
        return NULL;
    }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

void sources_load(void) {
    load_defaults();

    char *json = read_file(SOURCES_FILE);
    if (!json) return; /* no file — defaults stand, which is the normal case */

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root) return; /* corrupt: keep defaults rather than showing nothing */

    cJSON *arr = cJSON_GetObjectItemCaseSensitive(root, "sources");
    if (!cJSON_IsArray(arr)) {
        cJSON_Delete(root);
        return;
    }

    int n = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, arr) {
        if (n >= MAX_SOURCES) break;
        if (!cJSON_IsObject(item)) continue;

        EmuEntry *e = &g_sources[n];
        memset(e, 0, sizeof(*e));
        copy_field(e->code, sizeof(e->code), item, "code", "");
        copy_field(e->label, sizeof(e->label), item, "label", e->code);
        copy_field(e->archive_id, sizeof(e->archive_id), item, "archive_id", "");
        copy_field(e->url, sizeof(e->url), item, "url", "");
        copy_field(e->base, sizeof(e->base), item, "base", "");
        copy_field(e->ext, sizeof(e->ext), item, "ext", "");
        copy_field(e->thumb_repo, sizeof(e->thumb_repo), item, "thumb_repo", "");

        cJSON *av = cJSON_GetObjectItemCaseSensitive(item, "available");
        e->available = cJSON_IsBool(av) ? cJSON_IsTrue(av) : 1;

        /* An entry with no code or nothing to fetch would be a dead row in
         * the list, so drop it rather than render something unusable. */
        if (e->code[0] && (e->archive_id[0] || e->url[0])) n++;
    }

    /* An empty or entirely invalid file shouldn't leave the user with no
     * systems at all and no obvious way back. */
    if (n > 0) g_count = n;

    cJSON_Delete(root);
}

void sources_save(void) {
    FILE *f = fopen(SOURCES_FILE, "w");
    if (!f) return;
    fprintf(f, "{\n  \"sources\": [\n");
    for (int i = 0; i < g_count; i++) {
        const EmuEntry *e = &g_sources[i];
        fprintf(f,
                "    { \"code\": \"%s\", \"label\": \"%s\", \"archive_id\": \"%s\", "
                "\"url\": \"%s\", \"base\": \"%s\", \"ext\": \"%s\", "
                "\"thumb_repo\": \"%s\", \"available\": %s }%s\n",
                e->code, e->label, e->archive_id, e->url, e->base, e->ext,
                e->thumb_repo, e->available ? "true" : "false",
                i + 1 < g_count ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
}

int sources_count(void) { return g_count; }

const EmuEntry *sources_get(int i) {
    return (i >= 0 && i < g_count) ? &g_sources[i] : NULL;
}

const char *sources_text(int i) {
    const EmuEntry *e = sources_get(i);
    if (!e) return "";
    return e->url[0] ? e->url : e->archive_id;
}

void sources_set(int i, const char *text) {
    if (i < 0 || i >= g_count || !text) return;
    EmuEntry *e = &g_sources[i];

    /* "://" is the only thing that reliably separates a URL from an
     * archive.org identifier — identifiers are bare names like "nointro.md",
     * which otherwise look a lot like a hostname. */
    if (strstr(text, "://")) {
        snprintf(e->url, sizeof(e->url), "%s", text);
        e->archive_id[0] = '\0';
    } else {
        snprintf(e->archive_id, sizeof(e->archive_id), "%s", text);
        e->url[0] = '\0';
        e->base[0] = '\0';
    }
    sources_save();
}

void sources_set_available(int i, int available) {
    if (i < 0 || i >= g_count) return;
    g_sources[i].available = available ? 1 : 0;
    sources_save();
}

void sources_reset(void) {
    remove(SOURCES_FILE);
    load_defaults();
}

void sources_list_url(const EmuEntry *e, char *out, size_t outsz) {
    if (!e) {
        if (outsz) out[0] = '\0';
        return;
    }
    if (e->url[0]) snprintf(out, outsz, "%s", e->url);
    else snprintf(out, outsz, "https://archive.org/metadata/%s", e->archive_id);
}

void sources_download_base(const EmuEntry *e, char *out, size_t outsz) {
    if (!e) {
        if (outsz) out[0] = '\0';
        return;
    }
    if (!e->url[0]) {
        snprintf(out, outsz, "https://archive.org/download/%s", e->archive_id);
        return;
    }
    if (e->base[0]) {
        snprintf(out, outsz, "%s", e->base);
        return;
    }
    /* No explicit base: assume the files sit next to the manifest, which is
     * what someone dropping an index.json into a folder of roms would expect. */
    snprintf(out, outsz, "%s", e->url);
    char *slash = strrchr(out, '/');
    if (slash && slash > out + 8) *slash = '\0'; /* keep the "https://" intact */
}
