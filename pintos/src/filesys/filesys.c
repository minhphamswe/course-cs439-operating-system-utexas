#include "filesys/filesys.h"
#include "lib/debug.h"
#include "lib/stdio.h"
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  // Name is no longer implicitly checked on length below, must check
  // at creation of call
  
  if(strlen(name) > NAME_MAX)
    return 0;
  
  block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_root ();
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Creates a directory named NAME.
   Returns true if successful, false otherwise.
   Fails if a file/dir named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create_dir (const char *name) 
{
  block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_root ();
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, BLOCK_SECTOR_SIZE)
                  && dir_create (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;
  if (dir != NULL)
    dir_lookup (dir, name, &inode);

  dir_close (dir);
    return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = dir_open_root ();
  bool success = dir != NULL && dir_remove (dir, name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create_root (ROOT_DIR_SECTOR))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}


/* Changes the current working directory of the process to dir
   which may be relative or absolute. Returns true if successful,
   false on failure. */
struct dir *filesys_chdir(const char *dirname)
{

}

/* Creates the directory named dir, which may be relative or absolute.
   Returns true if successful, false on failure. Fails if dir already
   exists or if any directory name in dir, besides the last, does not
   already exist. That is, mkdir("/a/b/c") succeeds only if "/a/b"
   already exists and "/a/b/c" does not. */
struct dir *filesys_mkdir(const char *dirname)
{
//   struct inode *inode = inode_create (sector, 1 * sizeof (struct dir_entry));
//     set_is_dir(&inode->data.this);
//     return inode;
//   return inode_create(sector, entry_cnt *sizeof(struct dir_entry));
}
