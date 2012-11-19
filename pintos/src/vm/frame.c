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

#include "userprog/pagedir.h"

#include "lib/kernel/list.h"

#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/interrupt.h"

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
//   printf("Start allocate_frame()\n");
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

//   printf("End allocate_frame()\n");
  return fp;
}

bool install_frame(struct frame* fp, int writable) {
  if (fp == NULL) {
//     printf("install_frame(): Null frame pointer, returning false\n");
    return false;
  }
  else {
    enum intr_level old_level = intr_disable();
    struct thread *t = thread_current();
    void *uaddr = fp->upage->uaddr;
    void *kpage = fp->kpage;
    bool success = false;

    if (pagedir_get_page(t->pagedir, uaddr) == NULL)
    {
      if (pagedir_set_page(t->pagedir, uaddr, kpage, writable)) {
      printf("install_frame(): Mapped thread %x(%d) | page %x -> %x @ %x(%d) -> %x(%d)\n", t, t->tid, uaddr, kpage, fp->upage, fp->upage->tid, fp, fp->tid);
      list_push_back(&all_frames, &fp->elem);
      fp->upage->status = PAGE_PRESENT;
      success = true;
      }
//       else printf("page_set_page failed\n");
    }
    else {
//       printf("pagedir_get_page failed\n");
      // Physical address is already allocated to some process:
      // For now free frame & return false
      printf("install_frame(): Mapping thread %x(%d) | page %x -> %x @ %x(%d) -> %x(%d) FAILED!\n", t, t->tid, uaddr, kpage, fp->upage, fp->upage->tid, fp, fp->tid);
//       free_frame(fp);   // FIXME: this may cause a PANIC;
      success = false;
    }
    intr_set_level(old_level);

//     printf("install_frame(): success is: %d\n", success);
    return success;
  }
}

void free_frame(struct frame* fp)
{
  // Remove page->frame mapping from the CPU-based page directory
  struct thread *t = thread_by_tid(fp->tid);
  void *uaddr = fp->upage->uaddr;

  enum intr_level old_level = intr_disable();
  if (pagedir_get_page(t->pagedir, uaddr) != NULL) {
    // Page is in memory and registered to this thread
    printf("free_frame(): Unmapping thread %x(%d) | page %x -/-> %x @ %x(%d) -/-> %x(%d)\n", t, t->tid, uaddr, fp->kpage, fp->upage, fp->upage->tid, fp, fp->tid);
    pagedir_clear_page(t->pagedir, uaddr);
    palloc_free_page(fp->kpage);
  }
//   else {   TODO
//     // Page is registered to this thread but may be swapped out
//   }
  intr_set_level(old_level);

  // Remove page<->frame mapping from our supplemental structures
  fp->upage->frame = NULL;
  fp->upage = NULL;
  free(fp);
}

struct frame* evict_frame(void)
{
//  printf("Start evict_frame()\n");
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

  enum intr_level old_level = intr_disable();

  // Remove page->frame mapping from the CPU-based page directory
  struct thread *t = thread_current();
  struct thread *victim = thread_by_tid(fp->tid);
  printf("evict_frame(): Unmapping thread %x(%d) | page %x =/=> %x @ %x(%d) =/=> %x(%d)\n", t, t->tid, fp->upage->uaddr, fp->kpage, fp->upage, fp->upage->tid, fp, fp->tid);
  pagedir_clear_page(victim->pagedir, fp->upage->uaddr);

  // Remove page<->frame mapping from our supplemental structures
  fp->upage->frame = NULL;
  fp->upage = NULL;

  intr_set_level(old_level);

  list_push_back(&all_frames, &fp->elem);

//   printf("End evict_frame()\n");
  return fp;
}

