/**
 * The supplemental page table supplements the page table with additional data
 * about each page. Since the page table's format allows it to only contain
 * very specific information (see section A.7 Page Table), the supplemental
 * page table contains supplemental information such as whether the page is
 * currently swapped or in the file system. In real life, such a data
 * structure is often called a "page table" also; we add the word
 * "supplemental" to reduce confusion.
 *
 * The supplemental page table is used for at least two purposes. Most
 * importantly, on a page fault, the kernel looks up the virtual page that
 * faulted in the supplemental page table to find out what data should be
 * there. Second, the kernel consults the supplemental page table when a
 * process terminates, to decide what resources to free.

You may organize the supplemental page table as you wish. There are at least
two basic approaches to its organization: in terms of segments or in terms of
pages. Optionally, you may use the page table itself as an index to track the
members of the supplemental page table. You will have to modify the Pintos page
table implementation in "pagedir.c" to do so. We recommend this approach for
advanced students only. See section A.7.4.2 Page Table Entry Format, for more
information.

The most important user of the supplemental page table is the page fault
handler. In project 2, a page fault always indicated a bug in the kernel or a
user program. In project 3, this is no longer true. Now, a page fault might
only indicate that the page must be brought in from a file or swap. You will
have to implement a more sophisticated page fault handler to handle these
cases. Your page fault handler, which you should implement by modifying
page_fault() in "userprog/exception.c", needs to do roughly the following:

  Locate the page that faulted in the supplemental page table. If the memory
  reference is valid, use the supplemental page table entry to locate the
  data that goes in the page, which might be in the file system, or in a swap
  slot, or it might simply be an all-zero page. If you implement sharing, the
  page's data might even already be in a page frame, but not in the page table.

  If the supplemental page table indicates that the user process should not
  expect any data at the address it was trying to access, or if the page lies
  within kernel virtual memory, or if the access is an attempt to write to a
  read-only page, then the access is invalid. Any invalid access terminates
  the process and thereby frees all of its resources.

  Obtain a frame to store the page. See section 4.1.5 Managing the Frame
  Table, for details.

  If you implement sharing, the data you need may already be in a frame, in
  which case you must be able to locate that frame.

  Fetch the data into the frame, by reading it from the file system or swap,
  zeroing it, etc.

  If you implement sharing, the page you need may already be in a frame, in
  which case no action is necessary in this step.

  Point the page table entry for the faulting virtual address to the physical
  page. You can use the functions in "userprog/pagedir.c". 
*/
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"

#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/interrupt.h"

#include "lib/debug.h"

#include <stdio.h>

static struct semaphore filesys_sema;   // Protect access to disks
static struct semaphore paging_sema;    // Protect page loading

void page_init(void ) {
  sema_init(&filesys_sema, 1);
  sema_init(&paging_sema, 1);
}

/** Initialize a page table */
void page_table_init(struct page_table *pt)
{
  list_init(&pt->pages);
}

/** Free all pages the page table points to. */
void page_table_destroy(struct page_table *pt)
{
  struct page_entry *entry;
  struct list_elem *e;

  enum intr_level old_level = intr_disable();
  while (!list_empty(&pt->pages)) {
    e = list_pop_front(&pt->pages);
    entry = list_entry(e, struct page_entry, elem);
    free_page_entry(entry);
  }
  intr_set_level(old_level);
}

/**
 * Obtain a page to track a user page (specified by an address).
 * If the address already belong to another address, and the address is not
 * in the ancestor's 
 */
struct page_entry* allocate_page(void* uaddr)
{
  struct thread *t = thread_current();
  struct page_entry *entry = get_page_entry(uaddr);

  if (entry != NULL) {
    // This page is allocated: check if it is ours 
    if (entry->tid != t->tid) {
      entry = NULL;
    }
  }
  else if (!is_user_vaddr(uaddr) || uaddr == NULL) {
    entry = NULL;
  }
  else {
    // Compute address of the nearest page
    uaddr = (((uint32_t) uaddr) / PGSIZE) * PGSIZE;

    // This address is free: make a new page entry to track it
    entry = malloc(sizeof(struct page_entry));
    ASSERT(entry != NULL);

    entry->tid = t->tid;
    entry->uaddr = uaddr;
    entry->writable = true;

    entry->frame = NULL;
    entry->swap = NULL;
    entry->file = NULL;

    entry->offset = 0;
    entry->read_bytes = 0;

    enum intr_level old_level = intr_disable();
    list_push_back(&(t->pages.pages), &entry->elem);
    intr_set_level(old_level);
  }
  return entry;
}

