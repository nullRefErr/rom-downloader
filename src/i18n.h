#ifndef I18N_H
#define I18N_H

/* UI strings, loaded from lang.json next to the app.
 *
 * The file holds one object per language keyed by code ("en", "de", "fr",
 * "ja", "tr"), each mapping the same string keys to translated text. Keeping
 * it as data rather than compiled-in tables means a translation can be fixed
 * or a language added by editing one file on the card, with no rebuild. */

/* Loads lang.json and selects `lang`. English is always parsed as well and
 * used as the fallback for any key a translation is missing, so a partial
 * translation degrades to English per-string instead of showing a raw key.
 * Call once at startup after settings_load(). */
void i18n_load(const char *lang);

/* Switches language without re-reading the file. Screens pick up the change
 * the next time they are built. */
void i18n_set_lang(const char *lang);

/* Translated text for `key`: the active language, else English, else the key
 * itself, so a missing entry is visible rather than an empty label. Never
 * returns NULL. */
const char *T(const char *key);

/* Language codes and their names as written in their own language, for the
 * settings screen. */
int i18n_lang_count(void);
const char *i18n_lang_code(int i);
const char *i18n_lang_name(int i);
int i18n_lang_index(const char *code); /* -1 if unknown */

#endif
