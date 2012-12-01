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
#define NODE_CAPACITY 250

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
  off_t length;                         /* File size in bytes. */
  unsigned magic;                       /* Magic number. */
  inode_ptr blockptrs[NODE_CAPACITY];   /* Index of data pointers */
  inode_ptr doubleptr;                  /* Pointer to next indirect inode */
};

/* In-memory inode. */
struct inode
{
  struct list_elem elem;              /* Element in inode list. */
  block_sector_t sector;              /* Sector number of disk location. */
  int open_cnt;                       /* Number of openers. */
  bool removed;                       /* True if deleted, false otherwise. */
  int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
  struct inode_disk *data;            /* Inode content. */
};

//======[ Forward Declarations ]=============================================
inode_ptr ptr_create(block_sector_t sector);
block_sector_t ptr_get_address(inode_ptr *ptr);
void ptr_set_exist(inode_ptr *ptr);
bool ptr_exists(inode_ptr *ptr);

struct inode_disk* allocate_inode_disk(void);
struct inode* allocate_inode(void);

//======[ Methods to Set/Query Attributes of inode_ptr ]=====================

inode_ptr ptr_create(block_sector_t sector)
{
  return (inode_ptr)(sector & 0x3FFF);
}

// Get disk sector address from inode ptr
block_sector_t ptr_get_address(inode_ptr *ptr)
{
  ASSERT(ptr != NULL);
  return (block_sector_t)((*ptr) & 0x3FFF);
}

void ptr_set_exist(inode_ptr *ptr)
{
  ASSERT(ptr != NULL);
  (*ptr) = (*ptr) | 0x8000;
}

// Whether or not the meta data says the pointer exists on disk
bool ptr_exists(inode_ptr *ptr)
{
  ASSERT(ptr != NULL);
  return ((*ptr) >> 15);
}

//======[ Methods to Set/Query Attributes of inode ]=========================

/// Allocate a single new sector for an inode_disk
/// Return the struct inode_disk pointer if successful, NULL otherwise
struct inode_disk* allocate_inode_disk() {
  // Allocate space in memory for a struct inode_disk
  struct inode_disk *data = calloc(1, sizeof(struct inode_disk));
  if (data == NULL) {
    // Out of memory: return NULL
    return NULL;
  }
  else {
    // Allocate space on disk to back the data of a struct inode_disk
    block_sector_t data_addr = 0;
    if (!free_map_allocate(1, &data_addr)) {
      // Out of disk space: return NULL
      return NULL;
    }
    else {
      // Set attributes for the inode disk and return
      data->length = BLOCK_SECTOR_SIZE;
      data->magic = INODE_MAGIC;

      return data;
    }
  }
}


/// Allocate a single new inode, along with its data.
/// Return the struct inode pointer if successful, NULL otherwise
struct inode* allocate_inode() {
  struct inode_disk *data = calloc(1, sizeof(struct inode_disk));
  if (data == NULL) {
    // Out of memory: return NULL
    return NULL;
  }
  else {
    // Allocate space on disk to back the data of a struct inode_disk
    block_sector_t data_addr = 0;
    if (!free_map_allocate(1, &data_addr)) {
      // Out of disk space: return NULL
      return NULL;
    }
    else {
      // Allocate space on disk for a struct inode
      struct inode *node = malloc(sizeof(struct inode));
      if (node == NULL) {
        // Out of memory: return NULL
        return NULL;
      }
      else {
        // So far so good: set up attributes in the node, and return
        node->sector = data_addr;       // Sector number of disk location.
        node->open_cnt = 0;             // Number of openers.
        node->removed = false;          // True if deleted, false otherwise.
        node->deny_write_cnt = false;   // 0: writes ok, >0: deny writes.
        node->data = data;              // Inode content.

        return node;
      }
    }
  } 
}

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors(off_t size)
{
  return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE);
}

/* Returns the block device sector that contains byte offset POS within INODE.
   Returns -1 if INODE does not contain data for a byte at offset POS. */
