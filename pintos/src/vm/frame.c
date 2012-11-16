/**
The frame table contains one entry for each frame that contains a user page.
Each entry in the frame table contains a pointer to the page, if any, that
currently occupies it, and other data of your choice. The frame table allows
Pintos to efficiently implement an eviction policy, by choosing a page to
evict when no frames are free.

The frames used for user pages should be obtained from the "user pool,"
by calling palloc_get_page(PAL_USER). You must use PAL_USER to avoid allocating
from the "kernel pool," which could cause some test cases to fail unexpectedly
(see Why PAL_USER?).
If you modify "palloc.c" as part of your frame table implementation, be sure
to retain the distinction between the two pools.

The most important operation on the frame table is obtaining an unused frame.
This is easy when a frame is free. When none is free, a frame must be made
free by evicting some page from its frame.

The process of eviction comprises roughly the following steps:

    Choose a frame to evict, using your page replacement algorithm. The
    "accessed" and "dirty" bits in the page table, described below, will
    come in handy.

    Remove references to the frame from any page table that refers to it.

    Unless you have implemented sharing, only a single page should refer
    to a frame at any given time.

    If necessary, write the page to the file system or to swap. For more
    information about swapping, See section 4.1.6 Managing the Swap Table. 

The evicted frame may then be used to store a different page.

If no frame can be evicted without allocating a swap slot, but swap is
full, panic the kernel. Real OSes apply a wide range of policies to recover
from or prevent such situations, but these policies are beyond the scope
of this project.
*/
#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/page.h"

#include "lib/kernel/list.h"

#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

#include "stdio.h"

static struct list all_frames;

/** Initialize the frame table system. */
void frame_init(void) {
  list_init(&all_frames);
}

/**
 * Obtain a new frame to hold the page pointed to by UPAGE.
 * If there is no more space in memory, evict a frame.
 */
struct frame* allocate_frame(struct page_entry* upage) {
  // Allocate frame and get its kernel page address
//   printf("Allocating frame...\n");
  struct frame *fp;
  uint8_t *kpage;
  void * uaddr;

  // Compute a page-aligned user page address
  uaddr = upage->uaddr;
  uaddr = (((uint32_t) uaddr) / PGSIZE) * PGSIZE;

  // Attempt to allocate a new frame
  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage == NULL) {
    // All physical addresses are used: evict a frame
    fp = evict_frame();
    kpage = fp->kpage;
  }
  else {
    // We have space: register a new frame to track the address
    fp = malloc(sizeof(struct frame));
  }

  // Set frame attributes
  fp->upage = upage;
  fp->kpage = kpage;
  fp->tid = upage->tid;

  // Update page entry attributes
  upage->frame = fp;
  upage->status = PAGE_PRESENT;

//   printf("allocate_frame() returning: %x\n", fp);
  return fp;
}

bool install_frame(struct frame* fp, int writable) {
  if (fp == NULL) {
//     printf("install_frame(): Null frame pointer, returning false\n");
    return false;
  }
  else {
    struct thread *t = thread_current();
    void *uaddr = fp->upage->uaddr;
    void *kpage = fp->kpage;
    bool success = false;

//     printf("Mapping page %x -> %x\n", uaddr, kpage);

    if (pagedir_get_page(t->pagedir, uaddr) == NULL
        && pagedir_set_page(t->pagedir, uaddr, kpage, writable)) {

      // Check if likely code or data segment.  Code is unavailable for eviction
  //     if(uaddr > 0xb0000000)
  //         fp->writable = false;
  //     else
      list_push_back(&all_frames, &fp->elem);
      success = true;
    }
    else {
      // Physical address is already allocated to some process:
      // For now free frame & return false
//       free_frame(fp);   // FIXME: this may cause a PANIC
      success = false;
    }

//     printf("install_frame(): success is: %d\n", success);
    return success;
  }
}

void free_frame(struct frame* fp)
{
  struct thread *t = thread_current();
  void *uaddr = fp->upage->uaddr;
  pagedir_clear_page(t->pagedir, uaddr);
  palloc_free_page(fp->kpage);
}

struct frame* evict_frame(void)
{
//  printf("Evicting frame\n");
  // Remove the oldest frame from frame table
  struct list_elem *e = list_pop_front(&all_frames);
  struct frame *fp = list_entry(e, struct frame, elem);

//   while(!fp->upage->w) {
//     list_push_back(&all_frames, &fp->elem);
//     e = list_pop_front(&all_frames);
//     fp = list_entry(e, struct frame, elem);
//   }
//  printf("Kpage: %x  Upage: %x Writable: %d\n", fp->kpage, fp->upage->uaddr, fp->writable);

  // Write it to swap space
  push_to_swap(fp);

  list_push_back(&all_frames, &fp->elem);

  return fp;
}

