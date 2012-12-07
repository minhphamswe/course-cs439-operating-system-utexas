#include "filesys/inode.h"

#include "lib/kernel/list.h"
#include "lib/debug.h"
#include "lib/round.h"
#include "lib/string.h"
#include "lib/stdio.h"

#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include <threads/interrupt.h>

//======[ Global Definitions ]===============================================

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

// A full block of all zeros
static char zeros[BLOCK_SECTOR_SIZE];

//======[ Methods to Set/Query Attributes of inode_ptr ]=====================

// Create a node pointer from a sector number
inode_ptr ptr_create(block_sector_t sector)
{
  return (inode_ptr)(sector & 0x3FFF);
}

// Get disk sector address from inode ptr
block_sector_t ptr_get_address(const inode_ptr *ptr)
{
  ASSERT(ptr != NULL);
  return (block_sector_t)((*ptr) & 0x3FFF);
}

// Register the pointer as having data on disk
void ptr_set_exist(inode_ptr *ptr)
{
  ASSERT(ptr != NULL);
  (*ptr) = (*ptr) | 0x8000;
}

// Return true if the meta data says the pointer exists on disk
bool ptr_exists(const inode_ptr *ptr)
{
  ASSERT(ptr != NULL);
  return ((*ptr) & 0x8000);
}

// Set the pointer as being a directory file
void set_is_dir(inode_ptr *ptr)
{
  ASSERT(ptr != NULL);
  (*ptr) = (*ptr) | 0x4000;
}

// Return true if the meta data says the pointer is a directory file
bool inode_is_dir(const inode_ptr *ptr)
{
  ASSERT(ptr != NULL);
  return ((*ptr) & 0x4000);
}

//======[ Methods to Set/Query Attributes of inode ]=========================

// Allocate a single new inode, along with its data. If ON_DISK is true, also
// allocate space on disk for the inode.
// Return the struct inode pointer if successful, NULL otherwise
struct inode *allocate_inode(bool on_disk)
{
  // printf("allocate_inode(): Trace 1\n");
  bool success = true;
  block_sector_t data_addr = 0;
  struct inode *node = NULL;

  if (on_disk && (!free_map_allocate(1, &data_addr)))
  {
    // Out of disk space
    // printf("allocate_inode(): Trace 3 EXIT\tOut of disk space - return %d\n", NULL);
    success = false;
  }

  if (success)
  {
    // Either memory allocation on disk succeeded, or was unnecessary
    // Allocate space on disk for a struct inode
    // printf("allocate_inode(): Trace 3.1 \t \t\t\t sizeof(struct inode): %d\n", sizeof(struct inode));
    node = malloc(sizeof * node);

    if (node == NULL)
    {
      // Out of memory
      success = false;
    }
    else
    {
      // So far so good
      // Set up attributes in the node
      node->sector = data_addr;       // Sector number of disk location.
      node->open_cnt = 0;             // Number of openers.
      node->removed = false;          // True if deleted, false otherwise.
      node->deny_write_cnt = false;   // 0: writes ok, >0: deny writes.
      node->next = NULL;              // Pointer to next inode
      sema_init(&node->extend_sema, 1);

      // Set attributes for the inode disk
      node->data.file_length = 0;       // File length in bytes
      node->data.prev_length = 0;       // Combined length of previous inodes
      node->data.node_length = 0;       // Number of bytes used in this node
      node->data.doubleptr = 0;
      node->data.magic = INODE_MAGIC;   // Magic number

      // printf("allocate_inode(): Trace 3.1 \t \t\t\t sizeof(node->data): %d\n", sizeof(node->data));
      ASSERT(sizeof(node->data) == BLOCK_SECTOR_SIZE);
    }
  }

  // printf("allocate_inode(): Trace 4 EXIT\treturn %x\n", node);
  return node;
}

block_sector_t get_next_addr(struct inode *node)
{
  ASSERT(node != NULL);
  inode_ptr ptr = node->data.doubleptr;

  if (ptr_exists(&ptr))
  {
    return ptr_get_address(&ptr);
  }
  else
  {
    return -1;
  }
}


/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors(off_t size)
{
  return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE);
}

