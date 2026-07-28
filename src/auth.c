#include "auth.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define COOKIE_FILE "archive_cookies.txt"
#define CURL_CONFIG ".curl_auth.cfg"
#define CA_BUNDLE   "cacert.pem"

static bool g_ok;

static bool file_readable(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return false;
    fclose(f);
    return true;
}

/* Cookie values are opaque tokens; anything outside this set can only be
 * paste damage or an injection attempt against the curl config we write, so
 * drop it rather than trying to quote around it. Keeps '=' and ';' because
 * those are the cookie syntax itself. */
static bool cookie_char_ok(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           c == '=' || c == ';' || c == ' ' || c == '-' || c == '_' || c == '.' ||
           c == '%' || c == '+' || c == '/' || c == ':' || c == '@' || c == '*' || c == '~';
}

void auth_init(void) {
    g_ok = false;

    FILE *f = fopen(COOKIE_FILE, "r");
    if (!f) return; /* no cookies configured — anonymous mode, the normal case */

    /* Accept either one "a=1; b=2" line or the two cookies on separate
     * lines, and ignore blank/# lines, because this file is hand-made. */
    char cookie[2048];
    size_t o = 0;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\r' || *p == '\0') continue;

        if (o > 0 && o + 2 < sizeof(cookie)) { /* joining separate lines */
            cookie[o++] = ';';
            cookie[o++] = ' ';
        }
        for (; *p && o + 1 < sizeof(cookie); p++) {
            if (cookie_char_ok(*p)) cookie[o++] = *p;
        }
    }
    fclose(f);
    while (o > 0 && (cookie[o - 1] == ' ' || cookie[o - 1] == ';')) o--; /* tidy trailing separators */
    cookie[o] = '\0';

    if (o == 0) return; /* file present but empty/comments only */

    /* Refuse to run without the CA bundle: the device ships no trust store,
     * so curl would have to fall back to an unverified connection, and
     * sending someone's session cookies over one is not a trade worth
     * making. Better to stay anonymous and say why. */
    if (!file_readable(CA_BUNDLE)) return;

    FILE *cfg = fopen(CURL_CONFIG, "w");
    if (!cfg) return;
    fprintf(cfg, "cookie = \"%s\"\n", cookie);
    fprintf(cfg, "cacert = \"%s\"\n", CA_BUNDLE);
    fclose(cfg);

    g_ok = true;
}

/* The stored session carries the account address percent-encoded
 * ("you%40example.com"); decode just enough to show it back to the user. */
void auth_account(char *out, size_t outsz) {
    if (outsz) out[0] = '\0';
    FILE *f = fopen(COOKIE_FILE, "r");
    if (!f) return;

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        const char *v = strstr(line, "logged-in-user=");
        if (!v) continue;
        v += strlen("logged-in-user=");
        size_t o = 0;
        for (; *v && *v != ';' && *v != '\n' && *v != '\r' && o + 1 < outsz; v++) {
            if (v[0] == '%' && v[1] && v[2]) {
                char hex[3] = { v[1], v[2], 0 };
                char *end = NULL;
                long c = strtol(hex, &end, 16);
                if (end && *end == '\0' && c > 0) {
                    out[o++] = (char)c;
                    v += 2;
                    continue;
                }
            }
            out[o++] = *v;
        }
        out[o] = '\0';
        break;
    }
    fclose(f);
}

bool auth_available(void) {
    return g_ok;
}

const char *auth_curl_config(void) {
    return g_ok ? CURL_CONFIG : NULL;
}
