#include "emu_table.h"
#include <stddef.h>

/* archive.org source availability confirmed live against the Metadata API
 * during planning (2026-07-19) — GB/GBC/GBA/FC/SFC are is_dark or flagged
 * with no files[], not a scraping bug. Re-check occasionally; archive.org
 * can un-dark a collection just as easily as it darked it. */
const EmuEntry EMU_TABLE[] = {
    {"PS",  "PlayStation",           "chd_psx_eur",   "chd", 1, "Sony_-_PlayStation"},
    {"MD",  "Sega Genesis",          "nointro.md",    "7z",  1, "Sega_-_Mega_Drive_-_Genesis"},
    {"GB",  "Game Boy",              "nointro.gb",    "7z",  0, NULL},
    {"GBC", "Game Boy Color",        "nointro.gbc-1", "7z",  0, NULL},
    {"GBA", "Game Boy Advance",      "nointro.gba",   "7z",  0, NULL},
    {"FC",  "NES",                   "nointro.nes",   "7z",  0, NULL},
    {"SFC", "SNES",                  "nointro.snes",  "7z",  0, NULL},
};

const int EMU_TABLE_COUNT = sizeof(EMU_TABLE) / sizeof(EMU_TABLE[0]);
