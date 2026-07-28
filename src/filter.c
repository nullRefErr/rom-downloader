#include "filter.h"
#include "i18n.h"
#include <string.h>
#include <stdio.h>

const char *region_label(RegionFilter r) {
    switch (r) {
        case REGION_USA:    return T("region_usa");
        case REGION_EUROPE: return T("region_europe");
        case REGION_JAPAN:  return T("region_japan");
        case REGION_WORLD:  return T("region_world");
        case REGION_ALL:
        default:            return T("region_all");
    }
}

/* No-Intro puts the region(s) in the first parenthesised group, comma-
 * separated, e.g. "Tekken 3 (USA, Europe)". A single region is preceded by
 * "(", any subsequent one by ", " — so matching only "(Region" would miss
 * the 2nd+ region of a multi-region dump (caught by the self-check). Match
 * both boundaries; requiring "(" or ", " before the word also keeps a plain
 * title word (e.g. a game literally named "Europe ...") from matching. */
static bool has_region(const char *s, const char *region) {
    char open[24], comma[24];
    snprintf(open, sizeof(open), "(%s", region);
    snprintf(comma, sizeof(comma), ", %s", region);
    return strstr(s, open) != NULL || strstr(s, comma) != NULL;
}

bool region_matches(const char *filename, RegionFilter r) {
    switch (r) {
        case REGION_USA:    return has_region(filename, "USA");
        case REGION_EUROPE: return has_region(filename, "Europe");
        case REGION_JAPAN:  return has_region(filename, "Japan");
        case REGION_WORLD:  return has_region(filename, "World");
        case REGION_ALL:
        default:            return true;
    }
}
