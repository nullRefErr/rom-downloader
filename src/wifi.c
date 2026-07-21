#include "wifi.h"
#include <stdio.h>
#include <string.h>

/* /proc/net/wireless looks like:
 *
 *   Inter-| sta-|   Quality        |   Discarded packets ...
 *    face | tus | link level noise |  nwid  crypt ...
 *   wlan0: 0000   54.  -56.  -256        0 ...
 *
 * We want the "link" column (54 above). Rather than assume it's always the
 * 3rd physical line (some builds add/omit interfaces), scan for the wlan0
 * row explicitly. The value carries a trailing '.'; %d stops at it. */
int wifi_get_signal(void) {
    FILE *f = fopen("/proc/net/wireless", "r");
    if (!f) return -1;

    char line[256];
    int signal = -1;
    while (fgets(line, sizeof(line), f)) {
        char *colon = strchr(line, ':');
        if (!colon) continue; /* header lines have no interface colon */
        /* interface name is everything up to the colon (e.g. "wlan0",
         * possibly leading spaces) — accept any wireless interface, but
         * wlan0 is the only one on this device */
        int status, link;
        if (sscanf(colon + 1, "%d %d", &status, &link) == 2) {
            (void)status;
            signal = link;
            break;
        }
    }
    fclose(f);
    return signal;
}
