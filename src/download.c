#include "download.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>

static void dlog(const char *fmt, ...) {
    FILE *f = fopen("log.txt", "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

static DownloadState g_state = DOWNLOAD_IDLE;
static char g_part_path[560];
static char g_final_path[540];
static char g_done_marker[560];
static char g_failed_marker[560];
static char g_roms_dir[300];
static unsigned long g_expected_size;

static long file_size(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    return sz;
}

/* Same idea as thumbnail.c's url_encode, but keeps '/' literal — archive.org
 * item paths are real subdirectory paths within the item (e.g.
 * "CHD-PSX-EUR/007 - The World Is Not Enough (Europe).chd"), not a single
 * flat filename. */
static void url_encode_path(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 4 < outsz; i++) {
        unsigned char c = (unsigned char)in[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            out[o++] = (char)c;
        } else {
            o += (size_t)snprintf(out + o, outsz - o, "%%%02X", c);
        }
    }
    out[o] = '\0';
}

void download_start(const char *identifier, const char *item_path,
                     const char *dest_dir, const char *dest_filename,
                     unsigned long expected_size) {
    if (g_state == DOWNLOAD_RUNNING) return;

    g_expected_size = expected_size;
    snprintf(g_final_path, sizeof(g_final_path), "%s/%s", dest_dir, dest_filename);
    snprintf(g_part_path, sizeof(g_part_path), "%s.part", g_final_path);
    snprintf(g_done_marker, sizeof(g_done_marker), "%s.done", g_final_path);
    snprintf(g_failed_marker, sizeof(g_failed_marker), "%s.failed", g_final_path);
    snprintf(g_roms_dir, sizeof(g_roms_dir), "%s", dest_dir);

    mkdir(dest_dir, 0755); /* ignore EEXIST — Roms/<CODE> should already exist, belt and suspenders */
    remove(g_part_path);
    remove(g_done_marker);
    remove(g_failed_marker);

    char encoded_path[600], url[900], cmd[3072];
    url_encode_path(item_path, encoded_path, sizeof(encoded_path));
    snprintf(url, sizeof(url), "https://archive.org/download/%s/%s", identifier, encoded_path);
    /* backgrounded (&) so download_poll() can be called from the main
     * loop without blocking the UI — same reasoning as everywhere else
     * in this app, just async this time since a rom download can take
     * much longer than a metadata fetch or a thumbnail. */
    snprintf(cmd, sizeof(cmd),
             "(wget -q -O '%s' '%s' && touch '%s') || touch '%s' &",
             g_part_path, url, g_done_marker, g_failed_marker);
    system(cmd);

    g_state = DOWNLOAD_RUNNING;
}

DownloadState download_poll(float *out_progress) {
    if (g_state != DOWNLOAD_RUNNING) return g_state;

    FILE *done = fopen(g_done_marker, "r");
    if (done) {
        fclose(done);
        remove(g_done_marker);
        rename(g_part_path, g_final_path);

        dlog("download: complete, saved to %s", g_final_path);

        /* refresh Onion's game-list cache, exactly like mmp_getrom did —
         * easy detail to drop when porting, and without it the new rom
         * doesn't show up until a manual rescan. */
        char resetcmd[700];
        snprintf(resetcmd, sizeof(resetcmd),
                 "if [ -f /mnt/SDCARD/.tmp_update/script/reset_list.sh ]; then "
                 "/mnt/SDCARD/.tmp_update/script/reset_list.sh '%s' >/tmp/romdl_resetlist.log 2>&1; "
                 "echo rc=$? >> /tmp/romdl_resetlist.log; "
                 "else echo 'reset_list.sh not found' > /tmp/romdl_resetlist.log; fi",
                 g_roms_dir);
        int rc = system(resetcmd);
        dlog("download: reset_list.sh invoked for '%s', system() rc=%d (see /tmp/romdl_resetlist.log on-device)",
             g_roms_dir, rc);

        g_state = DOWNLOAD_DONE;
        return g_state;
    }

    FILE *failed = fopen(g_failed_marker, "r");
    if (failed) {
        fclose(failed);
        remove(g_failed_marker);
        remove(g_part_path);
        g_state = DOWNLOAD_FAILED;
        return g_state;
    }

    if (out_progress) {
        long sz = file_size(g_part_path);
        *out_progress = (g_expected_size > 0 && sz > 0)
                             ? (float)((double)sz / (double)g_expected_size)
                             : 0.0f;
        if (*out_progress > 0.99f) *out_progress = 0.99f; /* leave the last bit for the actual DONE transition */
    }
    return g_state;
}

void download_reset(void) {
    g_state = DOWNLOAD_IDLE;
}

bool download_file_exists(const char *dest_dir, const char *dest_filename) {
    char path[600];
    snprintf(path, sizeof(path), "%s/%s", dest_dir, dest_filename);
    return file_size(path) > 0;
}
