#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "lib/kernel/list.h"

#include "filesys/off_t.h"
#include "devices/block.h"
#include "threads/synch.h"

struct bitmap;

//======[ #define Macros ]===================================================

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Number of sectors per inode */
#define NODE_CAPACITY 244

/* Number of bytes stored per inode (= NODE_CAPACITY * BLOCK_SECTOR_SIZE) */
#define BYTE_CAPACITY (BLOCK_SECTOR_SIZE * NODE_CAPACITY)

typedef uint16_t inode_ptr;

//======[ Struct Definitions ]===============================================

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
{
  off_t file_length;                    // File length in bytes
  off_t prev_length;                    // Combined length of previous inodes
  off_t node_length;                    // Number of bytes used in this node
  inode_ptr self;                       // Pointer to myself, for reference
  uint32_t magic;                       // Magic number
  inode_ptr doubleptr;                  // Pointer to next indirect inode
  inode_ptr blockptrs[NODE_CAPACITY];   // Index of data pointers
};

/* In-memory inode. */
struct inode
{
  struct list_elem elem;              // Element in inode list.
  block_sector_t sector;              // Sector number of disk location.
  int open_cnt;                       // Number of openers.
  bool removed;                       // True if deleted, false otherwise
  int deny_write_cnt;                 // 0: writes ok, >0: deny writes.
  struct semaphore extend_sema;
  bool is_dir;                        // 0 is file, 1 is directory

  struct inode_disk data;             // Inode content.
  struct inode* next;                 // The subsequent inode struct pointer
};

//======[ Forward Declarations ]=============================================

//==========[ OLD: inode functions ]=========================================

void inode_init(void);
bool inode_create(block_sector_t, off_t);
struct inode *inode_open(block_sector_t);
struct inode *inode_reopen(struct inode *);
block_sector_t inode_get_inumber(const struct inode *);
void inode_close(struct inode *);
void inode_remove(struct inode *);
off_t inode_read_at(struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at(struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write(struct inode *);
void inode_allow_write(struct inode *);
off_t inode_length(const struct inode *);

//==========[ NEW: inode_ptr functions ]=====================================

// Create a node pointer from a sector number
inode_ptr ptr_create(block_sector_t sector);
// Get disk sector address from inode ptr
block_sector_t ptr_get_address(const inode_ptr *ptr);
// Register the pointer as having data on disk
void ptr_set_exist(inode_ptr *ptr);
// Return true if the meta data says the pointer exists on disk
bool ptr_exists(const inode_ptr *ptr);
// Set the pointer as being a directory file
void ptr_set_isdir(inode_ptr *ptr);
// Return true if the meta data says the pointer is a directory file
bool ptr_isdir(const inode_ptr *ptr);

//==========[ NEW: inode functions ]=========================================

// Allocate a single new inode, along with its data. If ON_DISK is true, also
// allocate space on disk for the inode.
// Return the struct inode pointer if successful, NULL otherwise
struct inode *allocate_inode(bool on_disk);
// Return the disk address of the next inode (if it exists, otherwise -1)
block_sector_t get_next_addr(struct inode *node);

bool inode_fill(struct inode *current_node, uint32_t *bytes_left, uint32_t length);
struct inode* inode_extend_link(struct inode *prev_node, uint32_t bytes_left, uint32_t length);


#endif /* filesys/inode.h */
