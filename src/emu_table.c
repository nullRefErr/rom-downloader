#include "emu_table.h"
#include <stddef.h>

/* Built-in defaults only. The live list comes from sources.c, which prefers
 * sources.json on the card — sources go away without warning (five of these
 * were taken down during development and the PlayStation one moved behind a
 * login), and users should be able to repoint one without waiting for a
 * release, or point at something they host themselves.
 *
 * Field order: code, label, archive_id, url, base, ext, thumb_repo, available.
 */
const EmuEntry EMU_DEFAULTS[] = {
    {"PS",   "PlayStation",      "chd_psx_eur",                                      "", "", "chd", "Sony_-_PlayStation", 1},
    {"MD",   "Sega Genesis",     "nointro.md",                                       "", "", "7z",  "Sega_-_Mega_Drive_-_Genesis", 1},
    {"GB",   "Game Boy",         "game-boy-and-game-boy-color-complete-collection",   "", "", "gb",  "Nintendo_-_Game_Boy", 1},
    {"GBC",  "Game Boy Color",   "game-boy-and-game-boy-color-complete-collection",   "", "", "gbc", "Nintendo_-_Game_Boy_Color", 1},
    {"GBA",  "Game Boy Advance", "CentralArquivista-GameBoyAdvance",                  "", "", "gba", "Nintendo_-_Game_Boy_Advance", 1},
    {"FC",   "NES",              "nintendo-entertainment-system-nes-roms-europeusa",  "", "", "nes", "Nintendo_-_Nintendo_Entertainment_System", 1},
    {"SFC",  "SNES",             "CentralArquivista-SuperNintendo",                   "", "", "sfc", "Nintendo_-_Super_Nintendo_Entertainment_System", 1},
    {"GG",   "Game Gear",        "nointro.gg",                                        "", "", "7z",  "Sega_-_Game_Gear", 1},
    {"MS",   "Master System",    "nointro.ms-mkiii",                                  "", "", "7z",  "Sega_-_Master_System_-_Mark_III", 1},
    {"PCE",  "PC Engine",        "nointro.tg-16",                                     "", "", "7z",  "NEC_-_PC_Engine_-_TurboGrafx_16", 1},
    {"2600", "Atari 2600",       "nointro.atari-2600",                                "", "", "7z",  "Atari_-_2600", 1},
    {"7800", "Atari 7800",       "nointro.atari-7800",                                "", "", "7z",  "Atari_-_7800", 1},
    /* Unlike the others this is not a No-Intro/Redump set — a small
     * user-uploaded collection (~48 titles), the only NDS source found that
     * is neither dark nor login-restricted and actually serves its files.
     * Names don't follow the No-Intro convention, so box art misses more
     * often here. Onion's NDS emulator accepts 7z (extlist: nds|zip|7z|rar). */
    {"NDS",  "Nintendo DS",      "nds-roms_202310",                                   "", "", "7z",  "Nintendo_-_Nintendo_DS", 1},
};

const int EMU_DEFAULTS_COUNT = sizeof(EMU_DEFAULTS) / sizeof(EMU_DEFAULTS[0]);
