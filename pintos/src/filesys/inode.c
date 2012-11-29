#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define NUM_SECTORS 250

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
{
  off_t length;                         /* File size in bytes. */
  unsigned magic;                       /* Magic number. */
	inode_ptr blockptrs[NUM_SECTORS];	    /* Index of data pointers */
	inode_ptr doubleptr;                  /* Pointer to next indirect inode */
};

inode_ptr ptr_create(block_sector_t sector)
{
  return (inode_ptr)(sector & 0x3FFF);
}

// Get disk sector address from inode ptr
block_sector_t ptr_get_address(inode_ptr *ptr)
{
  return (block_sector_t)((*ptr) & 0x3FFF);
}

void ptr_set_exist(inode_ptr *ptr)
{
  (*ptr) = (*ptr) | 0x8000;
}

// Whether or not the meta data says the pointer exists on disk
bool ptr_exists(inode_ptr *ptr)
{
  return ((*ptr) >> 15);
}

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length) {
    uint16_t next_addr = inode->sector;
    struct inode_disk data = inode->data;
    ASSERT(&data != &inode->data);

    while (pos / BLOCK_SECTOR_SIZE > NUM_SECTORS) {
      next_addr = ptr_get_address(&data.doubleptr);
      block_read(fs_device, next_addr, &data);
      pos -= BLOCK_SECTOR_SIZE * NUM_SECTORS;
    }

    next_addr = data.blockptrs[pos/BLOCK_SECTOR_SIZE];
    return (block_sector_t) ptr_get_address(&next_addr);
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  // Structure we will write to disk for main inode
  printf("inode_create(%d, %d): Trace 1\n", sector, length);
  struct inode_disk *main_inode = NULL;

  ASSERT (length >= 0);

  // If this assertion fails, the inode structure is not exactly
  //   one sector in size, and you should fix that.
  ASSERT (sizeof *main_inode == BLOCK_SECTOR_SIZE);

  main_inode = calloc (1, sizeof *main_inode);
  printf("inode_create(%d, %d): Trace 2\n", sector, length);
  if (main_inode != NULL)
  {
    printf("inode_create(%d, %d): Trace 3\n", sector, length);
    size_t numSectors = bytes_to_sectors (length);
    size_t numIndirects = numSectors / NUM_SECTORS;
    size_t sectorsInBlock;  // Number of sectors left in this data block
    
    main_inode->length = length;
    main_inode->magic = INODE_MAGIC;

    block_sector_t mainAddress;
    block_sector_t indirectAddress;
    block_sector_t dataAddress;
    block_sector_t currentAddress;
    
    struct inode_disk *current_inode = main_inode;
    
    block_write (fs_device, sector, main_inode);
    
    printf("inode_create(%d, %d): Trace 4\n", sector, length);
    // First get the main inode for the file
    if (free_map_allocate (1, &mainAddress))
    {
      printf("inode_create(%d, %d): Trace 5\n", sector, length);
      currentAddress = mainAddress;
      
      // If we will have indirects, go through those first.  All will be
      // NUM_SECTORS in length
      while (numIndirects > 0)
      {
        printf("inode_create(%d, %d): Trace 6\n", sector, length);
        sectorsInBlock = 0;
        numSectors -= NUM_SECTORS;
        if (free_map_allocate(1, &indirectAddress))
        {
          printf("inode_create(%d, %d): Trace 7\n", sector, length);
          current_inode->doubleptr = ptr_create(indirectAddress);
          ptr_set_exist(&current_inode->doubleptr);
          while (sectorsInBlock < NUM_SECTORS)
          {
            printf("inode_create(%d, %d): Trace 8\n", sector, length);
            if (free_map_allocate(1, &dataAddress))
            {
              printf("inode_create(%d, %d): Trace 9\n", sector, length);
              current_inode->blockptrs[sectorsInBlock] = ptr_create(dataAddress);
              current_inode->blockptrs[sectorsInBlock] = ptr_create(dataAddress);
              ptr_set_exist(&current_inode->blockptrs[sectorsInBlock]);
              sectorsInBlock++;
            }
            else
            {
              printf("inode_create(%d, %d): Trace 10\n", sector, length);
              // Free all allocated pages
              free (main_inode);
              return false;
            }
          }
        }
        else
        {
          printf("inode_create(%d, %d): Trace 11\n", sector, length);
          // Free all allocated pages
          free (main_inode);
          return false;
        }
        printf("inode_create(%d, %d): Trace 12\n", sector, length);
        numIndirects--;
        block_write (fs_device, currentAddress, current_inode);
        currentAddress = indirectAddress;
        printf("inode_create(%d, %d): Trace 13\n", sector, length);
      }
      // Have to go through one more time for the blocks in the last inode
      sectorsInBlock = (numSectors < NUM_SECTORS) ? numSectors : NUM_SECTORS;
      printf("inode_create(%d, %d): Trace 14\n", sector, length);
      while (sectorsInBlock > 0)
      {
        printf("inode_create(%d, %d): Trace 15\n", sector, length);
        if (free_map_allocate(1, &dataAddress))
        {
          printf("inode_create(%d, %d): Trace 16\n", sector, length);
          current_inode->blockptrs[NUM_SECTORS - sectorsInBlock] = ptr_create(dataAddress);
          ptr_set_exist(&current_inode->blockptrs[NUM_SECTORS - sectorsInBlock]);
          sectorsInBlock--;
        }
        else
        {
          printf("inode_create(%d, %d): Trace 17\n", sector, length);
          // Free all allocated pages
          free (main_inode);
          return false;
        }
      }
      printf("inode_create(%d, %d): Trace 18\n", sector, length);
      block_write (fs_device, currentAddress, current_inode);
    }
  }
  return true;
}
  