// Return the inode that contains byte offset POS within INODE.
// Return NULL there is no such inode.
static struct inode *
byte_to_inode(struct inode *inode, off_t pos)
{
  if (pos == -1)
    return inode;

  // printf("byte_to_inode(%x, %d): Trace 1\n", inode, pos);
//   ASSERT(inode != NULL);

  // printf("byte_to_inode(%x, %d): Trace 1.1 \t pos: %d, inode->data.file_length: %d, inode->data.prev_length: %d, inode->data.node_length: %d\n", inode, pos, pos, inode->data.file_length, inode->data.prev_length, inode->data.node_length);

  if (inode != NULL && pos < inode->data.file_length)
  {
    // printf("byte_to_inode(%x, %d): Trace 2\tpos: %d, inode->data.file_length: %d, inode->data.prev_length: %d, inode->data.node_length: %d\n", inode, pos, pos, inode->data.file_length, inode->data.prev_length, inode->data.node_length);
    // If this check failed, we passed POS at some point -> doubly-linked list?
    ASSERT(pos >= inode->data.prev_length);

    // printf("byte_to_inode(%x, %d): Trace 2.5\n", inode, pos);
    // Calculate the local byte offset position that would correspond to POS
    off_t local_pos = pos - inode->data.prev_length;

    if (local_pos < BYTE_CAPACITY)
    {
      // This is one of ours
      // printf("byte_to_inode(%x, %d): Trace 3 EXIT \t return %x ONE OF OURS\n", inode, pos, inode);
      return inode;
    }
    else
    {
      // Not ours. Go bother the next guy.
      if (inode->next != NULL)
      {
        ASSERT(inode->next != inode);
        // printf("byte_to_inode(%x, %d): Trace 4.1 EXIT \t return byte_to_inode(%x, %d)\n", inode, pos, inode->next, pos);
        return byte_to_inode(inode->next, pos);
      }
      else
        if (ptr_exists(&inode->data.doubleptr))
        {
          ASSERT(inode->sector != ptr_get_address(&inode->data.doubleptr));
          inode->next = inode_open(ptr_get_address(&inode->data.doubleptr));
          // printf("byte_to_inode(%x, %d): Trace 4.1 EXIT \t return byte_to_inode(%x, %d)\n", inode, pos, inode->next, pos);
          return byte_to_inode(inode->next, pos);
        }
        else
        {
          // printf("byte_to_inode(%x, %d): Trace 4.2 EXIT \t return NULL\n", inode, pos);
          return NULL;
        }
    }
  }
  else
  {
    // printf("byte_to_inode(%x, %d): Trace 5 EXIT return NULL\n", inode, pos);
    return NULL;
  }
}

/* Returns the block device sector that contains byte offset POS within INODE.
   Returns -1 if INODE does not contain data for a byte at offset POS. */
static block_sector_t
byte_to_sector(const struct inode *inode, off_t pos)
{
  // printf("byte_to_sector(%x, %d): Trace 1\n", inode, pos);
//   ASSERT(inode != NULL);

  struct inode *node = byte_to_inode(inode, pos);

  if (node != NULL)
  {
    // printf("byte_to_sector(%x, %d): Trace 2\n", inode, pos);

    // Calculate the local byte offset position that would correspond to POS
    off_t local_pos = pos - node->data.prev_length;
    // printf("byte_to_sector(%x, %d): Trace 3.1 \t local_pos: %d\n", inode, pos, local_pos);

    inode_ptr addr = node->data.blockptrs[local_pos / BLOCK_SECTOR_SIZE];
    // printf("byte_to_sector(%x, %d): Trace 3.2 \t local_pos: %d, addr: %x\n", inode, pos, local_pos, addr);

    if (ptr_exists(&addr))
    {
      // printf("byte_to_sector(%x, %d): Trace 4.1 EXIT return %d\n", inode, pos, ptr_get_address(&addr));
      return ptr_get_address(&addr);
    }
    else
    {
      // printf("byte_to_sector(%x, %d): Trace 4.2 EXIT return -1\n", inode, pos);
      return -1;
    }
  }
  else
  {
    // printf("byte_to_sector(%x, %d): Trace 5 EXIT return -1\n", inode, pos);
    return -1;
  }
}

static struct semaphore closing_sema;
static struct semaphore extend_sema;

