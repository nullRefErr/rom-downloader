#include "i18n.h"
#include "util.h"
#include "cJSON.h"
#include "i18n_fallback.h"
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


void i18n_load(const char *lang) {
    char *json = util_read_file(LANG_FILE, 0);
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
    /* No lang.json at all (e.g. updated in-app from a build that predates it):
     * fall back to the English compiled into the binary rather than showing
     * the user raw keys. */
    const char *builtin = i18n_fallback(key);
    if (builtin) return builtin;
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