/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
  {
    /* Remove from inode list and release lock. */
    list_remove (&inode->elem);

    /* Deallocate blocks if removed. */
    if (inode->removed) 
    {
      struct inode_disk current_data = inode->data;
      block_sector_t current_addr = inode->sector;
      inode_ptr next = current_data.doubleptr;

      while (ptr_exists(&next)) {
        free_map_release(current_addr, 1);

        current_addr = ptr_get_address(&next);
        block_read(fs_device, current_addr, &current_data);
        next = current_data.doubleptr;
      }
    }

    free (inode); 
  }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
} 

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  printf("inode_write_at(%x, %x, %d, %d): Trace 1\n", inode, buffer_, size, offset);
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  printf("inode_write_at(%x, %x, %d, %d): Trace 2\n", inode, buffer_, size, offset);
  if (inode->deny_write_cnt) {
    printf("inode_write_at(%x, %x, %d, %d): Trace 3\n", inode, buffer_, size, offset);
    return 0;
  }

  printf("inode_write_at(%x, %x, %d, %d): Trace 4\n", inode, buffer_, size, offset);
  while (size > 0) 
  {
    printf("inode_write_at(%x, %x, %d, %d): Trace 5\n", inode, buffer_, size, offset);
    /* Sector to write, starting byte offset within sector. */
    block_sector_t sector_idx = byte_to_sector (inode, offset);
    int sector_ofs = offset % BLOCK_SECTOR_SIZE;

    /* Bytes left in inode, bytes left in sector, lesser of the two. */
    off_t inode_left = inode_length (inode) - offset;
    int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
    int min_left = inode_left < sector_left ? inode_left : sector_left;

    /* Number of bytes to actually write into this sector. */
    int chunk_size = size < min_left ? size : min_left;
    if (chunk_size <= 0) {
      printf("inode_write_at(%x, %x, %d, %d): Trace 6\n", inode, buffer_, size, offset);
      break;
    }
    if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
    {
      printf("inode_write_at(%x, %x, %d, %d): Trace 7\n", inode, buffer_, size, offset);
      /* Write full sector directly to disk. */
      block_write (fs_device, sector_idx, buffer + bytes_written);
    }
    else 
    {
      /* We need a bounce buffer. */
      printf("inode_write_at(%x, %x, %d, %d): Trace 8\n", inode, buffer_, size, offset);
      if (bounce == NULL) 
      {
        printf("inode_write_at(%x, %x, %d, %d): Trace 9\n", inode, buffer_, size, offset);
        bounce = malloc (BLOCK_SECTOR_SIZE);
        if (bounce == NULL) {
          printf("inode_write_at(%x, %x, %d, %d): Trace 10\n", inode, buffer_, size, offset);
          break;
        }
      }

      /* If the sector contains data before or after the chunk
         we're writing, then we need to read in the sector
         first.  Otherwise we start with a sector of all zeros. */
      if (sector_ofs > 0 || chunk_size < sector_left) {
        printf("inode_write_at(%x, %x, %d, %d): Trace 11\n", inode, buffer_, size, offset);
        block_read (fs_device, sector_idx, bounce);
      }
      else {
        printf("inode_write_at(%x, %x, %d, %d): Trace 12\n", inode, buffer_, size, offset);
        memset (bounce, 0, BLOCK_SECTOR_SIZE);
      }
      printf("inode_write_at(%x, %x, %d, %d): Trace 13\n", inode, buffer_, size, offset);
      memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
      block_write (fs_device, sector_idx, bounce);
    }

    printf("inode_write_at(%x, %x, %d, %d): Trace 14\n", inode, buffer_, size, offset);
    /* Advance. */
    size -= chunk_size;
    offset += chunk_size;
    bytes_written += chunk_size;
  }
  printf("inode_write_at(%x, %x, %d, %d): Trace 15\n", inode, buffer_, size, offset);
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  printf("inode_length(%x): Trace 1\n", inode);
  printf("inode_length(%x): return %d\n", inode, inode->data.length);
  return inode->data.length;
}
