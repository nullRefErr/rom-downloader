#ifndef FILTER_H
#define FILTER_H

#include <stdbool.h>

/* Region filtering over No-Intro / Redump style names, e.g.
 * "Crash Bandicoot (Europe) (En,Fr,De).chd". The spec also proposed
 * Turkish-translation and [Hack] filters, but the live archive.org PS1/
 * Genesis/etc. collections this app pulls from are essentially all clean
 * No-Intro/Redump dumps — those two filters would match ~nothing, so
 * they're deliberately left out rather than shipped as dead options. */
typedef enum {
    REGION_ALL,
    REGION_USA,
    REGION_EUROPE,
    REGION_JAPAN,
    REGION_WORLD,
    REGION_COUNT
} RegionFilter;

const char *region_label(RegionFilter r);

/* True if `filename` matches region `r` (REGION_ALL always matches). */
bool region_matches(const char *filename, RegionFilter r);

#endif
