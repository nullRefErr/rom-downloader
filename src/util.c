#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *util_read_file(const char *path, long max_bytes) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || (max_bytes > 0 && sz > max_bytes)) {
        fclose(f);
        return NULL;
    }

    char *buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

void util_format_size(unsigned long long bytes, char *out, size_t outsz) {
    if (bytes >= 1024ULL * 1024 * 1024) snprintf(out, outsz, "%.1f GB", bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= 1024ULL * 1024)   snprintf(out, outsz, "%.1f MB", bytes / (1024.0 * 1024));
    else if (bytes >= 1024)             snprintf(out, outsz, "%.1f KB", bytes / 1024.0);
    else                                snprintf(out, outsz, "%llu B", bytes);
}

void util_shell_quote(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    if (o + 1 < outsz) out[o++] = '\'';
    for (size_t i = 0; in[i] && o + 5 < outsz; i++) {
        if (in[i] == '\'') {
            memcpy(out + o, "'\\''", 4);
            o += 4;
        } else {
            out[o++] = in[i];
        }
    }
    if (o + 1 < outsz) out[o++] = '\'';
    out[o] = '\0';
}

long util_file_size(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    return sz;
}

void util_url_encode(const char *in, char *out, size_t outsz, const char *keep) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 4 < outsz; i++) {
        unsigned char c = (unsigned char)in[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~' ||
            (keep && *keep && strchr(keep, (char)c))) {
            out[o++] = (char)c;
        } else {
            o += (size_t)snprintf(out + o, outsz - o, "%%%02X", c);
        }
    }
    out[o] = '\0';
}

char *util_run_capture(const char *cmd, int *out_exit) {
    FILE *p = popen(cmd, "r");
    if (!p) return NULL;

    size_t cap = 65536, len = 0;
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
            char *nbuf = realloc(buf, cap);
            if (!nbuf) {
                free(buf);
                pclose(p);
                return NULL;
            }
            buf = nbuf;
        }
    }

    int status = pclose(p);
    buf[len] = '\0';

    if (out_exit) {
        *out_exit = (status == -1) ? -1 : (status / 256);
        return buf;
    }
    if (status != 0 || len == 0) {
        free(buf);
        return NULL;
    }
    return buf;
}
