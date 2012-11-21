/**
 * The swap table tracks in-use and free swap slots. It should allow picking
 * an unused swap slot for evicting a page from its frame to the swap
 * partition. It should allow freeing a swap slot when its page returns to
 * memory or the process whose page was swapped is terminated.
 *
 * You may use the BLOCK_SWAP block device for swapping, obtaining the struct
 * block that represents it by calling block_get_role(). From the "vm/build"
 * directory, use the command pintos-mkdisk swap.dsk --swap-size=n to create
 * a disk named "swap.dsk" that contains a n-MB swap partition. Afterward,
 * "swap.dsk" will automatically be attached as an extra disk when you run
 * pintos. Alternatively, you can tell pintos to use a temporary n-MB swap
 * disk for a single run with "--swap-size=n".
 *
 * Swap slots should be allocated lazily, that is, only when they are actually
 * required by eviction. (Reading data pages from the executable and writing
 * them to swap immediately at process startup is not lazy.) Swap slots
 * should not be reserved to store particular pages.
 *
 * Free a swap slot when its contents are read back into a frame.
 */
#include "vm/swap.h"
#include "vm/frame.h"

#include "devices/block.h"
#include "lib/kernel/list.h"
#include "lib/debug.h"

#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"
#include "threads/interrupt.h"

#include "stdio.h"

static struct block* swap_block;
static uint32_t swap_size;
static uint32_t next_sector;

struct list swap_list;

/* Forward declaration for internal uses */
struct swap_slot* get_free_slot(void);

void read_swap(struct swap_slot *slot);
void write_swap(struct swap_slot *slot);

/** Initalize the swap system */
void swap_init(void) {
//   printf("Initializing swap\n");
  swap_block = block_get_role(BLOCK_SWAP);
  swap_size = (swap_block) ? block_size(swap_block) : 0;
  next_sector = 0;
  list_init(&swap_list);
}

bool push_to_swap(struct frame* fp)
{
//   printf("Start push_to_swap(%x)\n", fp);
  ASSERT(fp != NULL);
  ASSERT(fp->upage != NULL);

//   printf("Next sector: %d\n", next_sector);
//    printf("push_to_swap(%x): Trace 1\n", fp);
  struct swap_slot *slot = get_free_slot();
//   printf("push_to_swap(%x): Trace 2\n", fp);
  if (slot) {
//     printf("push_to_swap(%x): Trace 3\n", fp);

    // Track the swap slot
    slot->upage = fp->upage;
    slot->upage->swap = slot;
    slot->tid = slot->upage->tid;

    // Write data out onto the swap disk
//      printf("push_to_swap(%x): Trace 4\n", fp);
    write_swap(slot);
//      printf("push_to_swap(%x): Trace 5\n", fp);

    // Update the CPU-based page directory
    enum intr_level old_level = intr_disable();
    list_push_front(&swap_list, &slot->elem);
    intr_set_level(old_level);
//      printf("push_to_swap(%x): Trace 6\n", fp);
  }
//   printf("End push_to_swap(%x)\n", fp);
  return (slot != NULL);
}

/**
 * Read a frame from the swap space into main memory. Return false if the
 * frame cannot be found from the swap slots.
 */
bool pull_from_swap(struct page_entry *upage)
{
//   printf("Start pull_from_swap(%x)\n", upage);
  ASSERT(upage != NULL);

  struct swap_slot *slot = get_swapped_page(upage);
  if (slot != NULL) {
//     printf("Frame pull address: %x\n", slot->frame->kpage);
    // Read data in the swap slot into memory
    read_swap(slot);

    // Update the slot
    slot->upage->swap = NULL;
    slot->upage = NULL;

    enum intr_level old_level = intr_disable();

    // Rotate slot element
    list_remove(&slot->elem);
    list_push_back(&swap_list, &slot->elem);

    intr_set_level(old_level);
  }
//   printf("End pull_from_swap(%x)\n", upage);
  return (slot != NULL);
}

