#include "env.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define ENV_FILE ".env"

static void strip_eol(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) s[--n] = '\0';
}

/* Matches KEY=value, tolerating leading spaces and optional quotes around
 * the value, because this file is hand-edited. Returns the value or NULL. */
static const char *match_key(char *line, const char *key) {
    char *p = line;
    p += strspn(p, " \t");
    size_t klen = strlen(key);
    if (strncmp(p, key, klen) != 0) return NULL;
    p += klen;
    p += strspn(p, " \t");
    if (*p != '=') return NULL;
    p++;
    p += strspn(p, " \t");
    if (*p == '"' || *p == '\'') {
        char q = *p++;
        char *end = strrchr(p, q);
        if (end) *end = '\0';
    }
    return p;
}

bool env_exists(void) {
    FILE *f = fopen(ENV_FILE, "r");
    if (!f) return false;
    fclose(f);
    return true;
}

bool env_load(char *email, size_t email_sz, char *password, size_t password_sz) {
    FILE *f = fopen(ENV_FILE, "r");
    if (!f) return false;

    bool got_email = false, got_pass = false;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        p += strspn(p, " \t");
        if (*p == '#') continue; /* comment */
        strip_eol(line);

        const char *v = match_key(line, "ARCHIVE_EMAIL");
        if (v) {
            snprintf(email, email_sz, "%s", v);
            got_email = true;
            continue;
        }
        v = match_key(line, "ARCHIVE_PASSWORD");
        if (v) {
            snprintf(password, password_sz, "%s", v);
            got_pass = true;
        }
    }
    fclose(f);
    return got_email && got_pass && email[0] && password[0];
}

bool env_save(const char *email, const char *password) {
    FILE *f = fopen(ENV_FILE, "w");
    if (!f) return false;
    fprintf(f, "# archive.org credentials for rom-downloader.\n");
    fprintf(f, "# WARNING: stored in plain text. This card has no file permissions,\n");
    fprintf(f, "# so anyone who takes it can read this. Delete this file to stop\n");
    fprintf(f, "# storing your password; the app still works, it will just ask again\n");
    fprintf(f, "# when the saved session expires.\n");
    fprintf(f, "ARCHIVE_EMAIL=%s\n", email ? email : "");
    fprintf(f, "ARCHIVE_PASSWORD=%s\n", password ? password : "");
    fclose(f);
    /* Best effort: meaningless on FAT32 (which is what the card is), but
     * correct if the app ever runs from a real filesystem. */
    chmod(ENV_FILE, 0600);
    return true;
}
