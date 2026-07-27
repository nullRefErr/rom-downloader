#include "net_login.h"
#include "auth.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COOKIE_FILE "archive_cookies.txt"
#define CA_BUNDLE   "cacert.pem"
#define XAUTHN_URL  "https://archive.org/services/xauthn/?op=login"

/* The credentials must not appear in the command line — anything there is
 * readable by every other process via ps. curl's --data @file reads the POST
 * body from a file instead, so the password only ever exists in a file we
 * create, write, use and delete within this call. */
#define POST_BODY_FILE ".login_post.tmp"

/* Percent-encode for application/x-www-form-urlencoded. Emails contain '@'
 * and '+', passwords can contain anything at all, so this has to be strict:
 * everything outside the unreserved set gets encoded. */
static void form_encode(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 4 < outsz; i++) {
        unsigned char c = (unsigned char)in[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out[o++] = (char)c;
        } else {
            o += (size_t)snprintf(out + o, outsz - o, "%%%02X", c);
        }
    }
    out[o] = '\0';
}

static char *run_capture(const char *cmd) {
    FILE *p = popen(cmd, "r");
    if (!p) return NULL;
    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        pclose(p);
        return NULL;
    }
    size_t n;
    while ((n = fread(buf + len, 1, cap - len - 1, p)) > 0) {
        len += n;
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                pclose(p);
                return NULL;
            }
            buf = nb;
        }
    }
    pclose(p);
    buf[len] = '\0';
    return buf;
}

/* XAuthN hands back a full Set-Cookie string, not a bare value, e.g.
 *   "you%40example.com; expires=Tue, 27-Jul-2027 ...; path=/; domain=.archive.org"
 * Storing that whole thing as the cookie value would produce a malformed
 * cookie header, so keep only the part before the first ';'. */
static void cookie_value_only(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    while (in[o] && in[o] != ';' && o + 1 < outsz) {
        out[o] = in[o];
        o++;
    }
    out[o] = '\0';
}

static bool write_cookies(const char *user, const char *sig) {
    char u[512], s[512];
    cookie_value_only(user, u, sizeof(u));
    cookie_value_only(sig, s, sizeof(s));

    FILE *f = fopen(COOKIE_FILE, "w");
    if (!f) return false;
    fprintf(f, "logged-in-user=%s\n", u);
    fprintf(f, "logged-in-sig=%s\n", s);
    fclose(f);
    return true;
}

LoginResult net_login(const char *email, const char *password) {
    LoginResult r = {0};

    if (!email || !*email || !password || !*password) {
        snprintf(r.message, sizeof(r.message), "Enter email and password");
        return r;
    }
    /* Without the CA bundle the request could only go out with certificate
     * verification disabled, and a password is exactly the thing never to
     * send over an unverified connection. */
    FILE *ca = fopen(CA_BUNDLE, "r");
    if (!ca) {
        snprintf(r.message, sizeof(r.message), "Missing cacert.pem - cannot sign in safely");
        return r;
    }
    fclose(ca);

    char email_enc[512], pass_enc[1024];
    form_encode(email, email_enc, sizeof(email_enc));
    form_encode(password, pass_enc, sizeof(pass_enc));

    FILE *body = fopen(POST_BODY_FILE, "w");
    if (!body) {
        snprintf(r.message, sizeof(r.message), "Could not prepare sign-in request");
        return r;
    }
    fprintf(body, "email=%s&password=%s", email_enc, pass_enc);
    fclose(body);

    char cmd[768]; /* the full library path is long */
    snprintf(cmd, sizeof(cmd),
             "LD_LIBRARY_PATH=%s %s -s --max-time 30 --cacert %s "
             "--data @%s '%s' 2>>log.txt",
             CURL_LD_PATH, CURL_BIN_PATH, CA_BUNDLE, POST_BODY_FILE, XAUTHN_URL);
    char *json = run_capture(cmd);
    remove(POST_BODY_FILE); /* the password never outlives the request */

    {   /* record what actually came back, without ever logging the body */
        FILE *lf = fopen("log.txt", "a");
        if (lf) {
            fprintf(lf, "login: curl returned %d bytes\n", json ? (int)strlen(json) : -1);
            fclose(lf);
        }
    }

    if (!json || !*json) {
        free(json);
        snprintf(r.message, sizeof(r.message), "No response - check Wi-Fi");
        return r;
    }

    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root) {
        snprintf(r.message, sizeof(r.message), "Unexpected reply from archive.org");
        return r;
    }

    cJSON *success = cJSON_GetObjectItemCaseSensitive(root, "success");
    cJSON *values = cJSON_GetObjectItemCaseSensitive(root, "values");

    if (!cJSON_IsTrue(success)) {
        /* Surface archive.org's own reason ("account_not_found",
         * "invalid_password", ...) rather than a generic failure, so a typo
         * is distinguishable from a service problem. */
        cJSON *reason = values ? cJSON_GetObjectItemCaseSensitive(values, "reason") : NULL;
        if (cJSON_IsString(reason) && reason->valuestring) {
            snprintf(r.message, sizeof(r.message), "Sign-in failed: %s", reason->valuestring);
        } else {
            snprintf(r.message, sizeof(r.message), "Sign-in failed");
        }
        cJSON_Delete(root);
        return r;
    }

    cJSON *cookies = values ? cJSON_GetObjectItemCaseSensitive(values, "cookies") : NULL;
    cJSON *user = cookies ? cJSON_GetObjectItemCaseSensitive(cookies, "logged-in-user") : NULL;
    cJSON *sig = cookies ? cJSON_GetObjectItemCaseSensitive(cookies, "logged-in-sig") : NULL;

    if (!cJSON_IsString(user) || !cJSON_IsString(sig) || !user->valuestring || !sig->valuestring) {
        snprintf(r.message, sizeof(r.message), "Signed in, but no session returned");
        cJSON_Delete(root);
        return r;
    }

    bool wrote = write_cookies(user->valuestring, sig->valuestring);
    cJSON_Delete(root);

    if (!wrote) {
        snprintf(r.message, sizeof(r.message), "Could not save session to card");
        return r;
    }

    auth_init(); /* pick the new cookies up immediately, no restart needed */
    r.ok = true;
    snprintf(r.message, sizeof(r.message), "Signed in");
    return r;
}

void net_login_sign_out(void) {
    remove(COOKIE_FILE);
    remove(".curl_auth.cfg");
    auth_init(); /* drops back to anonymous */
}
