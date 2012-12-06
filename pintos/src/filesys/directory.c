#include "filesys/directory.h"
#include "lib/stdio.h"
#include "lib/string.h"
#include "lib/kernel/list.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

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
  node.data.this = &node;
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
//  dir = dir_get_leaf(dir);
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
  struct dir_entry e;

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  if (lookup(dir, name, &e, NULL))
    *inode = inode_open(e.inode_sector);
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

 // dir = dir_get_leaf(dir);

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen(name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup(dir, name, NULL, NULL))
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
  strlcpy(e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at(dir->inode, &e, sizeof e, ofs) == sizeof e;

done:
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

 // dir = dir_get_leaf(dir);

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  /* Find directory entry. */
  if (!lookup(dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open(e.inode_sector);

  if (inode == NULL)
    goto done;

  /* Erase directory entry. */
  e.in_use = false;

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
  
 // dir = dir_get_leaf(dir);

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
  struct dir_entry e;
  off_t ofs;
  bool success = false;

 // dir = dir_get_leaf(dir);

  ASSERT(dir != NULL);
  ASSERT(name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen(name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup(dir, name, NULL, NULL))
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
  strlcpy(e.name, name, sizeof e.name);
  e.inode_sector = sector;
  success = inode_write_at(dir->inode, &e, sizeof e, ofs) == sizeof e;

  done:

  if(success)
  {
    struct inode node;
    success = inode_create(sector, BLOCK_SECTOR_SIZE);
    node.data.file_length = 0;
    node.data.prev_length = 0;
    node.data.node_length = 0;
    node.data.this = &node;
    node.data.magic = INODE_MAGIC;
    node.data.doubleptr = NULL;
    block_write(fs_device, sector, &node.data);
  }
  return success;
}

/* Go to a child directory from the current directory */
struct dir *
dir_child(struct dir *current, const char *child)
{
  struct dir_entry e;
  struct dir *retdir;

  ASSERT(current != NULL);
  ASSERT(child != NULL);

  if (lookup(current, child, &e, NULL))
    retdir->inode = inode_open(e.inode_sector);
  else
    retdir->inode = NULL;
    
  if (!e.is_dir)
    retdir->inode = NULL;

  return retdir;
}

/* Strip string name to leaf directory starting at either cwd or root
   return a struct dir */
struct dir *
dir_get_leaf(const char *name)
{
  if(!is_thread(running_thread())
    return dir_open_root();
  char *tempname = calloc(1, 256 * sizeof(char)); 
  char *token = calloc(1, 256 * sizeof(char));;
  char *save_ptr;
  struct dir *tmpdir;
  struct dir *lastdir = tmpdir;
  bool enddir;
  struct thread *t = thread_current();

  if(name == NULL)
    return dir_open_root();
    
  strlcpy(tempname, name, strlen(name));
  
  if(tempname[0] == '/')        /* Absolute path name */
    tmpdir = dir_open_root();
    
//  strlcpy(&token, t->pwd[1], strlen(t->pwd) + 1);
    
  if(tempname[strlen(tempname)] == '/')
    enddir = true;
  else
    enddir = false;

  for (token = strtok_r (tempname, '/', &save_ptr); token != NULL;
       token = strtok_r (NULL, '/', &save_ptr)) {
    lastdir = tmpdir;
    tmpdir = dir_child(tmpdir, token);
    if(tmpdir == NULL)
      break;
  }

  if (enddir)
    return tmpdir;
  else
    return lastdir;
}
