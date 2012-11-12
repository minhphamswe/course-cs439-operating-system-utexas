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
#include <threads/malloc.h>
#include <threads/thread.h>

#include <stdio.h>

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

  while (!list_empty(&pt->pages)) {
    e = list_pop_front(&pt->pages);
    entry = list_entry(e, struct page_entry, elem);
    free_page_entry(entry);
  }
}

/** Allocate and track a user page. */
int allocate_page(void* uaddr)
{
//   printf("Allocating page\n");

  struct thread *t = thread_current();

  struct page_entry *entry = malloc(sizeof(struct page_entry));
  entry->uaddr = uaddr;

  int success = allocate_frame(entry, true);
  if (success) {
    // if page was successfully allocate, track frame
    list_push_back(&(t->pages.pages), &entry->elem);
  }
  else {
    // otherwise free it and don't bother
    free_page_entry(entry);
  }

  return success;
}

/** Return the status of the page a user address points to. */
page_status get_page_status(void* uaddr)
{
  struct page_entry *entry = get_page_entry(uaddr);
  if (entry)
    return entry->status;
  else
    return PAGE_NOT_EXIST;
}

_Bool set_page_status(void* uaddr, page_status status)
{
  struct page_entry *entry = get_page_entry(uaddr);
  if (entry)
    entry->status = status;
  return (entry != NULL);
}

/** */
void free_page(void* uaddr)
{
  struct page_entry *entry = get_page_entry(uaddr);
  if (entry)
    free_page_entry(entry);
}

/**
 * Look in the current thread's page table for page containing UADDR.
 * Return NULL if there is no such page.
 */
struct page_entry* get_page_entry(void* uaddr)
{
  struct thread *t = thread_current();
  struct page_table *pt = &t->pages;
  struct list_elem *e;

  for (e = list_begin(&pt->pages); e != list_end(&pt->pages);
       e = list_next(e)) {
    struct page_entry *entry = list_entry(e, struct page_entry, elem);
    if (entry->uaddr == uaddr)
      return entry;
  }

  return NULL;
}

/** Free a page, and any frame it points to. */
void free_page_entry(struct page_entry* entry)
{
  // TODO: free the frame the entry points to
  if (entry != NULL) {
    free(entry);
  }
}
