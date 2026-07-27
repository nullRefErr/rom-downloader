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

/* Raw "link" column for the first wireless interface, or -1 if there is no
 * interface row at all.
 *
 * /proc/net/wireless looks like:
 *   Inter-| sta-|   Quality        |   Discarded packets ...
 *    face | tus | link level noise |  nwid  crypt ...
 *   wlan0: 0000   54.  -56.  -256        0 ...
 *
 * The two header rows carry no interface colon, so keying on ':' skips them
 * without hard-coding "read two lines then parse the third". */
static int link_quality(const char *proc_path) {
    FILE *f = fopen(proc_path, "r");
    if (!f) return -1;

    char line[256];
    int quality = -1;
    while (fgets(line, sizeof(line), f)) {
        char *colon = strchr(line, ':');
        if (!colon) continue;
        int status, link;
        if (sscanf(colon + 1, "%d %d", &status, &link) == 2) {
            (void)status;
            quality = link;
            break;
        }
    }
    fclose(f);
    return quality;
}

/* Split out so the parsing/decision logic can be exercised against fixture
 * files in a self-check without touching the real /proc and /sys. */
int wifi_signal_from(const char *operstate_path, const char *wireless_path) {
    /* The load-bearing fix: an interface that exists but is switched off
     * KEEPS its /proc/net/wireless row, just with zeroed quality. The old
     * "a row exists => connected" test therefore lit the Wi-Fi indicator
     * green permanently, even with Wi-Fi turned off (reported directly).
     * Only an explicit "down" is treated as authoritative here — some
     * drivers report "unknown" while perfectly connected, so anything else
     * falls through to the quality check rather than being called offline. */
    char state[32];
    if (read_first_line(operstate_path, state, sizeof(state))) {
        if (strcmp(state, "down") == 0) return -1;
    }

    int q = link_quality(wireless_path);
    if (q <= 0) return -1; /* no interface row, or associated with nothing */

    int pct = q * 100 / LINK_QUALITY_MAX;
    if (pct > 100) pct = 100;
    return pct;
}

int wifi_get_signal(void) {
    return wifi_signal_from("/sys/class/net/wlan0/operstate", "/proc/net/wireless");
}
