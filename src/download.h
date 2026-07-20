#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <stdbool.h>

typedef enum {
    DOWNLOAD_IDLE,
    DOWNLOAD_RUNNING,
    DOWNLOAD_DONE,
    DOWNLOAD_FAILED,
} DownloadState;

/* Starts a background download: wget is backgrounded via a shell `&` (not
 * threading — matches the wget-via-shell pattern already used everywhere
 * else in this app) of archive.org item `identifier`'s file `item_path`
 * (as listed by the metadata API — may include an internal folder prefix,
 * e.g. "CHD-PSX-EUR/007 - ....chd") into `dest_dir/dest_filename`. Writes
 * to `dest_filename.part` first; download_poll() only renames to the
 * final name on success, so an interrupted download (wifi drop) can't
 * leave a corrupt-but-complete-looking rom sitting under its real name.
 * No-op if a download is already running. */
void download_start(const char *identifier, const char *item_path,
                     const char *dest_dir, const char *dest_filename,
                     unsigned long expected_size);

/* Call every tick while a download might be in flight. On DOWNLOAD_DONE
 * this also renames .part -> final and invokes Onion's own
 * reset_list.sh (if present) on `dest_dir`, exactly like mmp_getrom did,
 * so the new rom shows up in the emulator's game list without a manual
 * rescan. `out_progress` (may be NULL) is set to [0.0, 1.0] while
 * RUNNING, based on the partial file's growing size vs `expected_size`. */
DownloadState download_poll(float *out_progress);

/* Resets state back to IDLE — call after the caller has shown/handled a
 * DONE or FAILED result, so the next ENTER press can start fresh. */
void download_reset(void);

/* Simple existence check for the "Downloaded" indicator — independent of
 * the download_start/poll state machine above. */
bool download_file_exists(const char *dest_dir, const char *dest_filename);

#endif
