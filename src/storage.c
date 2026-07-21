#include "storage.h"
#include <sys/statvfs.h>

StorageInfo storage_get_free_space(const char *path) {
    StorageInfo info = {0};
    struct statvfs vfs;
    if (statvfs(path, &vfs) == 0 && vfs.f_blocks > 0) {
        info.total_bytes = (unsigned long long)vfs.f_blocks * vfs.f_frsize;
        info.free_bytes = (unsigned long long)vfs.f_bavail * vfs.f_frsize;
        info.free_percent = ((double)info.free_bytes / (double)info.total_bytes) * 100.0;
        info.ok = 1;
    }
    return info;
}
