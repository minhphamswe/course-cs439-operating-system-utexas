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
#include "lib/debug.h"

#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/interrupt.h"

#include "stdio.h"

static struct list all_frames;          // all allocated frames in the system
static struct list_elem *clockhand;     // the eviction clock hand

struct frame* evict_frame(void);

/*================== METHODS TO QUERY/SET STATUS OF FRAME =================*/
inline bool is_free(struct frame *fp);
inline bool is_dirty(struct frame *fp);
inline bool is_accessed(struct frame *fp);
inline void set_dirty(struct frame *fp, bool dirty);
inline void set_accessed(struct frame *fp, bool accessed);
inline bool is_readonly(struct frame *fp);
inline bool is_pinned(struct frame* fp);

/// Return true if the frame is not currently in use
inline bool is_free(struct frame *fp) {
  ASSERT(fp != NULL);
  return (fp->upage == NULL);
}

/// Return true if the dirty bit for the frame is set
inline bool is_dirty(struct frame *fp) {
  ASSERT(fp != NULL);
  ASSERT(!is_free(fp));
  return (pagedir_is_dirty(thread_current()->pagedir, fp->upage->uaddr) ||
          pagedir_is_dirty(thread_current()->pagedir, fp->kpage));
}

/// Return true if the access bit for the frame is set
inline bool is_accessed(struct frame *fp) {
  ASSERT(fp != NULL);
  ASSERT(!is_free(fp));
  return (pagedir_is_accessed(thread_current()->pagedir, fp->upage->uaddr) ||
          pagedir_is_accessed(thread_current()->pagedir, fp->kpage));
}

/// Set the dirty bit for the frame
inline void set_dirty(struct frame *fp, bool dirty) {
  ASSERT(fp != NULL);
  ASSERT(!is_free(fp));
  pagedir_set_dirty(thread_current()->pagedir, fp->upage->uaddr, dirty);
  pagedir_set_dirty(thread_current()->pagedir, fp->kpage, dirty);
}

/// Set the access bit for the frame
inline void set_accessed(struct frame *fp, bool accessed) {
  ASSERT(fp != NULL);
  ASSERT(!is_free(fp));
  pagedir_set_accessed(thread_current()->pagedir, fp->upage->uaddr, accessed);
  pagedir_set_accessed(thread_current()->pagedir, fp->kpage, accessed);
}

/// Return true if the frame cannot be written to
inline bool is_readonly(struct frame *fp) {
  ASSERT(fp != NULL);
  ASSERT(!is_free(fp));
  return (fp->upage->writable == false);
}

/// Return true if the frame is pinned, thus not evictable
inline bool is_pinned(struct frame* fp) {
  ASSERT(fp != NULL);
  return fp->pinned;
}

/// Mark the frame pinned, i.e. not evictable
void pin_frame(struct frame* fp) {
  ASSERT(fp != NULL);
  fp->pinned = true;
}

/// Mark the frame as evictable
void unpin_frame(struct frame* fp) {
  ASSERT(fp != NULL);
  fp->pinned = false;
}

/*=========================================================================*/

/// Initialize the frame table system
void frame_init(void) {
  list_init(&all_frames);
}

/// Obtain a new frame to hold the page pointed to by UPAGE
/// If there is no more space in memory, evict a frame
struct frame* allocate_frame(struct page_entry* upage) {
  // Allocate frame and get its kernel page address
 printf("Start allocate_frame(%x)\n", upage);
  struct frame *fp;
  uint8_t *kpage;
  void * uaddr;

  // Compute a page-aligned user page address
  uaddr = upage->uaddr;
  uaddr = (void*)((((uint32_t) uaddr) / PGSIZE) * PGSIZE);

