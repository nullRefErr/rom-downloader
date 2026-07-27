#ifndef NET_LOGIN_H
#define NET_LOGIN_H

#include <stdbool.h>

typedef struct {
    bool ok;            /* signed in, cookies written, auth_init() re-run */
    char message[96];   /* human-readable result to show on screen */
} LoginResult;

/* Signs in to archive.org with the user's OWN account and keeps only the
 * session cookies it returns.
 *
 * Each user enters their own email and password on the device; nothing is
 * shared or shipped with the app. The password is sent once, over a TLS
 * connection verified against the bundled CA store, and is never written to
 * disk — only the returned cookies are, and those the user can revoke at any
 * time by logging out of archive.org in a browser.
 *
 * Uses archive.org's XAuthN endpoint, which accepts a plain email/password
 * pair (verified against the live service). Note it is not a formally
 * supported third-party API, so it can change; failures are reported rather
 * than being allowed to look like a network glitch. Accounts protected by
 * two-factor authentication are not expected to work through it. */
LoginResult net_login(const char *email, const char *password);

/* Forgets the stored cookies (deletes them from the card). */
void net_login_sign_out(void);

#endif
