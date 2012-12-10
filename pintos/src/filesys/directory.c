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
  char *abspath = path_abspath(name);
//   // printf("abspath: %s\n", abspath);
  if (strcmp(abspath, name)) {
    abspath = "";
  }

  // // printf("lookup %s\n", name);
  if (!path_isvalid(name))
    return 0;

  struct dir_entry e;
  size_t ofs;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  for (ofs = 0; inode_read_at(dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) {
//     // printf("lookup(%x, %s, %x, %x): Trace 2 \t e.name: %s\n", dir, name, ep, ofsp, e.name);
    if (e.in_use && !strcmp(name, e.name))
    {
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

  // printf("dir_lookup(%x, %s, %x): Trace 1, abspath: %s \n", dir, name, inode, abspath);
  struct dir_entry e;

  // Change to pathed directory
  dir = dir_get_leaf(path_dirname(abspath));
  if (dir == NULL)
    return false;

  // printf("dir_lookup(%x, %s, %x): Trace 1.1, abspath: %s, path_basename(abspath): %s \n", dir, name, inode, abspath, path_basename(abspath));
  if (lookup(dir, path_basename(abspath), &e, NULL))
  {
    // printf("dir_lookup(%x, %s, %x): Trace 2 \t e.inode_sector: %x\n", dir, name, inode, e.inode_sector);
    *inode = inode_open(e.inode_sector);

    if (e.is_dir)
      (*inode)->is_dir = 1;
    else
      (*inode)->is_dir = 0;
  }
  else
    *inode = NULL;

  // printf("dir_lookup(%x, %s, %x): Trace 1.2 EXIT inode: %x\n", dir, name, inode, *inode);
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

  // printf("dir_add(%x, %s, %x) Tracer 1 \t abspath: %s\n", dir, name, inode_sector, abspath);
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  // Change to pathed directory
  dir = dir_get_leaf(abspath);
  if(dir == NULL) {
    // printf("dir_add(%x, %s, %x) Tracer 1.1 EXIT \t abspath: %s\n", dir, name, inode_sector, abspath);
    return false;
  }

  /* Check that NAME is not in use. */
  char *obj_name = path_basename(abspath);
  // printf("dir_add(%x, %s, %x) Tracer 1 \t abspath: %s,, obj_name: %s\n", dir, name, inode_sector, abspath, obj_name);
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
//   free(obj_name);
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove(struct dir *dir, const char *name)
{
  char *abspath = path_abspath(name);
  if (strcmp(abspath, name)) {
    abspath = "";
  }
  // // printf("dir_remove(%s) Tracer 1 \n", name);
  char *toDelete = calloc(1, PATH_MAX * sizeof(name));
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  // Change to pathed directory
  if(is_path(name))
  {
    // We need everything but the last token, so find the last slash
    int index = strlen(name);
    char *pathname = calloc(1, PATH_MAX * sizeof(char));
    
    while(name[index] != '/' && index > -1)
      index--;
    strlcpy(pathname, name, strlen(name));
    pathname[index] = '\0';

    if(strlen(pathname) == 0)
      dir = dir_open_root();
    else
      dir = dir_get_leaf(pathname);
    free(pathname);
    if(dir == NULL)
      return false;

    char *token, *prevtoken;
    char *save_ptr;
    strlcpy(toDelete, name, PATH_MAX);

    for (token = strtok_r(toDelete, "/", &save_ptr); token != NULL;
         token = strtok_r(NULL, "/", &save_ptr))
    {
      prevtoken = token;
    }

    strlcpy(toDelete, prevtoken, PATH_MAX);
  }
  else
    strlcpy(toDelete, name, strlen(name) + 1);

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  /* Find directory entry. */
  if (!lookup(dir, toDelete, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open(e.inode_sector);

  if (inode == NULL)
    goto done;

  /* Erase directory entry. */
  e.in_use = false;
  // // printf("Erasing %s \n", name);

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
  char *abspath = path_abspath(name);
  printf("dir_readdir(%x, %s): Trace 1 \t abspath: %s \n", abspath);
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
  char *abspath = path_abspath(name);
  if (strcmp(abspath, name)) {
    abspath = "";
  }
//   // printf("dir_create(%s) Tracer 1 \n", name);

  char *newdir = calloc(1, PATH_MAX * sizeof(char));
  if (!path_isvalid(name))
    return 0;

//   // printf("dir_create(%s) Tracer 1.1 \n", name);
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  // Change to pathed directory
  if(is_path(name))
  {
    // We need everything but the last token, so find the last slash
    int index = strlen(name);
    char *pathname = calloc(1, PATH_MAX * sizeof(char));
    
    while(name[index] != '/' && index > -1)
      index--;
    strlcpy(pathname, name, strlen(name));
    pathname[index] = '\0';
//     // printf("dir_create(%s) Tracer 2, pathname: \"%s\"  index: %d\n", name, pathname, index);
    if(strlen(pathname) == 0)
      dir = dir_open_root();
    else
      dir = dir_get_leaf(pathname);
    free(pathname);
    if(dir == NULL)
      return false;

    char *token, *prevtoken;
    char *save_ptr;
    strlcpy(newdir, name, PATH_MAX);

    for (token = strtok_r(newdir, "/", &save_ptr); token != NULL;
         token = strtok_r(NULL, "/", &save_ptr))
    {
      prevtoken = token;
    }

    strlcpy(newdir, prevtoken, PATH_MAX);

//    strlcpy(newdir, (char *)name[index], strlen(name) - index + 1);
//     // printf("dir_create(%s) Tracer 3   newdir: \"%s\"   dir: %x\n", name, newdir, dir);
  }
  else
    strlcpy(newdir, name, strlen(name) + 1);

  ASSERT(dir != NULL);
  ASSERT(newdir != NULL);

  /* Check NAME for validity. */
  if (*newdir == '\0' || strlen(newdir) > NAME_MAX)
  {
    free(newdir);
    return false;
  }

  // // printf("dir_create(%s) Tracer 4 \n", name);
  // // printf("Root: %x  currentdir: %x \n", dir_open_root()->inode, dir->inode);

  /* Check that NAME is not in use. */
  if (lookup(dir, newdir, NULL, NULL))
    goto done;

  // // printf("dir_create(%s) Tracer 5 \n", name);

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

//   // printf("dir_create(%s) Tracer 6   newdir: \"%s\"\n", name, newdir);

  /* Write slot. */
  e.in_use = true;
  e.is_dir = true;
  strlcpy(e.name, newdir, sizeof e.name);
  e.inode_sector = sector;
  success = inode_write_at(dir->inode, &e, sizeof e, ofs) == sizeof e;

done:

  if (success)
  {
    success = inode_create(sector, BLOCK_SECTOR_SIZE);
    struct inode *node = inode_open(sector);
    inode_mark_dir(node);
    inode_close(node);
  }

  free(newdir);
  return success;
}

/* Change directory */
bool
dir_changedir(const char *name)
{
  char *abspath = path_abspath(name);
//   // printf("dir_changedir(%s): Trace 1 \t abspath: %s\n", name, abspath);
//   // printf("dir_changedir(%s) Tracer 1\n", name);
  // Valid looking name?
  if (!path_isvalid(name))
    return false;

//   // printf("dir_changedir(%s) Tracer 2\n", name);
  // Not pre-user threads at this point, get the thread
  struct thread *t = thread_current();

  if (path_exists(abspath)) {
//     // printf("dir_changedir(%s) Tracer 2 EXIT\n", abspath);
    strlcpy(&t->pwd[0], abspath, sizeof(t->pwd));
//     // printf("&t->pwd[0]: %s\n", &t->pwd[0]);
    return true;
  }
  else {
//     // printf("dir_changedir(%s) Tracer 3 EXIT\n", abspath);
    return false;
  }
}

/* Go to a child directory from the current directory */
bool
dir_child(struct dir *current, const char *child, struct dir *retdir)
{
//   char *abspath = path_abspath(child);
//   if (strcmp(abspath, child)) {
//     abspath = "";
//   }
//   // printf("dir_child(%s) Tracer 1   current: %x\n", child, current);
  struct dir_entry e;
  retdir = calloc(1, sizeof(struct dir));

  ASSERT(current != NULL);
  ASSERT(child != NULL);

  if (lookup(current, child, &e, NULL))
  {
    retdir->inode = inode_open(e.inode_sector);
    // // printf("dir_child(%s) Tracer 2   inode: %x\n", child, retdir->inode);
  }
  else
  {
    // // printf("dir_child(%s) Tracer 3 \n", child);
    free(retdir);
    retdir = NULL;
    return false;
  }

  if (!e.is_dir)
  {
    free(retdir);
    retdir = NULL;
    return false;
    // // printf("dir_child(%s) Tracer 4 \n", child);
  }

  return true;
}

/* Strip string name to leaf directory starting at either cwd or root
   return a struct dir */
struct dir *
dir_get_leaf(const char *name)
{
  char *abspath = path_abspath(name);
  if (strcmp(abspath, name)) {
    abspath = "";
  }
  // // printf("dir_get_leaf(%s) Trace 1 \n", name);
  if (!path_isvalid(name))
    return NULL;
    
  // If root, will cause other problems, so take care of first
  if (strlen(name) == 1 && name[0] == '/')
  {
    return dir_open_root();
  }
  
  // // printf("dir_get_leaf(%s) Trace 2 \n", name);
  char *tempname = calloc(1, PATH_MAX * sizeof(char));
  char *token = calloc(1, PATH_MAX * sizeof(char));;
  char *save_ptr;
  struct dir *tmpdir;
  struct dir *lastdir = calloc(1, sizeof(struct dir));
  bool enddir;
  struct thread *t = thread_current();
  // // printf("dir_get_leaf(%s) Trace 2.5 \n", name);
  if(!is_path(name))
    return(t->pwd);

// // printf("dir_get_leaf(%s) Trace 3 \n", name);
  strlcpy(tempname, name, strlen(name) + 1);

  if (tempname[0] == '/')       /* Absolute path name */
    tmpdir = dir_open_root();
  else
    tmpdir = dir_get_leaf(t->pwd);

// // printf("dir_get_leaf(%s) Trace 4 \n", tempname);
  //strlcpy(&token, t->pwd[1], strlen(t->pwd) + 1);

  if (tempname[strlen(tempname) - 1] == '/')
  {
    enddir = true;
    // // printf("dir_get_leaf(%s) Trace 5 \n", tempname);
  }
  else
  {
    enddir = false;
    // // printf("dir_get_leaf(%s) Trace 6 \n", tempname);
  }

  for (token = strtok_r(tempname, "/", &save_ptr); token != NULL;
       token = strtok_r(NULL, "/", &save_ptr))
  {
    memcpy(lastdir, tmpdir, sizeof(struct dir));
    free(tmpdir);
    bool success;
    // // printf("dir_get_leaf(%s) Trace 7, token : %s\n", tempname, token);
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

// // printf("dir_get_leaf(%s) Trace 8  Enddir = %d  Lastdir = %x\n", tempname, enddir, lastdir);
// // printf("dir_get_leaf(%s) Trace 9  tmpdir: %x  lastdir: %x\n", tempname, tmpdir->inode, lastdir->inode);
  free(tempname);
  free(token);
  
  if (enddir)
    return lastdir;
  else
    return tmpdir;
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

  strlcpy(&tempname, name, strlen(name) + 1);
  
  // Any slashes?
  if(strlen(strtok_r(tempname, "/", &save_ptr)) == strlen(name))
    success = false;

  return success;
}