  // Attempt to allocate a new frame
  printf("allocate_frame(%x): Trace 1\n", upage);
  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  printf("allocate_frame(%x): Trace 2\n", upage);
  if (kpage == NULL) {
    // All physical addresses are used: evict a frame
    printf("allocate_frame(%x): Trace 3\n", upage);
    fp = evict_frame();
    printf("allocate_frame(%x): Trace 4\n", upage);
    kpage = fp->kpage;
    memset(kpage, 0, PGSIZE);
  }
  else {
    printf("allocate_frame(%x): Trace 5\n", upage);
    // We have space: register a new frame to track the address
    fp = malloc(sizeof(struct frame));
    enum intr_level old_level = intr_disable();
    list_push_back(&all_frames, &fp->elem);
    intr_set_level(old_level);
    printf("allocate_frame(%x): Trace 6\n", upage);
  }

  printf("allocate_frame(%x): Trace 7\n", upage);
  // Set frame attributes
  fp->upage = upage;
  fp->kpage = kpage;
  fp->tid = upage->tid;
  fp->pinned = false;
  fp->async_write = false;
  printf("allocate_frame(%x): Trace 8\n", upage);

  // Update page entry attributes
  upage->frame = fp;

  set_dirty(fp, false);
  printf("allocate_frame(%x): Trace 9\n", upage);

 printf("End allocate_frame(%x): Frame: %x, Upage: %x, Kpage: %x\n", upage, fp, fp->upage, fp->kpage);
  return fp;
}

bool install_frame(struct frame* fp, int writable) {
  printf("Start install_frame(%x, %d)\n", fp, writable);
  printf("install_frame(%x, %d): Frame: %x, Page: %x\n", fp, writable, fp, fp->upage);
  bool success = false;

  printf("install_frame(%x, %d): Trace 1\n", fp, writable);
  if (fp != NULL && fp->upage != NULL) {
    printf("install_frame(%x, %d): Trace 2\n", fp, writable);
    struct thread *t = thread_current();
    void *uaddr = fp->upage->uaddr;
    void *kpage = fp->kpage;

    if ((pagedir_get_page(t->pagedir, uaddr) == NULL) && 
        (pagedir_set_page(t->pagedir, uaddr, kpage, writable))) {
      printf("install_frame(%x, %d): Mapped thread %x(%d) | page %x -> %x @ %x(%d) -> %x(%d)\n", fp, writable, t, t->tid, uaddr, kpage, fp->upage, fp->upage->tid, fp, fp->tid);
      success = true;
      ASSERT(pagedir_get_page(t->pagedir, uaddr) != NULL);
      ASSERT(is_present(fp->upage));
      printf("install_frame(%x, %d): Trace 3\n", fp, writable);
    }
    else {
      printf("install_frame(%x, %d): Trace 4\n", fp, writable);
      free_frame(fp);
      ASSERT(pagedir_get_page(t->pagedir, uaddr) == NULL);
      ASSERT(!is_present(fp->upage));
      printf("install_frame(%x, %d): Trace 5\n", fp, writable);
    }
  }

  printf("install_frame(%x, %d): Trace 6\n", fp, writable);
  return success;
}

void free_frame(struct frame* fp)
{
  // Remove page->frame mapping from the CPU-based page directory
  if (fp != NULL) {
    if (fp->upage != NULL) {
      struct thread *t = thread_by_tid(fp->tid);
      void *uaddr = fp->upage->uaddr;

//       if (pagedir_get_page(t->pagedir, uaddr) != NULL) {
        // Page is in memory and registered to this thread
      printf("free_frame(): Unmapping thread %x(%d) | page %x -/-> %x @ %x(%d) -/-> %x(%d)\n", t, t->tid, uaddr, fp->kpage, fp->upage, fp->upage->tid, fp, fp->tid);
      pagedir_clear_page(t->pagedir, uaddr);
//       }

      // Remove page<->frame mapping from our supplemental structures
      struct page_entry *entry = fp->upage;
      ASSERT(entry != NULL);
      
      fp->upage->frame = NULL;
      fp->upage = NULL;

      ASSERT(pagedir_get_page(t->pagedir, entry->uaddr) == NULL);
      ASSERT(!is_present(entry));
    }
    enum intr_level old_level = intr_disable();
    palloc_free_page(fp->kpage);
    list_remove(&fp->elem);
    free(fp);
    intr_set_level(old_level);
  }
}

