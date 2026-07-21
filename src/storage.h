#ifndef STORAGE_H
#define STORAGE_H

typedef struct {
    unsigned long long total_bytes;
    unsigned long long free_bytes;
    double free_percent;
    int ok; /* 0 if statvfs failed (path missing, etc.) */
} StorageInfo;

/* Free space on the filesystem containing `path` (e.g. "/mnt/SDCARD").
 * free_bytes uses f_bavail (space available to a non-root user), which is
 * the honest "how much can I actually write" number. */
StorageInfo storage_get_free_space(const char *path);

#endif
