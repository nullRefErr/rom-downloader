#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>

/* User settings, persisted to settings.json next to the app so they survive
 * a restart. Deliberately tiny and hand-editable; cJSON is already vendored
 * for the archive.org responses, so it costs nothing to reuse here. */

typedef struct {
    char lang[8]; /* "en", "de", "fr", "ja", "tr" */
    bool sound;   /* navigation click on/off */
} Settings;

/* Reads settings.json, falling back to defaults (English, sound on) when it
 * is missing or unreadable. Call once at startup, before i18n_load(). */
void settings_load(void);

const Settings *settings_get(void);

/* Both persist immediately — the app can be powered off at any moment, so
 * there is no "save on exit" to rely on. */
void settings_set_lang(const char *code);
void settings_set_sound(bool on);

#endif
