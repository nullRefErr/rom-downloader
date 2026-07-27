#include "wifi.h"
#include <stdio.h>
#include <string.h>

/* Typical ceiling of the "link" column in /proc/net/wireless on this
 * driver; used to scale the raw quality into a percentage. */
#define LINK_QUALITY_MAX 70

static int read_first_line(const char *path, char *buf, size_t sz) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    if (!fgets(buf, (int)sz, f)) {
        fclose(f);
        return 0;
    }
    fclose(f);
    size_t n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = '\0';
    return 1;
}

/* Turn the driver's numbers into a 0-100 percentage.
 * `level` is dBm on drivers that set IW_QUAL_DBM (always negative, roughly
 * -30 excellent .. -90 unusable); that's comparable across drivers, so it's
 * preferred. The raw `qual` column is not — its ceiling is 70 on mac80211
 * but 100 on several out-of-tree Realtek SDIO drivers — so it's only the
 * fallback for drivers that report a non-dBm level. */
static int to_percent(int qual, int level) {
    int pct;
    if (level < 0) {
        pct = 2 * (level + 100);
    } else {
        pct = qual * 100 / LINK_QUALITY_MAX;
    }
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

/* Signal strength (0-100) for the first wireless interface, or -1 if there
 * is no interface row at all / it reports nothing.
 *
 * /proc/net/wireless looks like:
 *   Inter-| sta-|   Quality        |   Discarded packets ...
 *    face | tus | link level noise |  nwid  crypt ...
 *   wlan0: 0000   54.  -56.  -256        0 ...
 *
 * The two header rows carry no interface colon, so keying on ':' skips them
 * without hard-coding "read two lines then parse the third".
 *
 * The status column is printed as %04x, so it MUST be read with %x: parsing
 * it with %d stops at the first hex letter ("000a"), the rest of the row
 * fails to convert, and the interface is skipped entirely — which showed up
 * as a false "offline" while actually connected. The quality field carries a
 * trailing updated-flag char ('.'), eaten with %*c. */
static int link_strength(const char *proc_path) {
    FILE *f = fopen(proc_path, "r");
    if (!f) return -1;

    char line[256];
    int strength = -1;
    while (fgets(line, sizeof(line), f)) {
        char *colon = strchr(line, ':');
        if (!colon) continue;
        unsigned status;
        int qual, level;
        int n = sscanf(colon + 1, "%x %d%*c %d", &status, &qual, &level);
        if (n >= 2) {
            (void)status;
            strength = to_percent(qual, n >= 3 ? level : 0);
            break;
        }
    }
    fclose(f);
    return strength;
}

/* Split out so the parsing/decision logic can be exercised against fixture
 * files in a self-check without touching the real /proc and /sys. */
int wifi_signal_from(const char *operstate_path, const char *carrier_path,
                      const char *wireless_path) {
    /* The load-bearing fix: an interface that exists but is switched off
     * KEEPS its /proc/net/wireless row, just with zeroed quality (the kernel
     * prints it from a zeroed nullstats struct). The old "a row exists =>
     * connected" test therefore lit the Wi-Fi indicator green permanently,
     * even with Wi-Fi turned off (reported directly).
     *
     * Three independent offline signals, any one of which is conclusive.
     * Each is only consulted when readable, so a device missing any of these
     * files still degrades to the remaining checks instead of guessing. Note
     * only an explicit "down" counts: some drivers sit at "unknown" while
     * perfectly connected, and "dormant" means associating — neither should
     * become a false offline, and both are caught by the strength check
     * below if they really aren't connected. */
    char buf[32];
    if (read_first_line(operstate_path, buf, sizeof(buf)) && strcmp(buf, "down") == 0) {
        return -1;
    }
    /* carrier reads back EINVAL while the interface isn't running, so an
     * unreadable carrier is not evidence either way — only an explicit 0 is. */
    if (read_first_line(carrier_path, buf, sizeof(buf)) && strcmp(buf, "0") == 0) {
        return -1;
    }

    int strength = link_strength(wireless_path);
    if (strength <= 0) return -1; /* no interface row, or associated with nothing */
    return strength;
}

int wifi_get_signal(void) {
    return wifi_signal_from("/sys/class/net/wlan0/operstate",
                            "/sys/class/net/wlan0/carrier",
                            "/proc/net/wireless");
}