/// Called by page_table_destroy for each page entry to free it.
void free_page(void* uaddr)
{
  struct page_entry *entry = get_page_entry(uaddr);
  if (entry != NULL) {
    enum intr_level old_level = intr_disable();
    list_remove(&entry->elem);
    free_page_entry(entry);
    intr_set_level(old_level);
  }
}

bool load_page_entry(struct page_entry* entry) {
  sema_down(&paging_sema);
  bool success = false;

  if (entry != NULL) {
    ASSERT(!is_present(entry));
    // First get a free frame to put it in
    struct frame *fp = allocate_frame(entry);
    ASSERT(fp != NULL);

    // Pin the frame while loading data
    enum intr_level old_level = intr_disable();
    pin_frame(fp);
    intr_set_level(old_level);

    // Page is in swap: Read it in
    if (is_swapped(entry)) {
      // Page is swapped: swap it back into the free frame
      pull_from_swap(entry);
      success = install_frame(fp, entry->writable);
    }

    else if (is_in_fs(entry)) {
      // Page is in the file system: read it in
      if (entry->read_bytes > 0) {
        // Page must be read in
        file_seek(entry->file, entry->offset);
        sema_down(&filesys_sema);
        int bytes_read = file_read(entry->file, fp->kpage,
                                   entry->read_bytes);
        sema_up(&filesys_sema);
        if (bytes_read != entry->read_bytes) {
          free_frame(fp);
        }
        else {
          success = install_frame(fp, entry->writable);
        }
      }
      else {
        // Page doesn't need to be read in
        success = install_frame(fp, entry->writable);
      }
    }

    // Otherwise page was allocated by extending the stack: just install it
    else {
      success = install_frame(fp, entry->writable);
    }

    // Done, unpin frame
    old_level = intr_disable();
    if (fp != NULL)
      unpin_frame(fp);
    intr_set_level(old_level);
  }

  sema_up(&paging_sema);
  return success;
}


/// Bring a page belonging to this process into main memory from wherever
/// it is (e.g. swap). Return false if the address does not belong to the
/// process.
_Bool load_page(void* uaddr) {
  // Make sure it's supposed to be there
  bool success = false;
  struct page_entry *entry = get_page_entry(uaddr);
  struct thread *t = thread_current();

  if (entry != NULL) {
    success = load_page_entry(entry);
  }

  return success;
}

/// Look in the current thread's page table for page containing UADDR.
/// Return NULL if there is no such page.
struct page_entry* get_page_entry(void* uaddr)
{
  struct thread *t = thread_current();
  struct page_table *pt = &t->pages;
  struct list_elem *e;

  uaddr = (((uint32_t) uaddr) / PGSIZE) * PGSIZE;

  enum intr_level old_level = intr_disable();
  for (e = list_begin(&pt->pages); e != list_end(&pt->pages);
       e = list_next(e)) {
    struct page_entry *entry = list_entry(e, struct page_entry, elem);
    if (entry->uaddr == uaddr) {
      intr_set_level(old_level);
      return entry;
    }
  }
  intr_set_level(old_level);

  return NULL;
}

/// Free a page, and any frame it points to.
void free_page_entry(struct page_entry* entry)
{
  enum intr_level old_level = intr_disable();

  if (entry != NULL) {
    if (entry->frame != NULL) {
      // free the frames the entry points to
      free_frame(entry->frame);
    }
    if (entry->swap != NULL) {
      // free the swap slots the entry points to
      free_swap(entry->swap);
    }
    free(entry);
  }
  intr_set_level(old_level);
}

/// Return true if the page was loadded from the file system
bool is_in_fs(struct page_entry* entry) {
  ASSERT(entry != NULL);
  return (entry->file != NULL);
}

/// Return true if the page is currently in main memory
bool is_present(struct page_entry* entry) {
  ASSERT(entry != NULL);
  return (entry->frame != NULL);
}

/// Return true if the page is currently swapped out
bool is_swapped(struct page_entry* entry) {
  ASSERT(entry != NULL);
  return (entry->swap != NULL);
}
