#ifndef ENV_H
#define ENV_H

#include <stdbool.h>
#include <stddef.h>

/* Optional credential file (.env next to the app) holding an archive.org
 * login, so the session can be re-established without retyping it when the
 * stored cookies expire:
 *
 *   ARCHIVE_EMAIL=you@example.com
 *   ARCHIVE_PASSWORD=your-password
 *
 * This stores the password in plain text on the SD card. FAT32 carries no
 * file permissions, so anyone holding the card can read it, and unlike a
 * session cookie a password cannot be revoked by logging out. It exists
 * because it was asked for explicitly; the cookie-only path remains the
 * safer default and is what the app uses when this file is absent. */

/* True when both values were found. Buffers are left untouched otherwise. */
bool env_load(char *email, size_t email_sz, char *password, size_t password_sz);

/* Writes/overwrites the file with these credentials. */
bool env_save(const char *email, const char *password);

/* True if the file exists at all. */
bool env_exists(void);

#endif
