#include "filesys/filesys.h"

#include "lib/debug.h"
#include "lib/stdio.h"
#include "lib/string.h"

#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/path.h"

#include "threads/thread.h"
#include "threads/synch.h"

/* Partition that contains the file system. */
struct block *fs_device;

//======[ Global Definitions ]===============================================
static struct semaphore open_sema;

static void do_format(void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init(bool format)
{
  fs_device = block_get_role(BLOCK_FILESYS);

  if (fs_device == NULL)
    PANIC("No file system device found, can't initialize file system.");

  inode_init();
  free_map_init();

  if (format)
    do_format();

  free_map_open();

  // Initialize semaphore(s)
  sema_init(&open_sema, 1);
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done(void)
{
  free_map_close();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create(const char *name, off_t initial_size)
{
  char* abspath = path_abspath(name);
  // printf("filesys_create(%s, %d): Trace 1 \t name: %s, abspath: %s\n", name, initial_size, name, abspath);
  // Name is no longer implicitly checked on length below, must check
  // at creation of call

  if (strlen(name) > NAME_MAX)
    return 0;

  block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_root();
  bool success = (dir != NULL
                  && free_map_allocate(1, &inode_sector)
                  && inode_create(inode_sector, initial_size)
                  && dir_add(dir, name, inode_sector));

  if (!success && inode_sector != 0)
    free_map_release(inode_sector, 1);

  dir_close(dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open(const char *name)
{
  char* abspath = path_abspath(name);
  // printf("filesys_open(%s): Trace 1 \t name: %s, abspath: %s\n", name, name, abspath);
  
//   printf("filesys_open(%s): Trace 1\n", name);
//   printf("filesys_open(%s): Trace 2 \t thread_current()->pwd: %s\n", name, thread_current()->pwd);

  struct dir *dir = dir_open_root();
  struct inode *inode = NULL;

  printf("filesys_open(%s): Trace 2.1 \t dir: %x, dir->inode: %x\n", name, dir, dir->inode);
  sema_down(&open_sema);
  if (dir != NULL) {
    dir_lookup(dir, name, &inode);
//     printf("filesys_open(%s): Trace 2.1 \t inode: %x\n", name, inode);
  }

  dir_close(dir);
//   printf("filesys_open(%s): Trace 2 EXIT \t return %x\n", name, inode);
  sema_up(&open_sema);
  return file_open(inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove(const char *name)
{
  char* abspath = path_abspath(name);
  // printf("filesys_remove(%s): Trace 1 \t name: %s, abspath: %s\n", name, name, abspath);
  struct dir *dir = dir_open_root();
  bool success = dir != NULL && dir_remove(dir, name);
  dir_close(dir);

  return success;
}

/* Formats the file system. */
static void
do_format(void)
{
  printf("Formatting file system...");
  free_map_create();

  if (!dir_create_root(ROOT_DIR_SECTOR))
    PANIC("root directory creation failed");

  free_map_close();
  printf("done.\n");
}


/* Changes the current working directory of the process to dir
   which may be relative or absolute. Returns true if successful,
   false on failure. */
bool filesys_chdir(const char *dirname)
{

}


/* Creates a directory named NAME.
   Returns true if successful, false otherwise.
   Fails if a file/dir named NAME already exists,
   or if internal memory allocation fails,
   or if any directory name in dir, besides the last, does not
   already exist.
   That is, mkdir("/a/b/c") succeeds only if "/a/b" already exists and
   "/a/b/c" does not.
   */
bool
filesys_mkdir(const char *name)
{
  char* abspath = path_abspath(name);
  // printf("filesys_mkdir(%s): Trace 1 \t name: %s, abspath: %s\n", name, name, abspath);
  block_sector_t inode_sector = 0;
  struct dir *dir = dir_open_root();
  bool success = (dir != NULL
                  && free_map_allocate(1, &inode_sector)
                  && inode_create(inode_sector, BLOCK_SECTOR_SIZE)
                  && dir_create(dir, name, inode_sector));

  if (!success && inode_sector != 0)
    free_map_release(inode_sector, 1);

  dir_close(dir);

  return success;
}

/**
 * Open a directory from a given path.
 * Return a pointer to the newly open directory if successful,
 * NULL otherwise.
 */
struct dir*
filesys_opendir(const char* path)
{
  return NULL;
}
