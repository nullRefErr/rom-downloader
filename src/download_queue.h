#ifndef DOWNLOAD_QUEUE_H
#define DOWNLOAD_QUEUE_H

#include <stdbool.h>

#define MAX_QUEUE_SIZE 16

typedef enum { QRESULT_NONE, QRESULT_DONE, QRESULT_FAILED } QueueResult;

typedef struct {
    bool active;              /* a download is currently running */
    char active_name[256];    /* display name of the active download */
    float progress;           /* 0..1 of the active download */
    int waiting;              /* items still queued behind the active one */
    QueueResult last_result;  /* result of the most recently finished item */
    char last_name[256];      /* display name of that finished item */
} QueueStatus;

/* The queue is the single owner of the download lifecycle: it (and only it)
 * calls download_poll(), so nothing else races it for the DONE/FAILED
 * transition. queue_tick() is driven from the main loop every frame,
 * independent of the current screen, so a download started on the rom list
 * still completes (rename + reset_list) even after the user backs out. */

/* Clear the queue and the underlying download state. Call once at startup,
 * NOT per screen — downloads intentionally survive navigation. */
void queue_reset(void);

/* Enqueue a download. Returns false if the queue is full, the rom is
 * already downloaded, or it's already active/queued. */
bool queue_add(const char *base_url, const char *item_path,
               const char *dest_dir, const char *dest_filename,
               unsigned long size);

/* Advance the queue: poll the active download, start the next one when
 * idle. Call every frame from the main loop. */
void queue_tick(void);

void queue_get_status(QueueStatus *out);

/* Acknowledge last_result after the UI has shown it (resets it to NONE). */
void queue_clear_last_result(void);

#endif