void free_swap(struct swap_slot* slot)
{
  ASSERT(slot != NULL);
  ASSERT(slot->upage != NULL);
  ASSERT(slot->upage->swap != NULL);

  if (slot != NULL) {
    if (slot->upage != NULL) {
      slot->upage->swap = NULL;
      slot->upage = NULL;

      enum intr_level old_level = intr_disable();

      // Rotate slot to back of list
      list_remove(&slot->elem);
      list_push_back(&swap_list, &slot->elem);

      intr_set_level(old_level);
    }
  }
}


/**
 * Return the pointer to a free swap slot. Return NULL if the swap disk is
 * full.
 */
struct swap_slot* get_free_slot(void)
{
  struct swap_slot *slot;
  struct list_elem *e;

  enum intr_level old_level = intr_disable();

  e = list_tail(&swap_list)->prev;

  if (e != list_head(&swap_list) &&
      list_entry(e, struct swap_slot, elem)->upage == NULL) {
    // Last element is free: return it
    e = list_pop_back(&swap_list);
    slot = list_entry(e, struct swap_slot, elem);
  }
  else {
    // No free element: make one if there's still space on the swap disk
    if (next_sector < swap_size) {
      slot = malloc(sizeof(struct swap_slot));
      slot->sector = next_sector;
      next_sector += (PGSIZE / BLOCK_SECTOR_SIZE);
    }
    else {
      slot = NULL;
    }
  }

  intr_set_level(old_level);
  return slot;
}

struct swap_slot* get_swapped_page(struct page_entry *upage)
{
  struct list_elem *e;

  enum intr_level old_level = intr_disable();
  for (e = list_begin(&swap_list); e != list_end(&swap_list);
       e = list_next(e))
  {
    struct swap_slot *slot = list_entry(e, struct swap_slot, elem);
    if (slot->upage && slot->upage == upage) {
      intr_set_level(old_level);
      return slot;
    }
  }
  intr_set_level(old_level);
  return NULL;
}

/** Read the disk sector into the frame; both are pointed to by SLOT.*/
void read_swap(struct swap_slot* slot)
{
//printf("Read swap: %d\n", slot->sector);
  void *kpage = slot->upage->frame->kpage;
  int i;
//   enum intr_level old_level = intr_disable();
  for (i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++)
    block_read(swap_block, slot->sector + i, kpage + i * BLOCK_SECTOR_SIZE);
//   intr_set_level*/(old_level);
}

/** Write the frame out into the disk sector; both are pointed to by SLOT.*/
void write_swap(struct swap_slot* slot)
{
//   printf("Start write_swap(%x)\n", slot);
//   printf("Upage: %x, Upage->UAddr: %x, Upage->Frame: %x, Upage->Frame->Kpage: %x\n", slot->upage, slot->upage->uaddr, slot->upage->frame, slot->upage->frame->kpage);
  void *kpage = slot->upage->frame->kpage;
  int i;

  for (i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++) {
    block_write(swap_block, slot->sector + i, kpage + i * BLOCK_SECTOR_SIZE);
  }

//    printf("End write_swap(%x)\n", slot);
}

bool clean_swap(int tid)
{
//   printf("Start clean_swap(%d)\n", tid);
  struct list_elem *e;
  enum intr_level old_level = intr_disable();

  for (e = list_begin(&swap_list); e != list_end(&swap_list);
       e = list_next(e))
  {
    struct swap_slot *slot = list_entry(e, struct swap_slot, elem);

    // Check to see if we're done with the active slots
    if (slot->upage != NULL && slot->tid == tid) {
//printf("Slot %d cleaned\n", slot->sector);
      // Clear the slot
      slot->tid = 0;
      slot->upage = NULL;

      // Rotate slot to back of list
      list_remove(&slot->elem);
      list_push_back(&swap_list, &slot->elem);
    }
  }

  intr_set_level(old_level);
//   printf("End clean_swap(%d)\n", tid);
  return true;
}
