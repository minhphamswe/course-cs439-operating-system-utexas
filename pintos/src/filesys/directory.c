#include "filesys/directory.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "lib/kernel/list.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

// Function to check if a string has bad characters for a directory
bool is_valid_name(const char *name);

/* Creates the root directory at the sector given.
   Returns true if successful, false on failure. */
bool
dir_create_root(block_sector_t sector)
{
  bool success = 0;
  struct inode node;
  success = inode_create(sector, BLOCK_SECTOR_SIZE);
  node.data.file_length = 0;
  node.data.prev_length = 0;
  node.data.node_length = 0;
  node.data.self = ptr_create(&node.sector);
  node.data.magic = INODE_MAGIC;
  node.data.doubleptr = NULL;
  block_write(fs_device, sector, &node.data);
  return success;
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open(struct inode *inode)
{
  struct dir *dir = calloc(1, sizeof *dir);

  if (inode != NULL && dir != NULL)
  {
    dir->inode = inode;
    dir->pos = 0;
    return dir;
  }
  else
  {
    inode_close(inode);
    free(dir);
    return NULL;
  }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root(void)
{
  return dir_open(inode_open(ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen(struct dir *dir)
{
  return dir_open(inode_reopen(dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close(struct dir *dir)
{
  if (dir != NULL)
  {
    inode_close(dir->inode);
    free(dir);
  }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode(struct dir *dir)
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup(const struct dir *dir, const char *name,
       struct dir_entry *ep, off_t *ofsp)
{
//  printf("lookup %s\n", name);
  if (!is_valid_name(name))
    return 0;

  struct dir_entry e;
  size_t ofs;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  for (ofs = 0; inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (e.in_use && !strcmp(name, e.name))
    {
      if (ep != NULL)
        *ep = e;

      if (ofsp != NULL)
        *ofsp = ofs;

      return true;
    }

  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup(const struct dir *dir, const char *name,
           struct inode **inode)
{
//   printf("dirlookup 1  %s\n", name);
  struct dir_entry e;

  // Change to pathed directory
  dir = dir_get_leaf(name);

  // Strip path off of file name
  char *token, *prevtoken;
  char *tempname = calloc(1, 256);
  char *save_ptr;

  prevtoken = NULL;

  strlcpy(tempname, name, 256);

  for (token = strtok_r(tempname, "/", &save_ptr); token != NULL;
       token = strtok_r(NULL, "/", &save_ptr))
  {
    prevtoken = token;
  }

  // If it was just a name and not a path, token will be null
  // In that case, a recopy will suffice
  if (prevtoken == NULL)
  {
    token = calloc(1, NAME_MAX + 1);
    strlcpy(token, name, NAME_MAX + 1);
  }
  // Otherwise, we want the last
  else
  {
    token = calloc(1, NAME_MAX + 1);
    strlcpy(token, prevtoken, NAME_MAX + 1);
  }

  ASSERT(dir != NULL);
  ASSERT(token != NULL);

  /* Check NAME for validity. */
  if (*token == '\0' || strlen(token) > NAME_MAX)
  {
    free(tempname);
    free(token);
    return false;
  }

// printf("dir_lookup 2\n");
  if (lookup(dir, token, &e, NULL))
  {
    *inode = inode_open(e.inode_sector);

    if (e.is_dir)
      (*inode)->is_dir = 1;
    else
      (*inode)->is_dir = 0;
  }
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add(struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  // Change to pathed directory
  dir = dir_get_leaf(name);

  // Strip path off of file name
  char *token, *prevtoken;
  char *tempname = calloc(1, 256);
  char *save_ptr;

  prevtoken = NULL;

  strlcpy(tempname, name, 256);

  for (token = strtok_r(tempname, "/", &save_ptr); token != NULL;
       token = strtok_r(NULL, "/", &save_ptr))
  {
    prevtoken = token;
  }

  // If it was just a name and not a path, token will be null
  // In that case, a recopy will suffice
  if (prevtoken == NULL)
  {
    token = calloc(1, NAME_MAX + 1);
    strlcpy(token, name, NAME_MAX + 1);
  }
  // Otherwise, we want the last
  else
  {
    token = calloc(1, NAME_MAX + 1);
    strlcpy(token, prevtoken, NAME_MAX + 1);
  }

  ASSERT(dir != NULL);
  ASSERT(token != NULL);

  /* Check NAME for validity. */
  if (*token == '\0' || strlen(token) > NAME_MAX)
  {
    free(tempname);
    free(token);
    return false;
  }

  /* Check that NAME is not in use. */
  if (lookup(dir, token, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.

     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  e.is_dir = false;
  strlcpy(e.name, token, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at(dir->inode, &e, sizeof e, ofs) == sizeof e;

done:
  free(tempname);
  free(token);
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove(struct dir *dir, const char *name)
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  // Change to pathed directory
  dir = dir_get_leaf(name);

  // Strip path off of file name
  char *token, *prevtoken;
  char *tempname = calloc(1, 256);
  char *save_ptr;

  prevtoken = NULL;

  strlcpy(tempname, name, 256);

  for (token = strtok_r(tempname, "/", &save_ptr); token != NULL;
       token = strtok_r(NULL, "/", &save_ptr))
  {
    prevtoken = token;
  }

  // If it was just a name and not a path, token will be null
  // In that case, a recopy will suffice
  if (prevtoken == NULL)
  {
    token = calloc(1, NAME_MAX + 1);
    strlcpy(token, name, NAME_MAX + 1);
  }
  // Otherwise, we want the last
  else
  {
    token = calloc(1, NAME_MAX + 1);
    strlcpy(token, prevtoken, NAME_MAX + 1);
  }

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  /* Find directory entry. */
  if (!lookup(dir, token, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open(e.inode_sector);

  if (inode == NULL)
    goto done;

  /* Erase directory entry. */
  e.in_use = false;
//   printf("Erasing %s \n", name);

  if (inode_write_at(dir->inode, &e, sizeof e, ofs) != sizeof e)
    goto done;

  /* Remove inode. */
  inode_remove(inode);
  success = true;

done:

  inode_close(inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir(struct dir *dir, char *name)
{
  struct dir_entry e;

  //dir = dir_get_leaf(name);

  while (inode_read_at(dir->inode, &e, sizeof e, dir->pos) == sizeof e)
  {
    dir->pos += sizeof e;

    if (e.in_use)
    {
      strlcpy(name, e.name, NAME_MAX + 1);
      return true;
    }
  }

  return false;
}


/* Creates a directory that is not root at the given sector,
   adds the new directory to the current working directory.
   Returns true if successful, false if already exists, bad name, etc */
bool
dir_create(struct dir *dir, const char *name, block_sector_t sector)
{
  if (!is_valid_name(name))
    return 0;

  //printf("dir_create(%s) Tracer 1 \n", name);
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  // Change to pathed directory
  dir = dir_get_leaf(name);

  // Strip path off of file name
  char *token, *prevtoken;
  char *tempname = calloc(1, 256);
  char *save_ptr;

  prevtoken = NULL;

  strlcpy(tempname, name, 256);

  for (token = strtok_r(tempname, "/", &save_ptr); token != NULL;
       token = strtok_r(NULL, "/", &save_ptr))
  {
    prevtoken = token;
  }

  // If it was just a name and not a path, token will be null
  // In that case, a recopy will suffice
  if (prevtoken == NULL)
  {
    token = calloc(1, NAME_MAX + 1);
    strlcpy(token, name, NAME_MAX + 1);
  }
  // Otherwise, we want the last
  else
  {
    token = calloc(1, NAME_MAX + 1);
    strlcpy(token, prevtoken, NAME_MAX + 1);
  }

  //printf("dir_create(%s) Tracer 5 \n", token);

  ASSERT(dir != NULL);
  ASSERT(token != NULL);

  /* Check NAME for validity. */
  if (*token == '\0' || strlen(token) > NAME_MAX)
  {
    free(token);
    free(tempname);
    return false;
  }

  /* Check that NAME is not in use. */
  if (lookup(dir, token, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.

     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  e.is_dir = true;
  strlcpy(e.name, token, sizeof e.name);
  e.inode_sector = sector;
  success = inode_write_at(dir->inode, &e, sizeof e, ofs) == sizeof e;

done:

  if (success)
  {
    struct inode node;
    success = inode_create(sector, BLOCK_SECTOR_SIZE);
    node.data.file_length = 0;
    node.data.prev_length = 0;
    node.data.node_length = 0;
    node.data.self = ptr_create(&node.sector);
    node.data.magic = INODE_MAGIC;
    node.data.doubleptr = NULL;
    block_write(fs_device, sector, &node.data);
  }

  free(token);
  free(tempname);
  return success;
}

/* Change directory */
bool
dir_changedir(const char *name)
{
  // printf("dir_changedir(%s) Tracer 1\n", name);
  // Valid looking name?
  if (!is_valid_name(name))
    return false;

  // printf("dir_changedir(%s) Tracer 2\n", name);
  // Not pre-user threads at this point, get the thread
  struct thread *t = thread_current();

  // Changing to root?
  if (strlen(name) == 1 && name[0] == '/')
  {
    *t->pwd = '/';
    return true;
  }

  char *temp[256];
  strlcpy(temp, name, 256);

  // For get_leaf to work, directories must end in '/'
  if (name[strlen(name) - 1] != '/')
  {
    // printf("dir_changedir(%s) Tracer 1.5 \n", temp);
    strlcat(temp, "/", sizeof(temp));
  }

  // printf("dir_changedir(%s) Tracer 3\n", temp);
  // Is the new directory valid?
  // If it's root it would have already returned, so dir_get_leaf should
  // return not root if valid, root otherwise
  // Keep pwd the same
  if (dir_get_leaf(temp)->inode == dir_open_root()->inode)
  {
    // printf("dir_changedir(%s) Tracer 3.5  leaf: %x  root: %x\n", temp, dir_get_leaf(temp)->inode, dir_open_root()->inode);
    return false;
  }

  // printf("dir_changedir(%s) Tracer 4\n", temp);
  // Absolute path name?
  if (temp[0] == '/')
  {
    *t->pwd = temp;
    // printf("dir_changedir(%s) Tracer 5\n", temp);
  }
  // Relative path name?
  else
  {
    // printf("dir_changedir(%s) Tracer 6\n", temp);
    strlcat(t->pwd, temp, sizeof(t->pwd));
  }

  // printf("dir_changedir(%s) Tracer 7\n", temp);
  return true;
}

/* Go to a child directory from the current directory */
struct dir *
dir_child(struct dir *current, const char *child)
{
  // printf("dir_child(%s) Tracer 1 \n", child);
  // TODO: This calloc is never freed, every dir/file access has a memory leak
  struct dir_entry e;
  struct dir *retdir = calloc(1, sizeof(struct dir));

  ASSERT(current != NULL);
  ASSERT(child != NULL);

  if (lookup(current, child, &e, NULL))
  {
    // printf("dir_child(%s) Tracer 2 \n", child);
    retdir->inode = inode_open(e.inode_sector);
  }
  else
  {
    // printf("dir_child(%s) Tracer 3 \n", child);
    retdir->inode = NULL;
  }

  if (!e.is_dir)
  {
    retdir->inode = NULL;
    // printf("dir_child(%s) Tracer 4 \n", child);
  }

  return retdir;
}

/* Strip string name to leaf directory starting at either cwd or root
   return a struct dir */
struct dir *
dir_get_leaf(const char *name)
{
  // printf("dir_get_leaf(%s) Trace 1 \n", name);
  if (!is_valid_name(name))
    return dir_open_root();

  // printf("dir_get_leaf(%s) Trace 2 \n", name);
  char *tempname = calloc(1, 256 * sizeof(char));
  char *token = calloc(1, 256 * sizeof(char));;
  char *save_ptr;
  struct dir *tmpdir;
  struct dir *lastdir = tmpdir;
  bool enddir;
  struct thread *t = thread_current();
// printf("dir_get_leaf(%s) Trace 3 \n", name);
  strlcpy(tempname, name, strlen(name) + 1);

  if (tempname[0] == '/')       /* Absolute path name */
    tmpdir = dir_open_root();
  else
    tmpdir = dir_get_leaf(t->pwd);

// printf("dir_get_leaf(%s) Trace 4 \n", tempname);
  //strlcpy(&token, t->pwd[1], strlen(t->pwd) + 1);

  if (tempname[strlen(tempname) - 1] == '/')
  {
    enddir = true;
    // printf("dir_get_leaf(%s) Trace 5 \n", tempname);
  }
  else
  {
    enddir = false;
    // printf("dir_get_leaf(%s) Trace 6 \n", tempname);
  }

  for (token = strtok_r(tempname, "/", &save_ptr); token != NULL;
       token = strtok_r(NULL, "/", &save_ptr))
  {
    lastdir = tmpdir;
    // printf("dir_get_leaf(%s) Trace 7, token : %s\n", tempname, token);
    tmpdir = dir_child(tmpdir, token);

    if (tmpdir == NULL)
      break;
  }

  free(tempname);
  free(token);

  // printf("dir_get_leaf(%s) Trace 8 \n", tempname);
  if (enddir)
    return tmpdir;
  else
    return lastdir;
}

/* I need a function to determine if a string appears to be a valid
   directory or file from character set for some of the other functions */
bool
is_valid_name(const char *name)
{
  int i = 0;
  char c = 0;
  bool success = (name != NULL) &&      // Name cannot be NULL
                 (strlen(name) > 0);    // Must have at least 1 character

  while (i < strlen(name) && success)
  {
    c = (char) name[i];
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
