#include "emu_table.h"
#include <stddef.h>

/* archive.org source availability confirmed live against the Metadata API
 * during planning (2026-07-19) — GB/GBC/GBA/FC/SFC are is_dark or flagged
 * with no files[], not a scraping bug. Re-check occasionally; archive.org
 * can un-dark a collection just as easily as it darked it. */
const EmuEntry EMU_TABLE[] = {
    {"PS",  "PlayStation",           "chd_psx_eur",                                         "chd", 1, "Sony_-_PlayStation"},
    {"MD",  "Sega Genesis",          "nointro.md",                                          "7z",  1, "Sega_-_Mega_Drive_-_Genesis"},
    {"GB",  "Game Boy",              "game-boy-and-game-boy-color-complete-collection",     "gb",  1, "Nintendo_-_Game_Boy"},
    {"GBC", "Game Boy Color",        "game-boy-and-game-boy-color-complete-collection",     "gbc", 1, "Nintendo_-_Game_Boy_Color"},
    {"GBA", "Game Boy Advance",      "CentralArquivista-GameBoyAdvance",                    "gba", 1, "Nintendo_-_Game_Boy_Advance"},
    {"FC",  "NES",                   "nintendo-entertainment-system-nes-roms-europeusa",  "nes", 1, "Nintendo_-_Nintendo_Entertainment_System"},
    {"SFC", "SNES",                  "CentralArquivista-SuperNintendo",                     "sfc", 1, "Nintendo_-_Super_Nintendo_Entertainment_System"},
    {"GG",  "Game Gear",             "nointro.gg",                                          "7z",  1, "Sega_-_Game_Gear"},
    {"MS",  "Master System",         "nointro.ms-mkiii",                                    "7z",  1, "Sega_-_Master_System_-_Mark_III"},
    {"PCE", "PC Engine",             "nointro.tg-16",                                       "7z",  1, "NEC_-_PC_Engine_-_TurboGrafx_16"},
    {"2600", "Atari 2600",           "nointro.atari-2600",                                  "7z",  1, "Atari_-_2600"},
    {"7800", "Atari 7800",           "nointro.atari-7800",                                  "7z",  1, "Atari_-_7800"},
};

const int EMU_TABLE_COUNT = sizeof(EMU_TABLE) / sizeof(EMU_TABLE[0]);
