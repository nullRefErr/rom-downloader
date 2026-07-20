#ifndef EMU_TABLE_H
#define EMU_TABLE_H

typedef struct {
    const char *code;       /* Onion Roms/<code> folder name */
    const char *label;      /* shown in the UI */
    const char *archive_id; /* archive.org identifier */
    const char *ext;        /* rom file extension at that source */
    int available;          /* 0 = archive.org source confirmed dark/dead */
    const char *thumb_repo; /* libretro-thumbnails repo name, NULL = no box art */
} EmuEntry;

extern const EmuEntry EMU_TABLE[];
extern const int EMU_TABLE_COUNT;

#endif
