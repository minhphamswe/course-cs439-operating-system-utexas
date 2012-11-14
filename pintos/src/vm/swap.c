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
#include "threads/vaddr.h"
#include "threads/thread.h"

#include "userprog/pagedir.h"

#include "stdio.h"

static struct block* swap_block;
static uint32_t swap_size;
static uint32_t next_sector;

struct list swap_list;

struct swap_slot {
  tid_t tid;
  uint32_t sector;
  struct page_entry *upage;
  struct list_elem elem;
};

/* Forward declaration for internal uses */
struct swap_slot* get_free_slot(void);
struct swap_slot* get_used_slot(struct frame *fp);

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
//   printf("Pushing to swap\n");
  ASSERT(fp != NULL);

//   printf("Next sector: %d\n", next_sector);
  struct swap_slot *temp = get_free_slot();
  if (temp) {
    // Track the swap slot
    temp->upage = fp->upage;
    temp->upage->status = PAGE_SWAPPED;
    temp->tid = fp->tid;
    list_push_front(&swap_list, &temp->elem);

    // Write data out onto the swap disk
    write_swap(temp);

    // Update the CPU-based page directory
    struct thread *t = thread_current();
    pagedir_clear_page(t->pagedir, temp->upage->uaddr);

//     printf("Frame Push Address: %x\n", temp->frame->kpage);
//     printf("Clearing page: %x\n", temp->upage);
//     printf("Clearing address: %x\n", fp->upage->uaddr);
  }
  return (temp != NULL);
}

/**
 * Read a frame from the swap space into main memory. Return false if the
 * frame cannot be found from the swap slots.
 */
bool pull_from_swap(struct frame* fp)
{
//   printf("Pulling from swap\n");
  ASSERT(fp != NULL);

  struct swap_slot *slot = get_used_slot(fp);
  if (slot) {
//     printf("Frame pull address: %x\n", slot->frame->kpage);
    // Read data in the swap slot into memory
    read_swap(slot);

    // Update the CPU-based page directory
    struct thread *t = thread_current();
//     printf("Setting frame given: %x\n", fp);
//     printf("Setting frame looked up: %x\n", slot->frame);
//     printf("Setting page: %x\n", slot->upage);
//     printf("Setting address: %x\n", fp->upage->uaddr);
    pagedir_set_page(t->pagedir,
                     slot->upage->uaddr,
                     slot->upage->frame->kpage,
                     slot->upage->frame->writable);

    // Update the page
    slot->upage->status = PAGE_PRESENT;

    // Update the slot
    slot->upage = NULL;

    // Rotate slot element
    list_remove(&slot->elem);
    list_push_back(&swap_list, &slot->elem);
  }
  return (slot != NULL);
}

bool get_from_swap(struct frame* fp, void* uaddr)
{
//  printf("Getting from swap\n");
  ASSERT(fp != NULL);

  struct swap_slot *slot = get_slot_by_vaddr(uaddr);
  if (slot) {
//     printf("Frame pull address: %x\n", slot->frame->kpage);
    slot->upage->frame = fp;
    // Read data in the swap slot into memory
    read_swap(slot);
//printf("back from read\n");
    // Update the CPU-based page directory
 //   printf("trace1\n");
    struct thread *t = thread_current();
//     printf("Setting frame given: %x\n",slot->upage->frame->kpage);
//     printf("Setting frame looked up: %x\n", fp->kpage);
//     printf("Setting page: %x\n", slot->upage);
//     printf("Setting address: %x\n", fp->upage->uaddr);
    pagedir_clear_page(t->pagedir, slot->upage->frame->upage->uaddr);
    pagedir_set_page(t->pagedir,
                     slot->upage->uaddr,
                     slot->upage->frame-> kpage,
                     slot->upage->frame->writable);
                     
    // Update the page
    slot->upage->status = PAGE_PRESENT;
    
    // Update the slot
    slot->upage = NULL;
    
    // Rotate slot element
    list_remove(&slot->elem);
    list_push_back(&swap_list, &slot->elem);
  }
  return (slot != NULL);
}

/**
 * Return the pointer to a free swap slot. Return NULL if the swap disk is
 * full.
 */
struct swap_slot* get_free_slot(void)
{
  struct swap_slot *temp;
  struct list_elem *e;

  e = list_tail(&swap_list)->prev;

  if (e != list_head(&swap_list) &&
      list_entry(e, struct swap_slot, elem)->upage == NULL) {
    // Last element is free: return it
    e = list_pop_back(&swap_list);
    temp = list_entry(e, struct swap_slot, elem);
  }
  else {
    // No free element: make one if there's still space on the swap disk
    if (next_sector < swap_size) {
      temp = malloc(sizeof(struct swap_slot));
      temp->sector = next_sector;
      next_sector += (PGSIZE / BLOCK_SECTOR_SIZE);
    }
    else {
      temp = NULL;
    }
  }
  return temp;
}

struct swap_slot* get_used_slot(struct frame* fp)
{
  struct list_elem *e;
  tid_t tid = thread_current()->tid;
  for (e = list_begin(&swap_list); e != list_end(&swap_list);
       e = list_next(e))
  {
    struct swap_slot *slot = list_entry(e, struct swap_slot, elem);
    if (slot->upage && slot->upage->frame == fp && slot->tid == tid)
      return slot;
  }
  return NULL;
}

struct swap_slot* get_slot_by_vaddr(void* uaddr)
{
  uaddr = (((uint32_t) uaddr) / PGSIZE) * PGSIZE;
  struct list_elem *e;
  tid_t tid = thread_current()->tid;
  for (e = list_begin(&swap_list); e != list_end(&swap_list);
       e = list_next(e))
  {
    struct swap_slot *slot = list_entry(e, struct swap_slot, elem);
    if (slot->upage && slot->upage->uaddr == uaddr && slot->tid == tid)
      return slot;
  }
  return NULL;
}

/** Read the disk sector into the frame; both are pointed to by SLOT.*/
void read_swap(struct swap_slot* slot)
{
printf("Read swap: %d\n", slot->sector);
  void *kpage = slot->upage->frame->kpage;
  int i;
  for (i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++)
    block_read(swap_block, slot->sector + i, kpage + i * BLOCK_SECTOR_SIZE);
}

/** Write the frame out into the disk sector; both are pointed to by SLOT.*/
void write_swap(struct swap_slot* slot)
{
printf("Write swap: %d\n", slot->sector);
  void *uaddr = slot->upage->frame->kpage;
  printf("%x, %d, %d\n",uaddr,slot->tid,thread_current()->tid);
  int i;
  for (i = 0; i < PGSIZE / BLOCK_SECTOR_SIZE; i++)
    block_write(swap_block, slot->sector + i, uaddr + i * BLOCK_SECTOR_SIZE);
}


