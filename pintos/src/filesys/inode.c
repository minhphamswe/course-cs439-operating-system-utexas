#include "filesys/inode.h"

#include "lib/kernel/list.h"
#include "lib/debug.h"
#include "lib/round.h"
#include "lib/string.h"
#include "lib/stdio.h"

#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

//======[ #define Macros ]===================================================

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* Number of sectors per inode */
#define NODE_CAPACITY 246

/* Number of bytes stored per inode (= NODE_CAPACITY * BLOCK_SECTOR_SIZE) */
#define BYTE_CAPACITY 125952

//======[ Global Definitions ]===============================================

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

// A full block of all zeros
static char zeros[BLOCK_SECTOR_SIZE];

//======[ Struct Definitions ]===============================================

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
{
  off_t file_length;                    // File length in bytes
  off_t prev_length;                    // Combined length of previous inodes
  off_t node_length;                    // Number of bytes used in this node
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
};

//======[ Forward Declarations ]=============================================
// Create a node pointer from a sector number
inode_ptr ptr_create(block_sector_t sector);
// Get disk sector address from inode ptr
block_sector_t ptr_get_address(const inode_ptr *ptr);
// Register the pointer as having data on disk
void ptr_set_exist(inode_ptr *ptr);
// Return true if the meta data says the pointer exists on disk
bool ptr_exists(const inode_ptr *ptr);

// Allocate a single new inode, along with its data.
// Return the struct inode pointer if successful, NULL otherwise
struct inode *allocate_inode(void);
// Return the disk address of the next inode (if it exists, otherwise -1)
block_sector_t get_next_addr(struct inode *node);
// Return the inode that contains byte offset POS within INODE.
// Return NULL there is no such inode.
static struct inode* byte_to_inode(const struct inode *inode, off_t pos);

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
  return ((*ptr) >> 15);
}

//======[ Methods to Set/Query Attributes of inode ]=========================

// Allocate a single new inode, along with its data.
// Return the struct inode pointer if successful, NULL otherwise
// FIXME: this allocates memory for inode_disk twice
struct inode *allocate_inode()
{
  printf("allocate_inode(): Trace 1\n");
  struct inode_disk *data = calloc(1, sizeof(struct inode_disk));

  if (data == NULL)
  {
    // Out of memory: return NULL
    printf("allocate_inode(): Trace 2 EXIT\tOut of memory - return %d\n", NULL);
    return NULL;
  }
  else
  {
    // Allocate space on disk to back the data of a struct inode_disk
    block_sector_t data_addr = 0;

    if (!free_map_allocate(1, &data_addr))
    {
      // Out of disk space: return NULL
      printf("allocate_inode(): Trace 3 EXIT\tOut of disk space - return %d\n", NULL);
      return NULL;
    }
    else
    {
      // Allocate space on disk for a struct inode
      struct inode *node = malloc(sizeof(struct inode));

      if (node == NULL)
      {
        // Out of memory: return NULL
        printf("allocate_inode(): Trace 4 EXIT\tOut of memory - return %d\n", NULL);
        return NULL;
      }
      else
      {
        // So far so good
        // Set attributes for the inode disk
        data->file_length = 0;          // File length in bytes
        data->prev_length = 0;          // Combined length of previous inodes
        data->node_length = 0;          // Number of bytes used in this node
        data->magic = INODE_MAGIC;      // Magic number

        // Set up attributes in the node
        node->sector = data_addr;       // Sector number of disk location.
        node->open_cnt = 0;             // Number of openers.
        node->removed = false;          // True if deleted, false otherwise.
        node->deny_write_cnt = false;   // 0: writes ok, >0: deny writes.
        node->data = *data;             // Inode content.

        // And return
        printf("allocate_inode(): Trace 5 EXIT\treturn %x\n", node);
        return node;
      }
    }
  }
}

