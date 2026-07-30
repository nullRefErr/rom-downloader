#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

/* Small helpers that had grown a copy per module.
 *
 * read-a-whole-file existed three times (settings.c, i18n.c, sources.c) and
 * byte formatting twice — and the two size formatters had already drifted
 * apart: one took unsigned long and printed nothing for zero, the other took
 * unsigned long long and printed "0 B". That is the usual way copies go
 * wrong, so they are one function each now. */

/* Reads `path` whole into a NUL-terminated heap buffer the caller frees.
 * Returns NULL when the file is missing, empty, unreadable, or larger than
 * `max_bytes` — a cap rather than an unbounded read, since these are config
 * files and a huge one means something is wrong, not that we should load it. */
char *util_read_file(const char *path, long max_bytes);

/* "1.4 GB" / "265.0 MB" / "512 B". Always writes something, including for
 * zero; callers that want an empty string for an unknown size check first. */
void util_format_size(unsigned long long bytes, char *out, size_t outsz);

/* Wraps `in` in single quotes with POSIX '\'' escaping, so it can be dropped
 * into a system()/popen() command as one argument no matter what it contains.
 *
 * Every URL this app runs through a shell needs this now. It used to be
 * enough that only rom names were untrusted (they get percent-encoded), but
 * source URLs are typed by the user in the sources editor, and an apostrophe
 * in one closes the quote and hands the rest to sh — the same crash rom names
 * with apostrophes used to cause. Truncates rather than overflows. */
void util_shell_quote(const char *in, char *out, size_t outsz);

/* Byte size of `path`, or -1 if it can't be opened. */
long util_file_size(const char *path);

/* Percent-encodes everything outside the RFC 3986 unreserved set, plus any
 * character listed in `keep` (pass "" for none). Three copies of this existed:
 * download.c needed '/' left literal because archive.org item paths are real
 * subdirectory paths, thumbnail.c and net_login.c both wanted the strict set.
 * Only the keep-set ever differed. */
void util_url_encode(const char *in, char *out, size_t outsz, const char *keep);

/* Runs `cmd` through the shell and returns everything it wrote to stdout as a
 * NUL-terminated heap buffer the caller frees. Callers build their own command
 * string — what differed between the three copies of this was the wget flags
 * and the failure handling, never the read-it-all-back plumbing.
 *
 * out_exit == NULL: strict. NULL if the command could not start, exited
 *   non-zero, or produced nothing — for fetches where a partial body is
 *   useless anyway.
 * out_exit != NULL: receives the exit status (-1 if the command never ran)
 *   and the captured output is returned regardless, because the status is
 *   itself the information: curl's code is what separates "no network" from
 *   "certificate rejected" in the sign-in flow. */
char *util_run_capture(const char *cmd, int *out_exit);

#endif
