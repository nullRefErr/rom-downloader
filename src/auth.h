#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>
#include <stddef.h>

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

/* Library path curl needs when spawned from inside the app.
 *
 * curl links against SigmaStar vendor libraries (libmi_common.so and
 * friends) that live in /config/lib, plus libcurl in .tmp_update/lib. A
 * shell gets these from the system default; the app does not, because
 * setting LD_LIBRARY_PATH on the command REPLACES the inherited value —
 * which is why curl failed with "error while loading shared libraries" from
 * inside the app while the identical command worked over SSH. Spelled out in
 * full here, and the inherited value is still appended so anything the
 * firmware adds is kept. */
#define CURL_LD_PATH "/lib:/config/lib:/mnt/SDCARD/miyoo/lib:" \
                     "/mnt/SDCARD/.tmp_update/lib:/mnt/SDCARD/.tmp_update/lib/parasyte:$LD_LIBRARY_PATH"
#define CURL_BIN_PATH "/mnt/SDCARD/.tmp_update/bin/curl"

/* Reads the cookie file and prepares the curl config. Call once at startup. */
/* Returns false when no usable session was established — no cookie file, an
 * empty one, no CA bundle, or the curl config could not be written. Callers
 * that just want cookies picked up if present can ignore it; the sign-in flow
 * must not, or it reports "Signed in" for a session that isn't there. */
bool auth_init(void);

/* True when a usable cookie file AND the CA bundle are both present. */
bool auth_available(void);

/* Best-effort account label for the settings screen: the address from the
 * stored session, percent-decoded. Empty string when not signed in. */
void auth_account(char *out, size_t outsz);

/* Path to the generated curl config; only valid while auth_available(). */
const char *auth_curl_config(void);

#endif