block_sector_t get_next_addr(struct inode *node)
{
  ASSERT(node != NULL);
  inode_ptr ptr = node->data.doubleptr;
  if (ptr_exists(&ptr)) {
    return ptr_get_address(&ptr);
  }
  else {
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
static struct inode*
byte_to_inode(const struct inode *inode, off_t pos)
{
  printf("byte_to_inode(%x, %d): Trace 1\n", inode, pos);
  ASSERT(inode != NULL);

  printf("byte_to_inode(%x, %d): Trace 1.5\tpos: %d, inode->data.file_length: %d, inode->data.prev_length: %d\n", inode, pos, pos, inode->data.file_length, inode->data.prev_length);

  if (pos < inode->data.file_length)
  {
    printf("byte_to_inode(%x, %d): Trace 2\n", inode, pos);
    // If this check failed, we passed POS at some point -> doubly-linked list?
    ASSERT(pos >= inode->data.prev_length);

    printf("byte_to_inode(%x, %d): Trace 2.5\n", inode, pos);
    // Calculate the local byte offset position that would correspond to POS
    off_t local_pos = pos - inode->data.prev_length;

    struct inode *node;
    if (local_pos < BYTE_CAPACITY)
    { // This is one of ours
      return inode;
    }
    else
    { // Not ours. Go bother the next guy.
      node = inode_open(ptr_get_address(&inode->data.doubleptr));
      return byte_to_inode(node, pos);
    }
  }
  else
  {
    printf("byte_to_inode(%x, %d): Trace 5 EXIT return NULL\n", inode, pos);
    return NULL;
  }
}

/* Returns the block device sector that contains byte offset POS within INODE.
   Returns -1 if INODE does not contain data for a byte at offset POS. */
static block_sector_t
byte_to_sector(const struct inode *inode, off_t pos)
{
  printf("byte_to_sector(%x, %d): Trace 1\n", inode, pos);
  ASSERT(inode != NULL);

  struct inode *node = byte_to_inode(inode, pos);

  if (node != NULL)
  {
    printf("byte_to_sector(%x, %d): Trace 2\n", inode, pos);

    // Calculate the local byte offset position that would correspond to POS
    off_t local_pos = pos - inode->data.prev_length;

    inode_ptr addr;
    addr = inode->data.blockptrs[local_pos/BLOCK_SECTOR_SIZE];
    printf("byte_to_sector(%x, %d): Trace 4 EXIT return %d\n", inode, pos, ptr_get_address(&addr));
    return ptr_get_address(&addr);
  }
  else
  {
    printf("byte_to_sector(%x, %d): Trace 5 EXIT return -1\n", inode, pos);
    return -1;
  }
}

/* Initializes the inode module. */
void
inode_init(void)
{
  list_init(&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create(block_sector_t sector, off_t length)
{
  printf("inode_create(%d, %d): Trace 1\n", sector, length);
  ASSERT(length >= 0);

  // Declare parameters to be used in the following loops
  off_t bytes_left = length;  // Number of bytes left to allocate
  size_t next_sector;         // Next sector to allocate in current node
  block_sector_t data_addr;   // Disk address of a data sector

  struct inode *main_node = NULL;         // The main inode
  struct inode *prev_node = NULL;         // The previous inode
  struct inode *current_node;             // The current inode

  printf("inode_create(%d, %d): Trace 4\tmain_node: %x\n", sector, length, main_node);

  // Create as many inodes as we'll need to hold the bytes
  while (bytes_left > 0)
  {
    printf("inode_create(%d, %d): Trace 5\n", sector, length);
    current_node = allocate_inode();

    if (current_node == NULL)
    { // Failed to allocate a subsequent INODE: free all allocated pages
      printf("inode_create(%d, %d): Trace 6 EXIT return %d\n", sector, length, false);
      if (main_node)
        free(main_node);
      return false;
    }
    else
    { // Successfully allocated disk space for subsequent INODE
      printf("inode_create(%d, %d): Trace 7\n", sector, length);
      // Assign main_node if it's not already there
      if (main_node == NULL)
        main_node = current_node;
      printf("inode_create(%d, %d): Trace 7.5\tmain_node: %x\n", sector, length, main_node);

      // Update used file length of this node
      current_node->data.prev_length = length - bytes_left;
      current_node->data.file_length = length;
      
      // Calculate the number of sectors this node will contain
      next_sector = 0;

      printf("inode_create(%d, %d): Trace 8\tnext_sector: %d\n", sector, length, next_sector);

      // Allocate space for the data blocks
      while ((next_sector < NODE_CAPACITY) && (bytes_left > 0))
      {
        printf("inode_create(%d, %d): Trace 9\tnext_sector: %d\n", sector, length, next_sector);

        if (!free_map_allocate(1, &data_addr))
        {
          printf("inode_create(%d, %d): Trace 10 EXIT return %d\n", sector, length, false);
          // Free all allocated pages
          free(main_node);
          return false;
        }
        else
        { // Successfully allocated disk space for DATA node
          printf("inode_create(%d, %d): Trace 11\n", sector, length);

          // Write block of all zero to data sector
          block_write(fs_device, data_addr, &zeros);

          // Set the metadata for the DATA sector to existing
          current_node->data.blockptrs[next_sector] = ptr_create(data_addr);
          ptr_set_exist(&current_node->data.blockptrs[next_sector]);

          // Update used capacity of the current node
          if (bytes_left < BYTE_CAPACITY) {
            current_node->data.node_length += bytes_left;
            bytes_left = 0;
          }
          else {
            current_node->data.node_length += BYTE_CAPACITY;
            bytes_left -= BYTE_CAPACITY;
          }

          // Update loop parameters
          next_sector++;
        }
      }

      printf("inode_create(%d, %d): Trace 12\n", sector, length);

      // Write inode data to corresponding disk sector
      if (current_node != main_node)
        block_write(fs_device, current_node->sector, &current_node->data);

      // Link the previous node to the current node
      if (prev_node != NULL) {
        prev_node->data.doubleptr = ptr_create(current_node->sector);
        ptr_set_exist(&prev_node->data.doubleptr);
      }

      // Update loop parameters
      prev_node = current_node;
    }
  }

  printf("inode_create(%d, %d): Trace 13, main_node: %x\n", sector, length, main_node);
  // Everything OK: Write main inode to disk, and reset its sector member
  block_write(fs_device, sector, &main_node->data);
//   main_node->sector = sector;

  printf("inode_create(%d, %d): Trace 14 EXIT return %d\n", sector, length, true);
  return true;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open(block_sector_t sector)
{
  printf("inode_open(%d): Trace 1\n", sector);
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
      printf("inode_open(%d): Trace 2 EXIT\treturn: %x\n", sector, inode);
      return inode;
    }
  }

  /* Allocate memory. */
  inode = malloc(sizeof *inode);

  if (inode == NULL)
  {
    printf("inode_open(%d): Trace 3 EXIT\treturn: %x\n", sector, inode);
    return NULL;
  }

  /* Initialize. */
  list_push_front(&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read(fs_device, inode->sector, &inode->data);

  printf("inode_open(%d): Trace 4 EXIT\treturn %x, inode->data.file_length: %d\n", sector, inode, inode->data.file_length);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen(struct inode *inode)
{
  printf("inode_reopen(%x): Trace 1\n", inode);

  if (inode != NULL)
    inode->open_cnt++;

  printf("inode_reopen(%x): Trace 2 EXIT\n", inode);
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber(const struct inode *inode)
{
  printf("inode_get_inumber(%x): Trace 1\n", inode);
  ASSERT(inode != NULL);
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close(struct inode *inode)
{
  printf("inode_close(%x): Trace 1\n", inode);

  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  // Close subsequent inodes as well
  

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
  {
    /* Remove from inode list and release lock. */
    list_remove(&inode->elem);

    /* Deallocate blocks if removed. */
    if (inode->removed)
    {
      block_sector_t current_addr = inode->sector;
      inode_ptr next = inode->data.doubleptr;

      while (ptr_exists(&next))
      {
        free_map_release(current_addr, 1);

        current_addr = ptr_get_address(&next);
        block_read(fs_device, current_addr, &inode->data);
        next = inode->data.doubleptr;
      }
    }

    free(inode);
  }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove(struct inode *inode)
{
  printf("inode_remove(%x): Trace 1\n", inode);
  ASSERT(inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  printf("inode_read_at(%x, %x, %d, %d): Trace 1\n", inode, buffer_, size, offset);
  ASSERT(inode != NULL);
  ASSERT(buffer_ != NULL);

  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  printf("inode_read_at(%x, %x, %d, %d): Trace 2\tsize: %d\n", inode, buffer_, size, offset, size);
  while (size > 0)
  {
    printf("inode_read_at(%x, %x, %d, %d): Trace 3\tsize: %d\n", inode, buffer_, size, offset, size);
    /* Disk sector to read, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;
    printf("inode_read_at(%x, %x, %d, %d): Trace 4\tinode_left: %d, sector_left: %d\n", inode, buffer_, size, offset, inode_left, sector_left);
//     int min_left = inode->data.node_length;

    /* Number of bytes to actually copy out of this sector. */
    int chunk_size = size < min_left ? size : min_left;

    printf("inode_read_at(%x, %x, %d, %d): Trace 4\tmin_left: %d, chunk_size: %d\n", inode, buffer_, size, offset, min_left, chunk_size);
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

  printf("inode_read_at(%x, %x, %d, %d): Trace 2 EXIT\treturn: %d\n", inode, buffer_, size, offset, bytes_read);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at(struct inode *inode, const void *buffer_, off_t size,
               off_t offset)
{
  printf("inode_write_at(%x, %x, %d, %d): Trace 1\n", inode, buffer_, size, offset);
  ASSERT(inode != NULL);
  ASSERT(buffer_ != NULL);
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  printf("inode_write_at(%x, %x, %d, %d): Trace 2\n", inode, buffer_, size, offset);

  if (inode->deny_write_cnt)
  {
    printf("inode_write_at(%x, %x, %d, %d): Trace 3\n", inode, buffer_, size, offset);
    return 0;
  }

  printf("inode_write_at(%x, %x, %d, %d): Trace 4\n", inode, buffer_, size, offset);

  while (size > 0)
  {
    printf("inode_write_at(%x, %x, %d, %d): Trace 5\n", inode, buffer_, size, offset);
    /* Sector to write, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    printf("inode_write_at(%x, %x, %d, %d): Trace 5.2\n", inode, buffer_, size, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;
    printf("inode_write_at(%x, %x, %d, %d): Trace 5.5\n", inode, buffer_, size, offset);
    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually write into this sector. */
    int chunk_size = size < min_left ? size : min_left;

    printf("inode_write_at(%x, %x, %d, %d): Trace 5.9\n", inode, buffer_, size, offset);

    if (chunk_size <= 0)
    {
      printf("inode_write_at(%x, %x, %d, %d): Trace 6\n", inode, buffer_, size, offset);
      break;
    }

    if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
    {
      printf("inode_write_at(%x, %x, %d, %d): Trace 7\n", inode, buffer_, size, offset);
      /* Write full sector directly to disk. */
      block_write(fs_device, sector_idx, buffer + bytes_written);
    }
    else
    {
      /* We need a bounce buffer. */
      printf("inode_write_at(%x, %x, %d, %d): Trace 8\n", inode, buffer_, size, offset);

      if (bounce == NULL)
      {
        printf("inode_write_at(%x, %x, %d, %d): Trace 9\n", inode, buffer_, size, offset);
        bounce = malloc(BLOCK_SECTOR_SIZE);

        if (bounce == NULL)
        {
          printf("inode_write_at(%x, %x, %d, %d): Trace 10\n", inode, buffer_, size, offset);
          break;
        }
      }

      /* If the sector contains data before or after the chunk
         we're writing, then we need to read in the sector
         first.  Otherwise we start with a sector of all zeros. */
      if (sector_ofs > 0 || chunk_size < sector_left)
      {
        printf("inode_write_at(%x, %x, %d, %d): Trace 11\n", inode, buffer_, size, offset);
        block_read(fs_device, sector_idx, bounce);
      }
      else
      {
        printf("inode_write_at(%x, %x, %d, %d): Trace 12\n", inode, buffer_, size, offset);
        memset(bounce, 0, BLOCK_SECTOR_SIZE);
      }

      printf("inode_write_at(%x, %x, %d, %d): Trace 13\n", inode, buffer_, size, offset);
      memcpy(bounce + sector_ofs, buffer + bytes_written, chunk_size);
      block_write(fs_device, sector_idx, bounce);
    }

    printf("inode_write_at(%x, %x, %d, %d): Trace 14\n", inode, buffer_, size, offset);
    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_written += chunk_size;
  }

  free(bounce);
  printf("inode_write_at(%x, %x, %d, %d): Trace 15 EXIT\n", inode, buffer_, size, offset);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write(struct inode *inode)
{
  printf("inode_deny_write(%x): Trace 1\n", inode);
  inode->deny_write_cnt++;
  ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write(struct inode *inode)
{
  printf("inode_allow_write(%x): Trace 1\n", inode);
  ASSERT(inode->deny_write_cnt > 0);
  ASSERT(inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length(const struct inode *inode)
{
  printf("inode_length(%x): Trace 1\n", inode);
  printf("inode_length(%x): Trace 2 EXIT\treturn %d\n", inode, inode->data.file_length);
  return inode->data.file_length;
}
