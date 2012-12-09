#include "filesys/path.h"

#include "lib/stdbool.h"
#include "lib/string.h"
#include "lib/stdarg.h"
#include "lib/stdio.h"
#include "lib/debug.h"

#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/interrupt.h"

#include "filesys/directory.h"

/** Return TRUE if PATH is valid */
bool
path_isvalid(const char *path)
{
  enum intr_level old_level = intr_disable();
  size_t i = 0;
  char c = 0;
  bool success = (path != NULL) &&      // Name cannot be NULL
                 (strlen(path) > 0);    // Must have at least 1 character

  while (i < strlen(path) && success) {
    c = (char) path[i];
    success = (
                (c == 45) ||                  // dash
                (c == 46) ||                  // period
                (c == 47) ||                  // slash
                (c >= 48 && c <= 57) ||       // numbers
                (c >= 65 && c <= 90) ||       // upper case letters
                (c == 95) ||                  // underscore
                (c >= 97 && c <= 122)         // lower case letters
              );
    i++;
  }

  intr_set_level(old_level);
  return success;
}

/** Return a normalized absolutized version of the path name PATH. */
char *path_abspath(const char *path)
{
  return path_normpath(path_join2(path_cwd(), path));
}

char *path_cwd(void)
{
  enum intr_level old_level = intr_disable();
  char* cwd = malloc(sizeof(thread_current()->pwd));
  strlcpy(cwd, &thread_current()->pwd[0], sizeof(thread_current()->pwd) + 1);
  intr_set_level(old_level);
  return cwd;
}

/**
 * Return a normalized version of the path name PATH.
 * It collapses redundant separators and up-level references.
 * Examples:
 * path_normpath("a//b") => "a/b"
 * path_normpath("a/b/") => "a/b"
 * path_normpath("a/./b") => "a/b"
 * path_normpath("a/foo/../b") => "a/b"
 */
char *path_normpath(const char *path)
{
  enum intr_level old_level = intr_disable();
  char *tempname = malloc(strlen(path) + 1);
  char *token, *ret = NULL, *save_ptr;

  strlcpy(tempname, path, strlen(path) + 1);

  for (token = strtok_r(tempname, "/", &save_ptr); token != NULL;
       token = strtok_r(NULL, "/", &save_ptr)) {
    // Saw a ".."
    if (path_isdotdot(token)) {
      if (ret) {
        // If possible, discard the parent
        ret = path_dirname(ret);
        continue;
      }
      else {
        // Undefined edge case: anything goes
        if (path_isabs(tempname)) {
          ret = "/";
          continue;
        }
        else {
          ret = token;
          continue;
        }
      }
    }
    // Saw a "."
    else if (path_isdot(token)) {
      // Just pretend we didn't see it. Walk away.
      continue;
    }
    // Saw a normal token
    else {
      if (ret == NULL) {
        if (path_isabs(path)) {
          ret = path_join2("/", token);
        }
        else {
          ret = token;
        }
      }
      else {
        ret = path_join2(ret, token);
      }
    }
  }

  if (ret == NULL) {
    if (path_isabs(path)) {
      ret = "/";
    }
    else {
      ret = ".";
    }
  }

  intr_set_level(old_level);
  return ret;
}

/**
 * Return PATH with its trailing /component removed. If PATH contains no
 * /'s, output '.' (the current directory)
 * Example 1: path_dirname("/usr/bin/sort") => "/usr/bin"
 * Example 2: path_dirname("stdio.h") => "."
 */
char *path_dirname(const char *path)
{
  enum intr_level old_level = intr_disable();
  char *tempname = malloc(strlen(path) + 1);
  char *sentry, *ret = NULL, *save_ptr, *token = NULL;

  strlcpy(tempname, path, strlen(path) + 1);

  for (sentry = strtok_r(tempname, "/", &save_ptr); sentry != NULL;
       sentry = strtok_r(NULL, "/", &save_ptr)) {
    if (token != NULL) {
      if (ret == NULL) {
        if (path_isabs(path)) {
          ret = path_join2("/", token);
        }
        else {
          ret = token;
        }
      }
      else {
        ret = path_join2(ret, token);
      }
    }

    token = sentry;
  }

  if (ret == NULL) {
    if (path_isabs(path)) {
      ret = "/";
    }
    else {
      ret = ".";
    }
  }

  intr_set_level(old_level);
  return ret;
}

