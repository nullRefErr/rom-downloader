#ifndef I18N_FALLBACK_H
#define I18N_FALLBACK_H

/* Compiled-in English for when lang.json is absent — see i18n_fallback.c.
 * Returns NULL for an unknown key. */
const char *i18n_fallback(const char *key);

#endif
