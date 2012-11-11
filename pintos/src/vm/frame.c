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

#include "lib/kernel/list.h"

#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

#include "stdio.h"

static struct list all_frames;

void frame_init() {
  list_init(&all_frames);
}

/**
 * Allocate a new frame to hold the page pointed to by UPAGE, returning true
 * if success, and false if a page cannot be allocated.
 */
int allocate_frame(void* upage, int writable) {
  // Allocate frame and get its kernel page address 
  uint8_t *kpage;
  kpage = palloc_get_page (PAL_USER | PAL_ZERO);

  if (kpage == NULL) {
    //    printf("Can't allocate page, will need to do swaps.\n");
    return false;
  }
  else {
    // Page allocated
    struct thread *t = thread_current ();

    // Compute a valid user page address
//     printf("User address before: %x\n", (uint32_t) upage);
    upage = (((uint32_t) upage) / PGSIZE) * PGSIZE;
//     upage = ((((uint32_t) upage - (1)) / PGSIZE)) * PGSIZE;
//    printf("User address after: %x\n", (uint32_t) upage);

    if (pagedir_get_page (t->pagedir, upage) == NULL
        && pagedir_set_page (t->pagedir, upage, kpage, writable)) {
      // Page is not already allocated: map our page there.
      struct frame *fp = malloc(sizeof(struct frame));

      fp->upage = upage;
      fp->kpage = kpage;
      fp->writable = writable;
      list_push_back(&all_frames, &fp->elem);

      return true;
    }
    else {
      // Page is already allocated to some process: free frame & return false
      //printf("Page is already allocated to some process.\n");
      palloc_free_page (kpage);
      return false;
    }
  }
}

/** Remap the frame at KPAGE to contain the page pointed at by UPAGE.*/
void set_frame(void* upage, void* kpage, int writable)
{
  struct frame *fp = get_frame(upage);

  if (fp) {
    fp->upage = upage;
    fp->writable = writable;
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
struct frame * get_frame(void *upage)
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
