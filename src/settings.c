#include "settings.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SETTINGS_FILE "settings.json"

static Settings g_settings = { "en", true };

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 64 * 1024) { /* a settings file this big is not ours */
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

static void save(void) {
    FILE *f = fopen(SETTINGS_FILE, "w");
    if (!f) return;
    fprintf(f, "{\n  \"lang\": \"%s\",\n  \"sound\": %s\n}\n",
            g_settings.lang, g_settings.sound ? "true" : "false");
    fclose(f);
}

void settings_load(void) {
    char *json = read_file(SETTINGS_FILE);
    if (!json) return; /* first run — defaults stand */

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root) return; /* corrupt: keep defaults rather than refusing to start */

    cJSON *lang = cJSON_GetObjectItemCaseSensitive(root, "lang");
    if (cJSON_IsString(lang) && lang->valuestring) {
        snprintf(g_settings.lang, sizeof(g_settings.lang), "%s", lang->valuestring);
    }
    cJSON *sound = cJSON_GetObjectItemCaseSensitive(root, "sound");
    if (cJSON_IsBool(sound)) g_settings.sound = cJSON_IsTrue(sound);

    cJSON_Delete(root);
}

const Settings *settings_get(void) {
    return &g_settings;
}

void settings_set_lang(const char *code) {
    if (!code || !*code) return;
    snprintf(g_settings.lang, sizeof(g_settings.lang), "%s", code);
    save();
}

void settings_set_sound(bool on) {
    g_settings.sound = on;
    save();
}