struct frame* evict_frame(void)
{
  printf("Start evict_frame()\n");
  enum intr_level old_level = intr_disable();
  if (clockhand == NULL || clockhand == list_end(&all_frames))
    clockhand = list_begin(&all_frames);
  intr_set_level(old_level);

  int revolution = 0;   // number of revolutions of the clockhand
  bool found = false;   // true if we have found an eviction target
  struct frame *fp = NULL;
  struct list_elem *old_clockhand = clockhand;  // saved clockhand position

  while (revolution < 3 && !found) {
    ASSERT(clockhand->next != clockhand);
    ASSERT(clockhand != list_end(&all_frames));
    // Get the frame structure
    old_level = intr_disable();
    fp = list_entry(clockhand, struct frame, elem);
    intr_set_level(old_level);

    // Check if the frame is suitable for eviction
    if (revolution == 0) {
      // first time through
      found = (!is_pinned(fp) &&
               (is_free(fp) ||
                is_readonly(fp) ||
                (!is_accessed(fp) && !is_dirty(fp))));
    }
    else if (revolution == 1) {
      // second time through
      found = (!is_pinned(fp) &&
               (is_free(fp) ||
                is_readonly(fp) ||
                (!is_dirty(fp))));
    }
    else {
      // third time through
      found = !is_pinned(fp);
    }

    // Set accessed to false at every frame
    set_accessed(fp, false);

    // Reset dirty bit (remembering to write later if evicted)
    if (is_dirty(fp)) {
      set_dirty(fp, false);
      fp->async_write = true;
    }

//     printf("\nclockhand: %x, free: %d, readonly: %d, accessed: %d, dirty: %d, pinned: %d, revolution: %d, found: %d", clockhand, is_free(fp), is_readonly(fp), is_accessed(fp), is_dirty(fp), is_pinned(fp), revolution, found);
//     printf("\nclockhand: %x, clockhand->next: %x, frame: %x, old_clockhand: %x, revolution: %d, found: %d", clockhand, clockhand->next, fp, old_clockhand, revolution, found);

    // Advance clockhand
    old_level = intr_disable();
    clockhand = list_next(clockhand);
    if (clockhand == list_end(&all_frames))
      clockhand = list_begin(&all_frames);
    intr_set_level(old_level);

    // Increment revolution if we passed the saved position
    if (clockhand == old_clockhand)
      revolution++;
  }
  ASSERT(found);
  ASSERT(fp != NULL);
  ASSERT(!is_pinned(fp));

  // Write to swap space if the frame is dirty
  if (fp->upage != NULL) {
    if (is_dirty(fp) || fp->async_write) {
      push_to_swap(fp);
    }

  // Unmap frame if the frame is being used
    old_level = intr_disable();
    struct thread *victim = thread_by_tid(fp->tid);
    pagedir_clear_page(victim->pagedir, fp->upage->uaddr);
    printf("evict_frame(): Unmapping thread %x(%d) | page %x -/-> %x @ %x(%d) -/-> %x(%d)\n", thread_current(), thread_current()->tid, fp->upage->uaddr, fp->kpage, fp->upage, fp->upage->tid, fp, fp->tid);

    struct page_entry *entry = fp->upage;

    fp->upage->frame = NULL;
    fp->upage = NULL;

    ASSERT(pagedir_get_page(thread_current()->pagedir, entry->uaddr) == NULL);
    ASSERT(!is_present(entry));
    intr_set_level(old_level);
  }

  printf("evict_frame(): Returning thread %x(%d) | page %x -/-> %x @ %x(%d) -/-> %x(%d)\n", thread_current(), thread_current()->tid, fp->upage->uaddr, fp->kpage, fp->upage, fp->upage->tid, fp, fp->tid);
//   printf("Frame evicted: %x\n", fp);
  return fp;
}