/* Initializes the inode module. */
void
inode_init(void)
{
  list_init(&open_inodes);
  sema_init(&closing_sema, 1);
  sema_init(&extend_sema, 1);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create(block_sector_t sector, off_t length)
{
  // printf("inode_create(%d, %d): Trace 1\n", sector, length);
  ASSERT(length >= 0);

  // Declare parameters to be used in the following loops
  off_t bytes_left = length;  // Number of bytes left to allocate
  size_t sector_idx;          // Index of next sector to allocate in current node
  block_sector_t data_addr;   // Disk address of a data sector

  // The main inode
  struct inode *main_node = allocate_inode(false);
  main_node->sector = sector;
  main_node->data.file_length = length;

  // The last node to be allocated
  struct inode *tail_node = inode_extend_link(main_node, length, length);

  // Check if any problem occurred
  if (tail_node == NULL)
  {
    // Free all data blocks
    // Free main_node
    free(main_node);
    return false;
  }

  // No problem occurred: write MAIN_NODE to SECTOR
  block_write(fs_device, sector, &main_node->data);
  return true;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open(block_sector_t sector)
{
  // printf("inode_open(%x): Trace 1\n", sector);
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin(&open_inodes); e != list_end(&open_inodes);
       e = list_next(e))
  {
    inode = list_entry(e, struct inode, elem);

    if (inode->sector == sector)
    {
      inode_reopen(inode);
      // printf("inode_open(%x): Trace 2 EXIT\treturn: %x\n", sector, inode);
      return inode;
    }
  }

  /* Allocate memory. */
  inode = allocate_inode(false);

  if (inode == NULL)
  {
    // printf("inode_open(%x): Trace 3 EXIT\treturn: %x\n", sector, inode);
    return NULL;
  }

  /* Initialize. */
  enum intr_level old_level = intr_disable();
  list_push_front(&open_inodes, &inode->elem);
  intr_set_level(old_level);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  inode->next = NULL;
  block_read(fs_device, inode->sector, &inode->data);

  // printf("inode_open(%x): Trace 4 EXIT\treturn %x, inode->data.file_length: %d, inode->data.node_length: %d\n", sector, inode, inode->data.file_length, inode->data.node_length);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen(struct inode *inode)
{
  // printf("inode_reopen(%x): Trace 1\n", inode);

  if (inode != NULL)
    inode->open_cnt++;

  // printf("inode_reopen(%x): Trace 2 EXIT\n", inode);
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber(const struct inode *inode)
{
  // printf("inode_get_inumber(%x): Trace 1\n", inode);
  ASSERT(inode != NULL);
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close(struct inode *inode)
{
  // printf("inode_close(%x): Trace 1\n", inode);

  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  ASSERT(inode->next != inode);

  // printf("inode_close(%x): Trace 2 \t inode->open_cnt: %d  Double ptr: %x, inode->sector: %x\n", inode, inode->open_cnt, inode->data.doubleptr, inode->sector);

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
  {
    // printf("inode_close(%x): Trace 3 \t inode->open_cnt: %d, inode->sector: %d Releasing file.\n", inode, inode->open_cnt, inode->sector);
    /* Remove from inode list and release lock. */
    enum intr_level old_level = intr_disable();
    list_remove(&inode->elem);
    intr_set_level(old_level);

    sema_down(&closing_sema);

    /* Deallocate blocks if removed. */
    if (inode->removed)
    {
      // TODO, Free data for entire file
      free_map_release(inode->sector, 1);
    }

    sema_up(&closing_sema);

    if (inode->next != NULL && inode->next != inode)
      inode_close(inode->next);

    free(inode);
  }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove(struct inode *inode)
{
  // printf("inode_remove(%x): Trace 1\n", inode);
  ASSERT(inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  // printf("inode_read_at(%x, %x, %d, %d): Trace 1\n", inode, buffer_, size, offset);
  ASSERT(inode != NULL);
  ASSERT(buffer_ != NULL);

  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  // printf("inode_read_at(%x, %x, %d, %d): Trace 2\tsize: %d\n", inode, buffer_, size, offset, size);

  while (size > 0)
  {
    // printf("inode_read_at(%x, %x, %d, %d): Trace 3\tsize: %d\n", inode, buffer_, size, offset, size);

    /* Disk sector to read, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    // printf("inode_read_at(%x, %x, %d, %d): Trace 4.1 \t sector_idx: %d, sector_ofs: %d\n", inode, buffer_, size, offset, sector_idx, sector_ofs);

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;
    // printf("inode_read_at(%x, %x, %d, %d): Trace 4.2 \t inode_left: %d, sector_left: %d\n", inode, buffer_, size, offset, inode_left, sector_left);

    /* Number of bytes to actually copy out of this sector. */
    int chunk_size = size < min_left ? size : min_left;

    // printf("inode_read_at(%x, %x, %d, %d): Trace 4.3 \t min_left: %d, chunk_size: %d\n", inode, buffer_, size, offset, min_left, chunk_size);

    if (chunk_size <= 0)
      break;

    if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
    {
      /* Read full sector directly into caller's buffer. */
      block_read(fs_device, sector_idx, buffer + bytes_read);
    }
    else
    {
      /* Read sector into bounce buffer, then partially copy
         into caller's buffer. */
      if (bounce == NULL)
      {
        bounce = malloc(BLOCK_SECTOR_SIZE);

        if (bounce == NULL)
          break;
      }

      block_read(fs_device, sector_idx, bounce);
      memcpy(buffer + bytes_read, bounce + sector_ofs, chunk_size);
    }

    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_read += chunk_size;
  }

  free(bounce);

  // printf("inode_read_at(%x, %x, %d, %d): Trace 5 EXIT\treturn: %d\n", inode, buffer_, size, offset, bytes_read);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if an error occurs.nn */
off_t
inode_write_at(struct inode *inode, const void *buffer_, off_t size,
               off_t offset)
{
//   printf("inode_write_at(%x, %x, %d, %d): Trace 1\n", inode, buffer_, size, offset);
  ASSERT(inode != NULL);
  ASSERT(buffer_ != NULL);
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  // printf("inode_write_at(%x, %x, %d, %d): Trace 2\n", inode, buffer_, size, offset);

  if (inode->deny_write_cnt)
  {
    // printf("inode_write_at(%x, %x, %d, %d): Trace 3\n", inode, buffer_, size, offset);
    return 0;
  }

  // printf("inode_write_at(%x, %x, %d, %d): Trace 4 \t size: %d, offset: %d, size + offset: %d\n", inode, buffer_, size, offset, size, offset, size + offset);

//   if (size + offset > inode->data.file_length)
//     inode_extend(inode, size + offset);

  if (size + offset > inode->data.file_length)
  {
    uint32_t bytes_left = (size + offset) - inode->data.file_length;
//     printf("inode_write_at(%x, %x, %d, %d): Trace 4 \t bytes_left: %u\n", inode, buffer_, size, offset, bytes_left);
    struct inode *tail_node = inode_extend_link(inode, bytes_left, size + offset);

    if (tail_node == NULL)
    {
      return 0;
    }
  }

  // printf("inode_write_at(%x, %x, %d, %d): Trace 5\n", inode, buffer_, size, offset);

  while (size > 0)
  {
    // printf("inode_write_at(%x, %x, %d, %d): Trace 5.1\n", inode, buffer_, size, offset);
    /* Sector to write, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    // printf("inode_write_at(%x, %x, %d, %d): Trace 5.2 \t sector_idx: %d, sector_ofs: %d\n", inode, buffer_, size, offset, sector_idx, sector_ofs);
    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually write into this sector. */
    int chunk_size = size < min_left ? size : min_left;

    // printf("inode_write_at(%x, %x, %d, %d): Trace 5.3 \t inode_left: %d, sector_left: %d\n", inode, buffer_, size, offset, inode_left, sector_left);
    // printf("inode_write_at(%x, %x, %d, %d): Trace 5.4 \t min_left: %d, chunk_size: %d\n", inode, buffer_, size, offset, min_left, chunk_size);

    if (chunk_size <= 0)
    {
      // printf("inode_write_at(%x, %x, %d, %d): Trace 6\n", inode, buffer_, size, offset);
      break;
    }

    if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
    {
      // printf("inode_write_at(%x, %x, %d, %d): Trace 7 \t sector_idx: %d, buffer + bytes_written: %x \n", inode, buffer_, size, offset, sector_idx, buffer + bytes_written);
      /* Write full sector directly to disk. */
      block_write(fs_device, sector_idx, buffer + bytes_written);
    }
    else
    {
      /* We need a bounce buffer. */
      // printf("inode_write_at(%x, %x, %d, %d): Trace 8\n", inode, buffer_, size, offset);

      if (bounce == NULL)
      {
        // printf("inode_write_at(%x, %x, %d, %d): Trace 9\n", inode, buffer_, size, offset);
        bounce = malloc(BLOCK_SECTOR_SIZE);

        if (bounce == NULL)
        {
          // printf("inode_write_at(%x, %x, %d, %d): Trace 10\n", inode, buffer_, size, offset);
          break;
        }
      }

      /* If the sector contains data before or after the chunk
         we're writing, then we need to read in the sector
         first.  Otherwise we start with a sector of all zeros. */
      if (sector_ofs > 0 || chunk_size < sector_left)
      {
        // printf("inode_write_at(%x, %x, %d, %d): Trace 11\n", inode, buffer_, size, offset);
        block_read(fs_device, sector_idx, bounce);
      }
      else
      {
        // printf("inode_write_at(%x, %x, %d, %d): Trace 12\n", inode, buffer_, size, offset);
        memset(bounce, 0, BLOCK_SECTOR_SIZE);
      }

      // printf("inode_write_at(%x, %x, %d, %d): Trace 13\n", inode, buffer_, size, offset);
      memcpy(bounce + sector_ofs, buffer + bytes_written, chunk_size);
      block_write(fs_device, sector_idx, bounce);
    }

    // printf("inode_write_at(%x, %x, %d, %d): Trace 14\n", inode, buffer_, size, offset);
    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_written += chunk_size;
  }

  free(bounce);
  // printf("inode_write_at(%x, %x, %d, %d): Trace 15 EXIT\n", inode, buffer_, size, offset);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write(struct inode *inode)
{
  // printf("inode_deny_write(%x): Trace 1\n", inode);
  inode->deny_write_cnt++;
  ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write(struct inode *inode)
{
  // printf("inode_allow_write(%x): Trace 1\n", inode);
  ASSERT(inode->deny_write_cnt > 0);
  ASSERT(inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length(const struct inode *inode)
{
  // printf("inode_length(%x): Trace 1\n", inode);
  // printf("inode_length(%x): Trace 2 EXIT\treturn %d\n", inode, inode->data.file_length);
  return inode->data.file_length;
}

// Extend an inode by BYTES_LEFT.
// Returns the last node allocated (i.e. the tail) if successful. NULL otherwise
struct inode *
inode_extend_link(struct inode *prev_node, uint32_t bytes_left, uint32_t length)
{
//   printf("inode_extend_link(%x, %u): Trace 1\n", prev_node, bytes_left);
  ASSERT(prev_node != NULL);

  // Check if something went wrong trying to fill up PREV_NODE
  if (!inode_fill(prev_node, &bytes_left, length))
  {
//     printf("inode_extend_link(%x, %u): Trace 2 \t EXIT return NULL\n", prev_node, bytes_left);
    return NULL;
  }

//   printf("inode_extend_link(%x, %u): Trace 2.5\n", prev_node, bytes_left);

  // Check if there's nothing more to allocate
  if (bytes_left <= 0)
  {
//     printf("inode_extend_link(%x, %u): Trace 3 \t EXIT return %x\n", prev_node, bytes_left, prev_node);
    return prev_node;
  }

//   printf("inode_extend_link(%x, %u): Trace 3.5\n", prev_node, bytes_left);

  // Attempt to allocate a new node
  struct inode *current_node = allocate_inode(true);

  // Check if allocation failed
  if (current_node == NULL)
  {
//     printf("inode_extend_link(%x, %u): Trace 4 \t EXIT return NULL\n", prev_node, bytes_left);
    return NULL;
  }

//   printf("inode_extend_link(%x, %u): Trace 4.5\n", prev_node, bytes_left);
  // Successfully allocated disk space for subsequent INODE
  // Quick check against circularity
  ASSERT(current_node != prev_node);
  //ASSERT(current_node->sector != ptr_get_address(&prev_node->data.doubleptr));

  // Check if there's any problem allocating data to fill up this node
  if (!inode_fill(current_node, &bytes_left, length))
  {
    // Free all allocated pages

    // Free the current node, return NULL
    free(current_node);

//     printf("inode_extend_link(%x, %u): Trace 5 \t EXIT return NULL\n", prev_node, bytes_left);
    return NULL;
  }

//   printf("inode_extend_link(%x, %u): Trace 5.1\n", prev_node, bytes_left);

  // Successfully filled up this node
//   sema_down(&current_node->extend_sema);

  // Link the previous node to the current node
  if (prev_node != NULL)
  {
    prev_node->next = current_node;
    prev_node->data.doubleptr = ptr_create(current_node->sector);
    ptr_set_exist(&prev_node->data.doubleptr);
  }

//   printf("inode_extend_link(%x, %u): Trace 5.2\n", prev_node, bytes_left);

  // Continue extending and linking
  struct inode *node = inode_extend_link(current_node, bytes_left, length);

  // Check if something came unravelled down the line
  if (node == NULL)
  {
    // Free allocated data blocks

    // Free inode
    free(current_node);

//     printf("inode_extend_link(%x, %u): Trace 6 \t EXIT return NULL\n", prev_node, bytes_left);
    return NULL;
  }

//   printf("inode_extend_link(%x, %u): Trace 6.1\n", prev_node, bytes_left);

  // Everything is OK: finalize by writing to disk and return
  // Update lengths of this node
  current_node->data.file_length = prev_node->data.file_length;
  block_write(fs_device, prev_node->sector, &prev_node->data);
  block_write(fs_device, current_node->sector, &current_node->data);
//   sema_up(&current_node->extend_sema);

//   printf("inode_extend_link(%x, %u): Trace 7 EXIT \t return %x\n", prev_node, bytes_left, current_node);
  return current_node;
}

// Fill up a single inode.
bool
inode_fill(struct inode *current_node, uint32_t *bytes_left, uint32_t length)
{
//   printf("inode_fill(%x, %x, %d): Trace 1 \t current_node->data.file_length: %u, *bytes_left: %u\n", current_node, bytes_left, length, current_node->data.file_length, *bytes_left);
  ASSERT(current_node != NULL);
  ASSERT(bytes_left != NULL);
  // Calculate the number of sectors this node will contain
  uint32_t sector_idx = DIV_ROUND_UP(current_node->data.node_length,
                                     BLOCK_SECTOR_SIZE);

  // Hold disk sector address of the data blocks to be allocated
  block_sector_t data_addr;

  sema_down(&current_node->extend_sema);

  // Allocate space for the data blocks
  while ((sector_idx < NODE_CAPACITY) && (*bytes_left > 0))
  {
    if (!free_map_allocate(1, &data_addr))
    {
      // Free all allocated pages ??
//       printf("inode_fill(%x, %x, %d): Trace 2 EXIT \t current_node->data.file_length: %u, *bytes_left: %u\n", current_node, bytes_left, length, current_node->data.file_length, *bytes_left);
      sema_up(&current_node->extend_sema);
      return false;
    }
    else
    {
      // Successfully allocated disk space for DATA node
      // Write block of all zero to data sector
      block_write(fs_device, data_addr, &zeros);

      // Set the metadata for the DATA sector to existing
      current_node->data.blockptrs[sector_idx] = ptr_create(data_addr);
      ptr_set_exist(&current_node->data.blockptrs[sector_idx]);

      // Calculate the difference between the new size and the old size
      uint32_t diff = (sector_idx + 1) * BLOCK_SECTOR_SIZE - current_node->data.node_length;

      // Update used capacity of the current node
      if (*bytes_left < diff)
      {
        current_node->data.node_length += *bytes_left;
        *bytes_left = 0;
      }
      else
      {
        current_node->data.node_length += diff;
        *bytes_left -= diff;
      }

      // Update loop parameters
      sector_idx++;
    }
  }

  current_node->data.file_length = length;
  block_write(fs_device, current_node->sector, &current_node->data);
//   printf("inode_fill(%x, %x, %d): Trace 3 EXIT \t current_node->data.file_length: %u, *bytes_left: %u\n", current_node, bytes_left, length, current_node->data.file_length, *bytes_left);
  sema_up(&current_node->extend_sema);
  return true;
}