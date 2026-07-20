#include "thumbnail.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>

#define CACHE_DIR "thumb_cache"
#define TARGET_SIZE 120

static void tlog(const char *fmt, ...) {
    FILE *f = fopen("log.txt", "a");
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

/* archive.org file names can carry an internal path prefix (e.g.
 * "CHD-PSX-EUR/007 - ... .chd" for the PSX collection) — kept as-is in
 * RomEntry.name since Phase 4's download URL needs the full path, but
 * neither the box-art lookup nor the cache file name should include it. */
static void basename_no_ext(const char *in, char *out, size_t outsz) {
    const char *slash = strrchr(in, '/');
    const char *base = slash ? slash + 1 : in;
    const char *dot = strrchr(base, '.');
    size_t len = dot ? (size_t)(dot - base) : strlen(base);
    if (len >= outsz) len = outsz - 1;
    memcpy(out, base, len);
    out[len] = '\0';
}

/* Just enough percent-encoding for what actually shows up in No-Intro/
 * Redump-style filenames (space, parens, comma, apostrophe, ampersand) —
 * a full RFC3986 encoder is more than this needs. */
static void url_encode(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 4 < outsz; i++) {
        unsigned char c = (unsigned char)in[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out[o++] = (char)c;
        } else {
            o += (size_t)snprintf(out + o, outsz - o, "%%%02X", c);
        }
    }
    out[o] = '\0';
}

/* filesystem-safe version of the same base name, for the cache file */
static void sanitize_filename(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    for (size_t i = 0; in[i] && o + 1 < outsz; i++) {
        unsigned char c = (unsigned char)in[i];
        /* the resulting name gets embedded in a single-quoted shell string
         * (system("... '%s' ...")) — a raw apostrophe closes that quote
         * early and the rest of the name (still containing unescaped
         * parens, very common in game titles like "(Europe)") gets parsed
         * as shell syntax. Confirmed on-device via crash.txt: `sh: syntax
         * error: unexpected "("` on titles like "Kournikova's". Filtering
         * quote/backslash/backtick/dollar here removes the injection
         * vector at the source rather than trying to escape it later. */
        out[o++] = (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
                    c == '"' || c == '<' || c == '>' || c == '|' || c == '\'' ||
                    c == '`' || c == '$') ? '_' : (char)c;
    }
    out[o] = '\0';
}

static long file_size(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    return sz;
}

static void cache_path_for(const char *thumb_repo, const char *rom_name, char *out, size_t outsz) {
    char base[128], safe[160];
    (void)thumb_repo;
    basename_no_ext(rom_name, base, sizeof(base));
    sanitize_filename(base, safe, sizeof(safe));
    snprintf(out, outsz, "%s/%s.png", CACHE_DIR, safe);
}

static bool display_from_path(lv_obj_t *img, const char *cache_path) {
    char src[300];
    snprintf(src, sizeof(src), "S:%s", cache_path);
    lv_image_header_t header;
    lv_result_t res = lv_image_decoder_get_info(src, &header);
    if (res != LV_RESULT_OK || header.w == 0 || header.h == 0) {
        tlog("thumbnail: decode failed res=%d", (int)res);
        return false;
    }

    uint32_t zoom_w = (256u * TARGET_SIZE) / header.w;
    uint32_t zoom_h = (256u * TARGET_SIZE) / header.h;
    uint32_t zoom = zoom_w < zoom_h ? zoom_w : zoom_h;

    lv_image_set_src(img, src);
    lv_image_set_scale(img, zoom);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_HIDDEN);
    return true;
}

bool thumbnail_try_cache(lv_obj_t *img, const char *thumb_repo, const char *rom_name) {
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
    if (!thumb_repo || !rom_name) return false;

    char cache_path[256];
    cache_path_for(thumb_repo, rom_name, cache_path, sizeof(cache_path));
    if (file_size(cache_path) < 100) return false;

    return display_from_path(img, cache_path);
}

void thumbnail_fetch_network(lv_obj_t *img, const char *thumb_repo, const char *rom_name) {
    if (!thumb_repo || !rom_name) return;

    char base[128], cache_path[256];
    basename_no_ext(rom_name, base, sizeof(base));
    cache_path_for(thumb_repo, rom_name, cache_path, sizeof(cache_path));

    mkdir(CACHE_DIR, 0755); /* ignore EEXIST */

    char encoded[300], url[600], tmp_path[300], cmd[1024];
    url_encode(base, encoded, sizeof(encoded));
    snprintf(url, sizeof(url),
             "https://raw.githubusercontent.com/libretro-thumbnails/%s/master/Named_Boxarts/%s.png",
             thumb_repo, encoded);
    snprintf(tmp_path, sizeof(tmp_path), "%s.part", cache_path);
    snprintf(cmd, sizeof(cmd), "wget -q -T 5 -O '%s' '%s' 2>/dev/null", tmp_path, url);
    tlog("thumbnail: MISS, fetching url=%s", url);

    remove(tmp_path);
    if (system(cmd) != 0) {
        tlog("thumbnail: wget failed");
        remove(tmp_path);
        return;
    }
    long sz = file_size(tmp_path);
    if (sz < 100) { /* too small to be a real PNG — 404 page or empty response */
        tlog("thumbnail: too small (%ld bytes), treating as miss", sz);
        remove(tmp_path);
        return;
    }
    rename(tmp_path, cache_path);
    tlog("thumbnail: cached %ld bytes -> %s", sz, cache_path);

    display_from_path(img, cache_path);
}
