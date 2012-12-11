#include "filesys/directory.h"

#include "lib/stdio.h"
#include "lib/string.h"
#include "lib/kernel/list.h"
#include "lib/debug.h"

#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/path.h"

#include "threads/malloc.h"
#include "threads/thread.h"

int total_opens;

// Function to check if a string has bad characters for a directory
bool is_path(const char *name);

/* Creates the root directory at the sector given.
   Returns true if successful, false on failure. */
bool
dir_create_root(block_sector_t sector)
{
  bool success = false;

  // Create and write an inode to the sector
  success = inode_create(sector, BLOCK_SECTOR_SIZE);

  // Set the inode's metadata to say it's a directory
  if (success)
  {
    struct inode *node = inode_open(sector);
    inode_mark_dir(node);
    block_write(fs_device, node->sector, &node->data);
    inode_close(node);
  }

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
bool
lookup(const struct dir *dir, const char *name,
       struct dir_entry *ep, off_t *ofsp)
{
  if (!path_isvalid(name)) {
    return 0;
  }

  struct dir_entry e;
  size_t ofs;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  struct inode *node = dir->inode;

  for (ofs = 0; inode_read_at(node, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) {
    if (e.in_use && !strcmp(name, e.name)) {
      if (ep != NULL)
        *ep = e;

      if (ofsp != NULL)
        *ofsp = ofs;

      return true;
    }
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
  char *abspath = path_abspath(name);
  ASSERT(abspath != NULL);
  char *basename = path_basename(abspath);
  ASSERT(basename != NULL);
  char *dirname = path_dirname(abspath);
  ASSERT(dirname != NULL);

  struct dir_entry e;
  struct dir *foo;

  // Change to pathed directory
  foo = dir_get_leaf(dirname);
  if (foo == NULL) {
    free(abspath);
    return false;
  }

  if (lookup(foo, basename, &e, NULL))
  {
    *inode = inode_open(e.inode_sector);

    if (e.is_dir)
      (*inode)->is_dir = 1;
    else
      (*inode)->is_dir = 0;
  }
  else
    *inode = NULL;

  dir_close(foo);
  free(abspath);
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
  char *abspath = path_abspath(name);
  ASSERT(abspath != NULL);
  char *dirname = path_dirname(abspath);
  ASSERT(dirname != NULL);
  char *basename = path_basename(abspath);
  ASSERT(basename != NULL);

  struct dir_entry e;
  off_t ofs;
  bool success = false;

  // Change to pathed directory
  dir = dir_get_leaf(dirname);
  if(dir == NULL) {
    free(abspath);
    return false;
  }

  /* Check that NAME is not in use. */
  char *obj_name = basename;
  if (lookup(dir, obj_name, NULL, NULL))
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
  strlcpy(e.name, obj_name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at(dir->inode, &e, sizeof e, ofs) == sizeof e;

done:
  dir_close(dir);
  free(abspath);
  free(dirname);
  free(basename);
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove(struct dir *dir, const char *name)
{
  char toDelete[PATH_MAX];
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;
  struct dir *foo = NULL;

  // Change to pathed directory
  if(is_path(name)) {
    // We need everything but the last token, so find the last slash
    int index = strlen(name);
    char pathname[PATH_MAX];

    while(name[index] != '/' && index > -1)
      index--;
    strlcpy(pathname, name, strlen(name));
    pathname[index] = '\0';

    if(strlen(pathname) == 0)
      foo = dir_open_root();
    else
      foo = dir_get_leaf(pathname);

    if(foo == NULL)
      return false;

    char *token = NULL, *prevtoken = NULL, *save_ptr = NULL;
    strlcpy(toDelete, name, PATH_MAX);

    for (token = strtok_r(toDelete, "/", &save_ptr); token != NULL;
         token = strtok_r(NULL, "/", &save_ptr))
    {
      prevtoken = token;
    }

    strlcpy(toDelete, prevtoken, PATH_MAX);
  }
  else {
    strlcpy(toDelete, name, strlen(name) + 1);
    foo = dir_open(inode_open(dir->inode->sector));
  }

  ASSERT(foo != NULL);
  ASSERT(name != NULL);

  /* Find directory entry. */
  if (!lookup(foo, toDelete, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open(e.inode_sector);

  if (inode == NULL)
    goto done;

  // Can't remove open directories. Files, though, are fair game
  if (inode_is_dir(inode) && inode->open_cnt > 1) {
    goto done;
  }

  /* Erase directory entry. */
  e.in_use = false;

  if (inode_write_at(foo->inode, &e, sizeof e, ofs) != sizeof e)
    goto done;

  /* Remove inode. */
  inode_remove(inode);
  success = true;

done:

  dir_close(foo);
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
  char *abspath = path_abspath(name);
  ASSERT(abspath != NULL);

  char *dirname = path_dirname(abspath);
  ASSERT(dirname != NULL);

  char *basename = path_basename(abspath);
  ASSERT(basename != NULL);

  struct dir_entry e;
  off_t ofs;
  bool success = false;
  struct dir *foo = NULL;

  // Change to pathed directory
  foo = dir_get_leaf(dirname);
  if (foo == NULL) {
    free(abspath);
    return false;
  }

  /* Check that NAME is not in use. */
  char *newdir = basename;
  if (lookup(foo, newdir, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.

     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at(foo->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  e.is_dir = true;
  strlcpy(e.name, newdir, sizeof e.name);
  e.inode_sector = sector;
  success = inode_write_at(foo->inode, &e, sizeof e, ofs) == sizeof e;

done:

  if (success)
  {
    success = inode_create(sector, BLOCK_SECTOR_SIZE);
    struct inode *node = inode_open(sector);

    inode_mark_dir(node);
    block_write(fs_device, node->sector, &node->data);
    inode_close(node);
  }

  free(abspath);
  free(dirname);
  free(basename);

  dir_close(foo);
  return success;
}

/* Change directory */
bool
dir_changedir(const char *name)
{
  char *abspath = path_abspath(name);
  ASSERT(abspath != NULL);
  // Valid looking name?
  if (!path_isvalid(name))
    return false;

  // Not pre-user threads at this point, get the thread
  struct thread *t = thread_current();

  if (path_exists(abspath)) {
    strlcpy(&t->pwd[0], abspath, sizeof(t->pwd));
    free(abspath);
    return true;
  }
  else {
    free(abspath);
    return false;
  }
}

/* Go to a child directory from the current directory */
bool
dir_child(struct dir *current, const char *child, struct dir *retdir)
{
  struct dir_entry e;
  retdir = calloc(1, sizeof(struct dir));

  ASSERT(current != NULL);
  ASSERT(child != NULL);

  if (lookup(current, child, &e, NULL)) {
    retdir->inode = inode_open(e.inode_sector);
  }
  else {
    retdir = NULL;
    return false;
  }

  if (!e.is_dir) {
    retdir = NULL;
    return false;
  }

  return true;
}

/* Strip string name to leaf directory starting at either cwd or root
   return a struct dir */
struct dir *
dir_get_leaf(const char *name)
{
total_opens++;
  if (!path_isvalid(name))
    return NULL;

  // If root, will cause other problems, so take care of first
  if (strlen(name) == 1 && name[0] == '/')
  {
    return dir_open_root();
  }

  char *tempname[PATH_MAX];
  char *token, *save_ptr;
  struct dir *tmpdir;
  struct dir *lastdir = calloc(1, sizeof(struct dir));
  bool enddir;
  struct thread *t = thread_current();

  strlcpy(tempname, name, strlen(name) + 1);

  if (tempname[strlen(tempname) - 1] == '/')
  {
    enddir = true;
  }
  else
  {
    enddir = false;
  }
 
  tmpdir = dir_open_root();
  lastdir = dir_open(inode_open(tmpdir->inode->sector));
  for (token = strtok_r(tempname, "/", &save_ptr); token != NULL;
       token = strtok_r(NULL, "/", &save_ptr))
  {
    dir_close(lastdir);
    lastdir = dir_open(inode_open(tmpdir->inode->sector));
    dir_close(tmpdir);
    bool success;
    success = dir_child(lastdir, token, tmpdir);

    if (!success)
    {
      if(strcmp(token, name))
      {
        // File name only, return last directory
        return lastdir;
      }
      else
      {
        // Looked for a directory that does not exist
        return NULL;
      }
    }
  }

  if (enddir) {
    dir_close(tmpdir);
    return lastdir;
  }
  else {
    dir_close(lastdir);
    return tmpdir;
  }
}

/* Is name a path or just a file/dir node name */
bool
is_path(const char *name)
{
  char tempname[PATH_MAX];
  char *save_ptr;
  bool success = true;
  
  if(!path_isvalid(name))
    success = false;

  strlcpy(&tempname[0], name, strlen(name) + 1);
  
  // Any slashes?
  if(strlen(strtok_r(tempname, "/", &save_ptr)) == strlen(name))
    success = false;

  return success;
}

/* Checks DIR to see if it is a parent or not (can't remove parents)
   Returns 1 if directory is empty, 0 otherwise */
bool
dir_is_empty(const char *name)
{
  if (!path_isvalid(name)) {
    return 0;
  }

  char *abspath = path_abspath(name);
  ASSERT(abspath != NULL);

  struct dir *dir = dir_get_leaf(abspath);

  struct dir_entry e;
  size_t ofs;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  for (ofs = 0; inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
  {
    if (e.in_use)
    {
      dir_close(dir);
      free(abspath);
      return false;
    }
  }

  dir_close(dir);
  free(abspath);
  return true;
}
