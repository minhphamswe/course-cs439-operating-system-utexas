#ifndef FILESYS_DIRECTORY_H
#define FILESYS_DIRECTORY_H

#include <stdbool.h>
#include <stddef.h>
#include "devices/block.h"
#include "filesys/off_t.h"

/* Maximum length of a file name component.
   This is the traditional UNIX maximum length.
   After directories are implemented, this maximum length may be
   retained, but much longer full path names must be allowed. */
#define NAME_MAX 14
#define PATH_MAX 256

//======[ Struct Definitions ]===============================================

struct inode;

/* A directory. */
struct dir 
{
  struct inode *inode;                /* Backing store. */
  off_t pos;                          /* Current position. */
};

/* A single directory entry. */
struct dir_entry 
{
  block_sector_t inode_sector;        /* Sector number of header. */
  char name[NAME_MAX + 1];            /* Null terminated file name. */
  bool in_use;                        /* In use or free? */
  bool is_dir;                        /* Is a directory? */
};

//======[ Forward Declarations ]=============================================

/* Opening and closing directories. */
bool dir_create_root (block_sector_t sector);
bool dir_create(struct dir *dir, const char *name, block_sector_t sector);
struct dir *dir_open (struct inode *);
struct dir *dir_open_root (void);
struct dir *dir_reopen (struct dir *);
void dir_close (struct dir *);
struct inode *dir_get_inode (struct dir *);
struct dir *dir_get_leaf(const char *name);
bool dir_changedir(const char *name);

/* Reading and writing. */
bool dir_lookup (const struct dir *, const char *name, struct inode **);
bool dir_add (struct dir *, const char *name, block_sector_t);
bool dir_remove (struct dir *, const char *name);
bool dir_readdir (struct dir *, char name[NAME_MAX + 1]);
char* dir_getcwd(void);

#endif /* filesys/directory.h */
