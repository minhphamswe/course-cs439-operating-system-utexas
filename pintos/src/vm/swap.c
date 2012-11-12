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
#include "threads/malloc.h"

#include "stdio.h"

static struct block* swap_block;
static uint32_t swap_size;
static uint32_t next_sector;

struct list swap_list;

struct swap_slot {
  uint32_t sector;
  struct frame *frame;
  bool used;
  struct list_elem elem;
};

/** Initalize the swap system */
void swap_init(void) {
  swap_block = block_get_role(BLOCK_SWAP);
//   printf("swap block address: %x\n", swap_block);
//    swap_size = block_size(swap_block);
//   next_sector = 0;
//   list_init(&swap_list);*/
}

void push_to_swap(struct frame* fp)
{
  if(next_sector >= block_size(swap_block))
  {
    PANIC ("Failed to allocate memory for swap slot.\n");
  }
  struct swap_slot *temp;
  if (!list_empty(&swap_list))
  {
    struct list_elem *e = list_back(&swap_list);
    temp = list_entry(e, struct swap_slot, elem);
    if(temp->used == false)
    {
        e = list_pop_back(&swap_list);
    }
    else
    {
      temp = malloc(sizeof(struct swap_slot));
      temp->sector = next_sector;
      next_sector++;
    }
  }
  else
  {
      temp = malloc(sizeof(struct swap_slot));
      temp->sector = next_sector;
      next_sector++;
  }
  temp->frame = fp;
  temp->used = true;
  list_push_front(&swap_list, &temp->elem);
  block_write(swap_block, temp->sector, fp->upage->uaddr);
  fp->upage->status = PAGE_SWAPPED;
}


void pull_from_swap(struct frame* fp)
{
  struct list_elem *e;
  for(e = list_begin(&swap_list); e != list_end(&swap_list); e = list_next(e))
  {
    struct swap_slot *slot = list_entry(e, struct swap_slot, elem);
    if (slot->frame == fp) {
      block_read(swap_block, slot->sector, fp->kpage);
      list_remove(&slot->elem);
      slot->used = false;
      list_push_back(&swap_list, &slot->elem);
      return;
    }
  }
}