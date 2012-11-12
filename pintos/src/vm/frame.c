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
 * Allocate a new frame to hold the page pointed to by UPAGE.
 * Frames will be evicted to make space for the page. This method should
 * return true (success) in all cases except when swap is full.
 */
int allocate_frame(struct page_entry *upage, int writable) {
  // Allocate frame and get its kernel page address
//   printf("Allocating frame...\n");
  struct thread *t = thread_current ();
  uint8_t *kpage;
  void * uaddr;

  // Compute a valid user page address
  uaddr = upage->uaddr;
  uaddr = (((uint32_t) uaddr) / PGSIZE) * PGSIZE;
//     printf("User address after: %x\n", (uint32_t) upage);

  if (pagedir_get_page (t->pagedir, uaddr) == NULL) {
    // Page is not already allocated
    kpage = palloc_get_page (PAL_USER | PAL_ZERO);
    if (kpage == NULL) {
      kpage = evict_frame();
    }

    if (pagedir_set_page (t->pagedir, uaddr, kpage, writable)) {
      // Map and track frame
      struct frame *fp = malloc(sizeof(struct frame));

      fp->upage = upage;
      fp->kpage = kpage;
      fp->writable = writable;
      list_push_back(&all_frames, &fp->elem);

      // Update page entry
      upage->frame = fp;
      upage->status = PAGE_PRESENT;

      return true;
    }
    else {
      // Page is already allocated to some process: free frame & return false
      //       printf("TODO: Page is already allocated to some other process. Needs to implement sharing.\n");
      palloc_free_page (kpage);
      return false;
    }
  }
  else {
    return true;
  }
}

/** Remap the frame at KPAGE to contain the page pointed at by UPAGE.*/
void set_frame(struct page_entry* upage, void* kpage, int writable)
{
  struct list_elem *e;

  for (e = list_begin (&all_frames); e != list_end (&all_frames);
       e = list_next (e))
  {
    struct frame *f = list_entry (e, struct frame, elem);
    if(f->kpage == upage) {
      f->upage = upage;
      f->writable = writable;
    }
  }
}

/** Remove the frame pointed to by fp */
void unset_frame(struct frame *fp)
{
  // Free the frame
  palloc_free_page(fp->kpage);

  // Remove the frame from the list
  list_remove(&fp->elem);

  // Free the frame
  free(fp);
}

/** Get a KPAGE by UPAGE */
struct frame * get_frame(struct page_entry* upage)
{
  struct list_elem *e;

  for (e = list_begin (&all_frames); e != list_end (&all_frames);
       e = list_next (e))
  {
    struct frame *f = list_entry (e, struct frame, elem);
    if(f->upage == upage)
      return f;
  }
  return NULL;
}

/** Get a KPAGE by KPAGE */
struct frame * get_kernel_frame(void *kpage)
{
  struct list_elem *e;

  for (e = list_begin (&all_frames); e != list_end (&all_frames);
       e = list_next (e))
  {
    struct frame *f = list_entry (e, struct frame, elem);
    if(f->kpage == kpage)
      return f;
  }
  return NULL;
}

void* evict_frame(void )
{
//   printf("Evicting frame\n");
  struct list_elem *e = list_pop_front(&all_frames);
  struct frame *fp = list_entry(e, struct frame, elem);

  list_push_back(&all_frames, &fp->elem);
  push_to_swap(fp);

  struct thread *t = thread_current();
  pagedir_clear_page(t->pagedir, fp->upage->uaddr);

  return fp->kpage;
}

