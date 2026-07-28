#include "i18n.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LANG_FILE "lang.json"

/* Names are written in each language's own script, which is the convention
 * users expect in a language picker — you look for the one you can read. */
static const struct {
    const char *code;
    const char *name;
} LANGS[] = {
    { "en", "English"  },
    { "de", "Deutsch"  },
    { "fr", "Francais" },
    { "ja", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E" }, /* 日本語 */
    { "tr", "T\xC3\xBCrk\xC3\xA7\x65" },              /* Türkçe */
};
#define LANG_COUNT ((int)(sizeof(LANGS) / sizeof(LANGS[0])))

static cJSON *g_root;     /* whole file, kept alive: the returned strings point into it */
static cJSON *g_active;   /* selected language object */
static cJSON *g_fallback; /* English */

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
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

void i18n_load(const char *lang) {
    char *json = read_file(LANG_FILE);
    if (!json) return; /* no file: T() falls through to returning keys */
    g_root = cJSON_Parse(json);
    free(json);
    if (!g_root) return;

    g_fallback = cJSON_GetObjectItemCaseSensitive(g_root, "en");
    i18n_set_lang(lang);
}

void i18n_set_lang(const char *lang) {
    if (!g_root) return;
    g_active = lang ? cJSON_GetObjectItemCaseSensitive(g_root, lang) : NULL;
    if (!g_active) g_active = g_fallback;
}

const char *T(const char *key) {
    if (g_active) {
        cJSON *v = cJSON_GetObjectItemCaseSensitive(g_active, key);
        if (cJSON_IsString(v) && v->valuestring) return v->valuestring;
    }
    if (g_fallback) {
        cJSON *v = cJSON_GetObjectItemCaseSensitive(g_fallback, key);
        if (cJSON_IsString(v) && v->valuestring) return v->valuestring;
    }
    return key;
}

int i18n_lang_count(void) { return LANG_COUNT; }
const char *i18n_lang_code(int i) { return (i >= 0 && i < LANG_COUNT) ? LANGS[i].code : "en"; }
const char *i18n_lang_name(int i) { return (i >= 0 && i < LANG_COUNT) ? LANGS[i].name : "English"; }

int i18n_lang_index(const char *code) {
    if (!code) return -1;
    for (int i = 0; i < LANG_COUNT; i++) {
        if (strcmp(LANGS[i].code, code) == 0) return i;
    }
    return -1;
}
