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

/* Quote a string for safe embedding in a shell command (POSIX single-quote
 * escaping: close the quote, insert an escaped literal quote, reopen).
 * Game titles routinely contain apostrophes ("Awesome Possum Kicks Dr.
 * Machino's Butt", "Adiboo & Paziral's Secret" — 150+ in the PS1 catalog
 * alone, 300+ in Genesis) which otherwise break out of the single-quoted
 * paths in download_start()'s wget command, same bug class as the one
 * already fixed in thumbnail.c's sanitize_filename(), just unfixed here. */
static void shell_quote(const char *in, char *out, size_t outsz) {
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

static bool g_refresh_game_list;

static void begin_download(const char *url, const char *dest_dir,
                            const char *dest_filename, unsigned long expected_size) {
    if (g_state == DOWNLOAD_RUNNING) return;

    g_expected_size = expected_size;
    snprintf(g_final_path, sizeof(g_final_path), "%s/%s", dest_dir, dest_filename);
    snprintf(g_part_path, sizeof(g_part_path), "%s.part", g_final_path);
    snprintf(g_done_marker, sizeof(g_done_marker), "%s.done", g_final_path);
    snprintf(g_failed_marker, sizeof(g_failed_marker), "%s.failed", g_final_path);
    snprintf(g_roms_dir, sizeof(g_roms_dir), "%s", dest_dir);

    mkdir(dest_dir, 0755); /* ignore EEXIST — Roms/<CODE> should already exist, belt and suspenders */
    /* Deliberately NOT removing g_part_path in general: a leftover .part from
     * an interrupted attempt (wifi drop, app killed) is exactly what `wget -c`
     * resumes from, so keeping it is the whole point of the resume feature.
     * Markers are still cleared — they're per-attempt signals.
     *
     * The exception: a .part that is ALREADY at/past the expected size can
     * only make `wget -c` ask for a range starting at or beyond EOF, which
     * archive.org answers with 416, and BusyBox wget treats any 416 as a hard
     * error. That wedges the rom into a permanent "Download failed" loop with
     * no way out from the UI, since the "already downloaded" check only ever
     * stats the FINAL name and nothing else deletes a .part. Reachable
     * whenever the app is quit mid-transfer: the wget was backgrounded, so it
     * keeps running to completion and leaves a full-size .part behind. */
    if (expected_size > 0) {
        long part = file_size(g_part_path);
        if (part > 0 && (unsigned long)part >= expected_size) remove(g_part_path);
    }
    remove(g_done_marker);
    remove(g_failed_marker);

    char cmd[3072];
    char part_q[600], done_q[600], failed_q[600];
    shell_quote(g_part_path, part_q, sizeof(part_q));
    shell_quote(g_done_marker, done_q, sizeof(done_q));
    shell_quote(g_failed_marker, failed_q, sizeof(failed_q));

    dlog("download: starting %s -> %s", url, g_final_path);

    /* This device ships BusyBox wget, which supports -c (confirmed: it
     * honours HTTP range requests for resume) but NOT -nv (rejects it as
     * "invalid option", which broke every download), and its -q suppresses
     * error messages entirely. So use plain wget: no -q means BusyBox still
     * prints the failure reason ("HTTP error 404", "No space left") to
     * stderr, and — because stderr is redirected to a file here, not a tty —
     * it emits only a few status lines, no animated progress bar. The whole
     * group's output is appended to log.txt so a failed download records
     * WHY. backgrounded (&) so download_poll() can run from the main loop
     * without blocking the UI. */
    snprintf(cmd, sizeof(cmd),
             "( (wget -c -O %s '%s' && touch %s) || touch %s ) >>log.txt 2>&1 &",
             part_q, url, done_q, failed_q);
    system(cmd);

    g_state = DOWNLOAD_RUNNING;
}

void download_start(const char *identifier, const char *item_path,
                     const char *dest_dir, const char *dest_filename,
                     unsigned long expected_size) {
    if (g_state == DOWNLOAD_RUNNING) return;

    char encoded_path[600], url[900];
    url_encode_path(item_path, encoded_path, sizeof(encoded_path));
    snprintf(url, sizeof(url), "https://archive.org/download/%s/%s", identifier, encoded_path);

    g_refresh_game_list = true;
    begin_download(url, dest_dir, dest_filename, expected_size);
}

void download_start_url(const char *url, const char *dest_dir,
                         const char *dest_filename, unsigned long expected_size) {
    if (g_state == DOWNLOAD_RUNNING) return;
    g_refresh_game_list = false;
    begin_download(url, dest_dir, dest_filename, expected_size);
}

DownloadState download_poll(float *out_progress) {
    if (g_state != DOWNLOAD_RUNNING) return g_state;

    FILE *done = fopen(g_done_marker, "r");
    if (done) {
        fclose(done);
        remove(g_done_marker);

        /* Only accept the transfer if it's actually the size archive.org
         * advertised. Resuming means the .part may carry bytes from an
         * earlier, different version of the file (archive.org re-derives
         * CHDs), in which case wget still stops at content-length and exits
         * 0 — renaming that to the final name would report "Downloaded" for
         * a corrupt rom. Drop the bad .part so a retry starts clean.
         * Skipped when the size is unknown (expected_size == 0). */
        long got = file_size(g_part_path);
        if (g_expected_size > 0 && (got < 0 || (unsigned long)got != g_expected_size)) {
            dlog("download: size mismatch for %s (got %ld, expected %lu) — discarding",
                 g_final_path, got, g_expected_size);
            remove(g_part_path);
            g_state = DOWNLOAD_FAILED;
            return g_state;
        }

        rename(g_part_path, g_final_path);

        dlog("download: complete, saved to %s", g_final_path);

        /* refresh Onion's game-list cache — without it the new rom doesn't
         * show up until a manual rescan. Only for rom downloads (meaningless
         * for the app's own binary update, see g_refresh_game_list).
         *
         * BACKGROUNDED (&): reset_list.sh rescans the whole destination
         * Roms/<code> folder, and on a device with a large existing library
         * for that system that rescan takes long enough that running it
         * synchronously here froze the UI right after a successful download
         * (reported via Reddit: "app froze after a successful GB download" —
         * a user with a full Game Boy library). This system() is called from
         * download_poll() on the render loop, so it must not block; the cache
         * just refreshes a moment later, async. */
        if (g_refresh_game_list) {
            char resetcmd[700];
            snprintf(resetcmd, sizeof(resetcmd),
                     "( [ -f /mnt/SDCARD/.tmp_update/script/reset_list.sh ] && "
                     "/mnt/SDCARD/.tmp_update/script/reset_list.sh '%s' ) "
                     ">/tmp/romdl_resetlist.log 2>&1 &",
                     g_roms_dir);
            system(resetcmd);
            dlog("download: reset_list.sh launched in background for '%s'", g_roms_dir);
        }

        g_state = DOWNLOAD_DONE;
        return g_state;
    }

    FILE *failed = fopen(g_failed_marker, "r");
    if (failed) {
        fclose(failed);
        remove(g_failed_marker);
        /* Keep g_part_path on failure so the next attempt at this same rom
         * resumes via `wget -c` instead of starting over. A partial file
         * for an abandoned rom just lingers (harmless — the "Downloaded"
         * check looks at the final name, never the .part), and gets
         * overwritten/continued if the user retries it. */
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
