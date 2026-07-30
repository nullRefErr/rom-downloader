#include "settings.h"
#include "util.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SETTINGS_FILE "settings.json"

static Settings g_settings = { "en", true };


/* Built with cJSON rather than printf for the same reason sources_save() is:
 * lang is only ever a code from the picker today, but a hand-edited
 * settings.json round-trips straight back through here, and one unescaped
 * quote produces a file that settings_load() then fails to parse — at which
 * point it keeps the defaults and BOTH preferences silently revert. */
static void save(void) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return;
    cJSON_AddStringToObject(root, "lang", g_settings.lang);
    cJSON_AddBoolToObject(root, "sound", g_settings.sound);

    char *text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!text) return;

    FILE *f = fopen(SETTINGS_FILE, "w");
    if (f) { fputs(text, f); fputc('\n', f); fclose(f); }
    free(text);
}

void settings_load(void) {
    char *json = util_read_file(SETTINGS_FILE, 64 * 1024);
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
