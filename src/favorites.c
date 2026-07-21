#include "favorites.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char **g_favs;
static int g_count;
static int g_cap;
static char g_path[128];

static void clear(void) {
    for (int i = 0; i < g_count; i++) free(g_favs[i]);
    g_count = 0;
}

static int find(const char *name) {
    for (int i = 0; i < g_count; i++) {
        if (strcmp(g_favs[i], name) == 0) return i;
    }
    return -1;
}

static void push(const char *name) {
    if (g_count == g_cap) {
        int ncap = g_cap ? g_cap * 2 : 32;
        char **n = realloc(g_favs, (size_t)ncap * sizeof(char *));
        if (!n) return;
        g_favs = n;
        g_cap = ncap;
    }
    g_favs[g_count] = strdup(name);
    if (g_favs[g_count]) g_count++;
}

static void save(void) {
    FILE *f = fopen(g_path, "w");
    if (!f) return;
    for (int i = 0; i < g_count; i++) fprintf(f, "%s\n", g_favs[i]);
    fclose(f);
}

void favorites_load(const char *emu_code) {
    clear();
    snprintf(g_path, sizeof(g_path), "favorites_%s.txt", emu_code);

    FILE *f = fopen(g_path, "r");
    if (!f) return; /* no favourites file yet — that's fine */

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (len > 0) push(line);
    }
    fclose(f);
}

bool favorites_is(const char *display_name) {
    return find(display_name) >= 0;
}

void favorites_toggle(const char *display_name) {
    int i = find(display_name);
    if (i >= 0) {
        free(g_favs[i]);
        g_favs[i] = g_favs[--g_count]; /* swap-remove, order doesn't matter */
    } else {
        push(display_name);
    }
    save();
}
