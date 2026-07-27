#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>

/* Optional archive.org sign-in.
 *
 * archive.org has moved some items (the PlayStation source among them) into
 * a logged-in-only tier: the metadata stays public, so the rom list still
 * populates, but every file URL answers 403/401 to an anonymous request.
 *
 * Rather than handle a password, the app reads session cookies the user
 * exported from their own browser into `archive_cookies.txt` next to the
 * app. Those cookies are then handed to curl through a config file (never
 * on a command line, where any other process could read them out of `ps`)
 * over a TLS connection verified against the bundled CA store — the device
 * itself ships no CA store at all, so without that bundle curl could only
 * run with verification disabled, which is not acceptable for a request
 * carrying someone's session. */

/* Reads the cookie file and prepares the curl config. Call once at startup. */
void auth_init(void);

/* True when a usable cookie file AND the CA bundle are both present. */
bool auth_available(void);

/* Path to the generated curl config; only valid while auth_available(). */
const char *auth_curl_config(void);

#endif
