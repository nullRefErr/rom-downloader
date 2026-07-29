#include "download_queue.h"
#include "download.h"
#include <string.h>
#include <stdio.h>

/* Buffers sized to comfortably hold the longest real archive.org values
 * (item_path carries an internal subdir prefix + a long title; download.c
 * itself works in 512-600 byte path buffers). Undersizing these silently
 * truncated the saved filename so the "already downloaded" check — which
 * uses the full untruncated name — never matched. */
typedef struct {
    char base_url[300];
    char item_path[512];
    char dest_dir[300];
    char dest_filename[256];
    unsigned long size;
} QueueItem;

static QueueItem g_items[MAX_QUEUE_SIZE];
static int g_head, g_tail, g_count; /* ring buffer: g_count avoids the head==tail full/empty ambiguity */

static bool g_active;
static QueueItem g_current;
static float g_progress;
static QueueResult g_last_result;
static char g_last_name[256]; /* matches QueueItem.dest_filename */

void queue_reset(void) {
    g_head = g_tail = g_count = 0;
    g_active = false;
    g_progress = 0.0f;
    g_last_result = QRESULT_NONE;
    g_last_name[0] = '\0';
    download_reset();
}

static bool same_target(const QueueItem *it, const char *dir, const char *file) {
    return strcmp(it->dest_dir, dir) == 0 && strcmp(it->dest_filename, file) == 0;
}

bool queue_add(const char *base_url, const char *item_path,
               const char *dest_dir, const char *dest_filename,
               unsigned long size) {
    if (g_count >= MAX_QUEUE_SIZE) return false;
    if (download_file_exists(dest_dir, dest_filename)) return false; /* already have it */
    if (g_active && same_target(&g_current, dest_dir, dest_filename)) return false;
    for (int i = 0; i < g_count; i++) {
        int idx = (g_head + i) % MAX_QUEUE_SIZE;
        if (same_target(&g_items[idx], dest_dir, dest_filename)) return false; /* already queued */
    }

    QueueItem *it = &g_items[g_tail];
    snprintf(it->base_url, sizeof(it->base_url), "%s", base_url);
    snprintf(it->item_path, sizeof(it->item_path), "%s", item_path);
    snprintf(it->dest_dir, sizeof(it->dest_dir), "%s", dest_dir);
    snprintf(it->dest_filename, sizeof(it->dest_filename), "%s", dest_filename);
    it->size = size;
    g_tail = (g_tail + 1) % MAX_QUEUE_SIZE;
    g_count++;
    return true;
}

void queue_tick(void) {
    if (g_active) {
        float p = 0.0f;
        DownloadState st = download_poll(&p);
        if (st == DOWNLOAD_RUNNING) {
            g_progress = p;
            return;
        }
        /* DONE or FAILED: record result, free the slot. A FAILED result is
         * the ONLY signal the UI has for a failure (there's no persistent
         * failed indicator, unlike the green "Downloaded" mark for DONE), so
         * don't let a later DONE overwrite an unconsumed FAILED — otherwise
         * a failure that happened while the user was off the rom-list screen
         * is silently lost when a subsequent queued item succeeds. */
        QueueResult r = (st == DOWNLOAD_DONE) ? QRESULT_DONE : QRESULT_FAILED;
        if (!(r == QRESULT_DONE && g_last_result == QRESULT_FAILED)) {
            g_last_result = r;
            snprintf(g_last_name, sizeof(g_last_name), "%s", g_current.dest_filename);
        }
        g_active = false;
        g_progress = 0.0f;
        download_reset();
    }

    if (!g_active && g_count > 0) {
        g_current = g_items[g_head];
        g_head = (g_head + 1) % MAX_QUEUE_SIZE;
        g_count--;
        download_start(g_current.base_url, g_current.item_path,
                       g_current.dest_dir, g_current.dest_filename, g_current.size);
        g_active = true;
        g_progress = 0.0f;
    }
}

void queue_get_status(QueueStatus *out) {
    out->active = g_active;
    snprintf(out->active_name, sizeof(out->active_name), "%s", g_active ? g_current.dest_filename : "");
    out->progress = g_progress;
    out->waiting = g_count;
    out->last_result = g_last_result;
    snprintf(out->last_name, sizeof(out->last_name), "%s", g_last_name);
}

void queue_clear_last_result(void) {
    g_last_result = QRESULT_NONE;
}
