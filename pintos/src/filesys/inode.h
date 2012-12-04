#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "lib/kernel/list.h"

#include "filesys/off_t.h"
#include "devices/block.h"

struct bitmap;

//======[ #define Macros ]===================================================

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Number of sectors per inode */
#define NODE_CAPACITY 246

/* Number of bytes stored per inode (= NODE_CAPACITY * BLOCK_SECTOR_SIZE) */
#define BYTE_CAPACITY 125952

typedef uint16_t inode_ptr;

//======[ Struct Definitions ]===============================================

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
{
  off_t file_length;                    // File length in bytes
  off_t prev_length;                    // Combined length of previous inodes
  off_t node_length;                    // Number of bytes used in this node
  inode_ptr this;                       // Pointer to myself, for reference
  unsigned magic;                       // Magic number
  inode_ptr blockptrs[NODE_CAPACITY];   // Index of data pointers
  inode_ptr doubleptr;                  // Pointer to next indirect inode
};

/* In-memory inode. */
struct inode
{
  struct list_elem elem;              // Element in inode list.
  block_sector_t sector;              // Sector number of disk location.
  int open_cnt;                       // Number of openers.
  bool removed;                       // True if deleted, false otherwise
  int deny_write_cnt;                 // 0: writes ok, >0: deny writes.

  struct inode_disk data;             // Inode content.
  struct inode* next;                 // The subsequent inode struct pointer
};

void inode_init (void);
bool inode_create (block_sector_t, off_t);
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

void set_is_dir(inode_ptr *ptr);
bool inode_is_dir(inode_ptr *ptr);


#endif /* filesys/inode.h */
