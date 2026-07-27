#ifndef WIFI_H
#define WIFI_H

/* Wi-Fi connection state for the header indicator.
 *   -1  = not connected (interface down, absent, or associated with nothing)
 *    0..100 = connected, with link quality as a percentage
 * Cheap enough to poll every couple of seconds. */
int wifi_get_signal(void);

#endif
