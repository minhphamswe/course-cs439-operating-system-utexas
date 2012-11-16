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
#include <threads/malloc.h>
#include <threads/thread.h>

#include "threads/vaddr.h"
#include <threads/interrupt.h>
#include <userprog/pagedir.h>

#include <stdio.h>

/** Initialize a page table */
void page_table_init(struct page_table *pt)
{
  list_init(&pt->pages);
}

/** Free all pages the page table points to. */
void page_table_destroy(struct page_table *pt)
{
//   printf("Destroying page table\n");
  struct page_entry *entry;
  struct list_elem *e;

  while (!list_empty(&pt->pages)) {
    e = list_pop_front(&pt->pages);
    entry = list_entry(e, struct page_entry, elem);
    free_page_entry(entry);
  }
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

  if (entry) {
    // This page is allocated: check if it is ours 
    // TODO: or our ancestor's code segment
    if (entry->tid != t->tid) {
//       printf("Page does not belong to the running process. TODO: check if it's parent's code segment.\n");
      entry = NULL;
    }
  }
  else if (!is_user_vaddr(uaddr) || uaddr == NULL) {
//     printf("Address is not user address\n");
    entry = NULL;
  }
  else {
//     printf("Allocating new page at user address: %x\n", uaddr);
//     printf("Writable: %d\n", writable);
    // Compute address of the nearest page
    uaddr = (((uint32_t) uaddr) / PGSIZE) * PGSIZE;

    // This address is free: make a new page entry to track it
    entry = malloc(sizeof(struct page_entry));
    entry->uaddr = uaddr;
    entry->tid = t->tid;

    // Map it the new page entry to a frame
    struct frame* fp = allocate_frame(entry);

    enum intr_level old_level = intr_disable();
    if (fp) {
//       printf("Got here.\n");
      // if frame was successfully allocated, track page
      list_push_back(&(t->pages.pages), &entry->elem);
    }
    else {
      // otherwise free it and don't bother
      free_page_entry(entry);
      entry = NULL;
    }
    intr_set_level(old_level);
  }

//   printf("allocate_page() returning is: %x\n", entry);

  return entry;
}

bool install_page(struct page_entry* entry, int writable)
{
  if (entry == NULL) {
//     printf("install_page(): Null page entry, returning false\n");
    return false;
  }
  else {
    bool success = install_frame(entry->frame, writable);
//     if (!success)
//       free_page_entry(entry);    // FIXME: this causes a triple fault
//     printf("install_page(): success is: %d\n", success);
    return success;
  }
}

/** Return the status of the page a user address points to. */
page_status get_page_status(void* uaddr)
{
//   printf("Getting page status\n");
  struct page_entry *entry = get_page_entry(uaddr);
  if (entry)
    return entry->status;
  else
    return PAGE_NOT_EXIST;
}

_Bool set_page_status(void* uaddr, page_status status)
{
//   printf("Setting page status\n");
  struct page_entry *entry = get_page_entry(uaddr);
  if (entry)
    entry->status = status;
  return (entry != NULL);
}

/** Called by page_table_destroy for each page entry to free it. */
void free_page(void* uaddr)
{
//   printf("Freeing page\n");
//   struct thread *t = thread_current();
//   struct page_table *pt = &t->pages;
  struct page_entry *entry = get_page_entry(uaddr);
  if (entry) {
    list_remove(&entry->elem);
    free_page_entry(entry);
  }
}

/**
 * Bring a page belonging to this process into main memory from wherever
 * it is (e.g. swap). Return false if the address does not belong to the
 * process.
 */
_Bool load_page(void* uaddr)
{
  // Make sure it's supposed to be there
  struct page_entry *entry = get_page_entry(uaddr);
  if (entry == NULL) {
    return false;
  }

  struct thread *t = thread_current();
//   printf("load_page(): Loading thread %x(%d) | page %x (%x) @ %x(%d)\n", t, t->tid, entry->uaddr, uaddr, entry, entry->tid);
//   printf("Page status: %d\n", entry->status);

  // If it's swapped, let's go get it
  if (entry->status == PAGE_SWAPPED) {
    // First need to get a free frame to put it in
    struct frame * fp = allocate_frame(entry);
    int success = install_frame(fp, true);

    if (!success)
      return false;   // out of swap space, should be panic'd before here

    // Now swap back into free frame
//     get_from_swap(entry->frame, uaddr);
    pull_from_swap(entry);
    ASSERT(pagedir_get_page(thread_current()->pagedir, entry->uaddr) != NULL);
    return true;
  }

  return false;
}

/**
 * Look in the current thread's page table for page containing UADDR.
 * Return NULL if there is no such page.
 */
struct page_entry* get_page_entry(void* uaddr)
{
//   printf("Getting page entry\n");
  struct thread *t = thread_current();
  struct page_table *pt = &t->pages;
  struct list_elem *e;

  uaddr = (((uint32_t) uaddr) / PGSIZE) * PGSIZE;

  for (e = list_begin(&pt->pages); e != list_end(&pt->pages);
       e = list_next(e)) {
    struct page_entry *entry = list_entry(e, struct page_entry, elem);
    if (entry->uaddr == uaddr) {
//       printf("get_page_entry(): Get thread %x(%d)| page %x @ %x(%d) \n", t, t->tid, uaddr, entry, entry->tid);
      return entry;
    }
  }

//   printf("get_page_entry(): Get thread %x(%d) | page %x @ NULL \n", t, t->tid, uaddr);
  return NULL;
}

/** Free a page, and any frame it points to. */
void free_page_entry(struct page_entry* entry)
{
//   printf("Freeing page entry\n");
  // TODO: free the frame the entry points to
  if (entry != NULL) {
    if (entry->frame != NULL) {
      free_frame(entry->frame);
      free(entry->frame);
    }
    free(entry);
  }
}