static block_sector_t
byte_to_sector(const struct inode *inode, off_t pos)
{
  ASSERT(inode != NULL);

  if (pos < inode->data->length)
  {
    uint16_t next_addr = inode->sector;
    struct inode_disk *data = inode->data;

    while (pos / BLOCK_SECTOR_SIZE > NODE_CAPACITY)
    {
      next_addr = ptr_get_address(&data->doubleptr);
      block_read(fs_device, next_addr, data);
      pos -= BLOCK_SECTOR_SIZE * NODE_CAPACITY;
    }

    next_addr = data->blockptrs[pos / BLOCK_SECTOR_SIZE];
    return (block_sector_t) ptr_get_address(&next_addr);
  }
  else
  {
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

  // Structure we will write to disk for main inode
  struct inode *main_node = allocate_inode();

  if (main_node == NULL)
  {
    printf("inode_create(%d, %d): Trace 2\n", sector, length);
    // Failed to allocate space for the MAIN inode
    return false;
  }
  else
  {
    printf("inode_create(%d, %d): Trace 3\n", sector, length);
    // Successfully allocated disk space for the MAIN inode
    struct inode_disk *main_data = main_node->data;

    // Check the inode structure is exactly one sector in size
    ASSERT(main_data != NULL);
    ASSERT(sizeof *main_data == BLOCK_SECTOR_SIZE);

    // Number of DATA sectors to allocate overall
    size_t num_sectors_total = bytes_to_sectors(length);

    // The next sector to allocate in the current node
    size_t next_sector;

    // Numner of sectors to allocate in the current node
    size_t num_sectors_in_node;

    // Disk address of a data sector
    block_sector_t data_addr;

    // The previous inode
    struct inode *prev_node = main_node;
    struct inode *current_node;
    struct inode_disk *current_data;

    printf("inode_create(%d, %d): Trace 4\tnum_sectors_total:%d\n", sector, length, num_sectors_total);
    while (num_sectors_total > 0)
    {
      printf("inode_create(%d, %d): Trace 5\tnum_sectors_total:%d\n", sector, length, num_sectors_total);
      current_node = allocate_inode();

      if (current_node == NULL)
      {
        printf("inode_create(%d, %d): Trace 6\n", sector, length);
        // Failed to allocate a subsequent INODE: free all allocated pages
        free(main_data);
        return false;
      }
      else
      {
        printf("inode_create(%d, %d): Trace 7\n", sector, length);
        // Successfully allocated disk space for subsequent INODE
        // Link the previous node to the current node
        prev_node->data->doubleptr = ptr_create(current_node->sector);
        ptr_set_exist(&prev_node->data->doubleptr);

        // Get the data of the current node
        current_data = current_node->data;
        ASSERT(current_data != NULL);

        // Calculate the number of sectors this node will contain
        next_sector = 0;
        num_sectors_in_node = ((num_sectors_total < NODE_CAPACITY) ?
                                num_sectors_total : NODE_CAPACITY);

        printf("inode_create(%d, %d): Trace 8\tnext_sector: %d\tnum_sectors_in_node: %d\n", sector, length, next_sector, num_sectors_in_node);
        while (next_sector < num_sectors_in_node)
        {
          printf("inode_create(%d, %d): Trace 9\tnext_sector: %d\tnum_sectors_in_node: %d\n", sector, length, next_sector, num_sectors_in_node);
          if (!free_map_allocate(1, &data_addr))
          {
            printf("inode_create(%d, %d): Trace 10\n", sector, length);
            // Free all allocated pages
            free(main_data);
            return false;
          }
          else
          {
            printf("inode_create(%d, %d): Trace 11\n", sector, length);
            // Successfully allocated disk space for DATA node
            current_data->blockptrs[next_sector] = ptr_create(data_addr);
            ptr_set_exist(&current_data->blockptrs[next_sector]);

            // Write block of all zero to data sector
            block_write(fs_device, data_addr, &zeros);

            next_sector++;
          }
        }

        printf("inode_create(%d, %d): Trace 12\n", sector, length);
        // Write inode data to corresponding disk sector
        block_write(fs_device, current_node->sector, current_data);

        // Update loop parameters
        num_sectors_total -= num_sectors_in_node;
        prev_node = current_node;
      }
    }

    printf("inode_create(%d, %d): Trace 13\n", sector, length);
    // Everything OK: Set attributes of the main inode, and write to disk
    main_data->length = length;
    block_write(fs_device, sector, main_data);
  }

  printf("inode_create(%d, %d): Trace 14 EXIT\n", sector, length);
  return true;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open(block_sector_t sector)
{
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
      return inode;
    }
  }

  /* Allocate memory. */
  inode = malloc(sizeof *inode);

  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front(&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read(fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen(struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;

  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber(const struct inode *inode)
{
  ASSERT(inode != NULL);
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close(struct inode *inode)
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
  {
    /* Remove from inode list and release lock. */
    list_remove(&inode->elem);

    /* Deallocate blocks if removed. */
    if (inode->removed)
    {
      struct inode_disk *current_data = inode->data;
      block_sector_t current_addr = inode->sector;
      inode_ptr next = current_data->doubleptr;

      while (ptr_exists(&next))
      {
        free_map_release(current_addr, 1);

        current_addr = ptr_get_address(&next);
        block_read(fs_device, current_addr, &current_data);
        next = current_data->doubleptr;
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
  ASSERT(inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at(struct inode *inode, void *buffer_, off_t size, off_t offset)
{
  ASSERT(inode != NULL);
  ASSERT(buffer_ != NULL);

  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0)
  {
    /* Disk sector to read, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector(inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually copy out of this sector. */
    int chunk_size = size < min_left ? size : min_left;

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
  ASSERT(inode != NULL);
  ASSERT(buffer_ != NULL);
  
  printf("inode_write_at(%x, %x, %d, %d): Trace 1\n", inode, buffer_, size, offset);
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
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length(inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually write into this sector. */
    int chunk_size = size < min_left ? size : min_left;

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

  printf("inode_write_at(%x, %x, %d, %d): Trace 15\n", inode, buffer_, size, offset);
  free(bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write(struct inode *inode)
{
  inode->deny_write_cnt++;
  ASSERT(inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write(struct inode *inode)
{
  ASSERT(inode->deny_write_cnt > 0);
  ASSERT(inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length(const struct inode *inode)
{
  printf("inode_length(%x): Trace 1\n", inode);
  printf("inode_length(%x): return %d\n", inode, inode->data->length);
  return inode->data->length;
}
