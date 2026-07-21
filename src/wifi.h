#ifndef WIFI_H
#define WIFI_H

/* Link quality read from /proc/net/wireless for wlan0.
 *   -1  = no Wi-Fi / interface down / driver not exposing stats
 *    0..N (typically 0-70) = raw link quality, higher is better
 * Cheap enough to poll every couple of seconds. */
int wifi_get_signal(void);

#endif