/**
 * Return PATH with any leading directory components removed.
 * Example 1: path_basename("/usr/bin/sort") => "sort"
 * Example 2: path_basename("stdio.h") => "stdio.h"
 */
char *path_basename(const char *path)
{
  enum intr_level old_level = intr_disable();
  char *tempname = malloc(strlen(path) + 1);
  char *token = NULL, *ret = NULL, *save_ptr = NULL;

  strlcpy(tempname, path, strlen(path) + 1);

  for (token = strtok_r(tempname, "/", &save_ptr); token != NULL;
       token = strtok_r(NULL, "/", &save_ptr)) {
    ret = token;
  }

  if (ret == NULL) {
    if (path_isabs(path)) {
      ret = "/";
    }
    else {
      ret = ".";
    }
  }

  intr_set_level(old_level);
  return ret;
}

/**
 * Join one or more path components intelligently. If any component is an
 * absolute path, all previous components (if there are any) are thrown away
 * and joining continues.
 * Return the concatenation of PATH1, and optionally PATH2, etc., with
 * exactly one directory separator following each non-empty part except the
 * last.
 */
char *path_join(int num_paths, const char *path1, ...)
{
  va_list args;
  int i = 0;
  char *ret, *tmp;

  ret = malloc(strlen(path1) + 1);
  strlcpy(ret, path1, strlen(path1) + 1);

  va_start(args, path1);

  for (i = 0; i < num_paths; i++) {
    tmp = va_arg(args, char *);
    ret = path_join2(ret, tmp);
  }

  va_end(args);

  return ret;
}

/**
 * Join 2 path components intelligently. If any component is an
 * absolute path, all previous components (if there are any) are thrown away
 * and joining continues.
 * Return the concatenation of PATH1, and PATH2, with exactly one directory
 * separator following each non-empty part except the last.
 */
char *
path_join2(const char *path1, const char *path2)
{
  ASSERT(path1 != NULL || path2 != NULL);
  enum intr_level old_level = intr_disable();
  
  char *ret;

  if (path2 == NULL)
  {
    ret = malloc(strlen(path1) + 1);
    strlcpy(ret, path1, strlen(path1) + 1);
  }
  else if (path1 == NULL || path_isabs(path2))
  {
    // If PATH2 is an absolute path, then just return it
    ret = malloc(strlen(path2) + 1);
    strlcpy(ret, path2, strlen(path2) + 1);
  }
  else
  {
    // Join PATH1 and PATH2
    if (path1[strlen(path1) - 1] == '/')
    {
      ret = malloc(strlen(path1) + strlen(path2) + 1);
      strlcpy(ret, path1, strlen(path1) + 1);       // copy all but '\0'
      strlcpy(ret + strlen(path1),
              path2, strlen(path2) + 1);        // copy all of path2
    }
    else
    {
      ret = malloc(strlen(path1) + strlen(path2) + 1);
      strlcpy(ret, path1, strlen(path1) + 1);       // copy all but '\0'
      strlcpy(ret + strlen(path1), "/", 2);     // copy the '/'
      strlcpy(ret + strlen(path1) + 1,
              path2, strlen(path2) + 1);        // copy all of path2
    }
  }

  intr_set_level(old_level);
  return ret;
}

/**
 * Return TRUE if PATH refers to an existing path.
 */
bool path_exists(const char *path)
{
  return path_isvalid(path);
}

/**
 * Return TRUE if PATH is an absolute path name, i.e. it begins with a slash
 */
bool path_isabs(const char *path)
{
  enum intr_level old_level = intr_disable();
  bool success = path_isvalid(path);

  if (success) {
    success = (path[0] == '/');
  }

  intr_set_level(old_level);
  return success;
}

/**
 * Return TRUE if PATH is an existing regular file.
 */
bool path_isfile(const char *path)
{
  return false;
}

/**
 * Return TRUE if PATH is an existing directory.
 */
bool path_isdir(const char *path)
{
  return false;
}

/** Return TRUE if PATH is the root directory (/) */
bool path_isroot(const char *path)
{
  return (path_isvalid(path) &&
          strlen(path) == 1 && path[0] == '/');
}

/** Return TRUE if PATH is the current directory (.) */
bool path_isdot(const char *path)
{
  return (path_isvalid(path) &&
          strlen(path) == 1 && path[0] == '.');
}

/** Return TRUE if PATH is the parent directory (..) */
bool path_isdotdot(const char *path)
{
  return (path_isvalid(path) &&
          strlen(path) == 2 &&
          path[0] == '.' && path[1] == '.');
}
