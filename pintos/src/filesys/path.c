#include "filesys/path.h"

// #include "lib/stdbool.h"
// #include "lib/string.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>

static bool path_isvalid(const char *path);

/** Return TRUE if PATH is valid */
static bool
path_isvalid(const char *path)
{
  size_t i = 0;
  char c = 0;
  bool success = (path != NULL) &&      // Name cannot be NULL
                 (strlen(path) > 0);    // Must have at least 1 character

  while (i < strlen(path) && success)
  {
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

  return success;
}

/**
 * Return PATH with its trailing /component removed. If PATH contains no
 * /'s, output '.' (the current directory)
 * Example 1: path_dirname("/usr/bin/sort") => "/usr/bin"
 * Example 2: path_dirname("stdio.h") => "."
 */
char* path_dirname(const char* path)
{
  char *tempname = malloc(strlen(path) + 1);
  char *token, *ret = NULL, *save_ptr, *sentry = NULL;

  strncpy(tempname, path, strlen(path) + 1);

  for (token = strtok_r(tempname, "/", &save_ptr); token != NULL;
       token = strtok_r(NULL, "/", &save_ptr))
  {
    if (sentry != NULL)
    {
      if (ret == NULL)
      {
        if (path_isabs(path))
        {
          ret = path_join2("/", sentry);
        }
        else
        {
          ret = sentry;
        }
      }
      else
      {
        ret = path_join2(ret, sentry);
      }
    }
    sentry = token;
  }

  if (ret == NULL)
  {
    ret = malloc(2);
    strncpy(ret, ".", 2);
  }

  return ret;
}

/**
 * Return PATH with any leading directory components removed.
 * Example 1: path_basename("/usr/bin/sort") => "sort"
 * Example 2: path_basename("stdio.h") => "stdio.h"
 */
char* path_basename(const char* path)
{
  char *tempname = malloc(strlen(path) + 1);
  char *token, *ret, *save_ptr;

  strncpy(tempname, path, strlen(path) + 1);

  for (token = strtok_r(tempname, "/", &save_ptr); token != NULL;
       token = strtok_r(NULL, "/", &save_ptr))
  {
    ret = token;
  }

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
char* path_join(int num_paths, const char *path1, ...)
{
  va_list args;
  int i = 0;
  char *ret, *tmp;

  ret = malloc(strlen(path1) + 1);
  strncpy(ret, path1, strlen(path1) + 1);

  va_start(args, path1);
  for (i = 0; i < num_paths; i++)
  {
    tmp = va_arg(args, char*);
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
char*
path_join2(const char *path1, const char *path2)
{
//   ASSERT(path1 != NULL);
//   ASSERT(path2 != NULL);
  char *ret;

  if (path_isabs(path2))
  {
    // If PATH2 is an absolute path, then just return it
    ret = malloc(strlen(path2) + 1);
    strncpy(ret, path2, strlen(path2) + 1);
  }
  else
  {
    // Join PATH1 and PATH2
    if (path1[strlen(path1) - 1] == '/')
    {
      ret = malloc(strlen(path1) + strlen(path2) + 1);
      strncpy(ret, path1, strlen(path1));       // copy all but '\0'
      strncpy(ret + strlen(path1),
              path2, strlen(path2) + 1);        // copy all of path2
    }
    else
    {
      ret = malloc(strlen(path1) + strlen(path2) + 1);
      strncpy(ret, path1, strlen(path1));       // copy all but '\0'
      strncpy(ret + strlen(path1), "/", 1);     // copy the '/'
      strncpy(ret + strlen(path1) + 1,
              path2, strlen(path2) + 1);        // copy all of path2
    }
  }

  return ret;
}

/**
 * Return TRUE if PATH refers to an existing path.
 */
bool path_exists(const char* path)
{
  return path_isvalid(path);
}

/**
 * Return TRUE if PATH is an absolute path name, i.e. it begins with a slash
 */
bool path_isabs(const char* path)
{
  bool success = path_isvalid(path);
  if (success)
  {
    success = (path[0] == '/');
  }
  return success;
}

/**
 * Return TRUE if PATH is an existing regular file.
 */
bool path_isfile(const char* path)
{
  return false;
}

/**
 * Return TRUE if PATH is an existing directory.
 */
bool path_isdir(const char* path)
{
  return false;
}
