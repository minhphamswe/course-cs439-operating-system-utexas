#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/list.h"
#include "vm/frame.h"

typedef enum {
  PAGE_NOT_EXIST = -1,
  PAGE_PRESENT,
  PAGE_SWAPPED
} page_status;

struct page_table {
  struct list pages;
};

struct page_entry {
  int tid;                  // ID of the owner of the page
  void *uaddr;              // User address
  page_status status;       // Status of the page
  bool writable;            // Whether the page is writable
  struct frame *frame;      // Frame table entry address
  struct list_elem elem;    // List element for thread-based page table
};

// Page table operations
void page_table_init(struct page_table *pt);
void page_table_destroy(struct page_table* pt);

// Page operations (for user and kernel processes)
struct page_entry* allocate_page(void* uaddr);
bool install_page(struct page_entry *entry, int writable);

void free_page(void *uaddr);
bool load_page(void *uaddr);

// Page entry operations (for VM internal operations)
struct page_entry* get_page_entry(void *uaddr);
void free_page_entry(struct page_entry *entry);


#endif /* vm/page.h */
