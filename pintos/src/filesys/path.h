#ifndef FILESYS_PATH_H
#define FILESYS_PATH_H

#include "lib/stdbool.h"

/**
 * Provide path-related functionalities.
 */

/** Return TRUE if PATH is valid */
bool path_isvalid(const char *path);

/** Return a normalized absolutized version of the path name PATH. */
char* path_abspath(const char* path);

/**
 * Return a normalized version of the path name PATH.
 * It collapses redundant separators and up-level references.
 * Examples:
 * path_normpath("a//b") => "a/b"
 * path_normpath("a/b/") => "a/b"
 * path_normpath("a/./b") => "a/b"
 * path_normpath("a/foo/../b") => "a/b"
 */
char* path_normpath(const char* path);

/**
 * Return PATH with its trailing /component removed. If PATH contains no
 * /'s, output '.' (the current directory)
 * Examples:
 * path_dirname("/usr/bin/sort") => "/usr/bin"
 * path_dirname("stdio.h") => "."
 */
char* path_dirname(const char* path);

/**
 * Return PATH with any leading directory components removed.
 * Example 1: path_basename("/usr/bin/sort") => "sort"
 * Example 2: path_basename("stdio.h") => "stdio.h"
 */
char* path_basename(const char* path);

/**
 * Join one or more path components intelligently. If any component is an
 * absolute path, all previous components (if there are any) are thrown away
 * and joining continues.
 * Return the concatenation of PATH1, and optionally PATH2, etc., with
 * exactly one directory separator following each non-empty part except the
 * last.
 */
char* path_join(int num_paths, const char* path1, ...);

/**
 * Join 2 path components intelligently. If any component is an
 * absolute path, all previous components (if there are any) are thrown away
 * and joining continues.
 * Return the concatenation of PATH1, and PATH2, with exactly one directory
 * separator following each non-empty part except the last.
 */
char* path_join2(const char *path1, const char *path2);

/** Return TRUE if PATH refers to an existing path. */
bool path_exists(const char* path);

/** Return TRUE if PATH is an absolute path name, i.e. begins with a slash */
bool path_isabs(const char* path);

/** Return TRUE if PATH is an existing regular file. */
bool path_isfile(const char* path);

/** Return TRUE if PATH is an existing directory. */
bool path_isdir(const char* path);

/** Return TRUE if PATH is the root directory (/) */
bool path_isroot(const char* path);

/** Return TRUE if PATH is the current directory (.) */
bool path_isdot(const char* path);

/** Return TRUE if PATH is the parent directory (..) */
bool path_isdotdot(const char* path);


#endif