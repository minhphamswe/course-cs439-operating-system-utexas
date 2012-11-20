#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "vm/frame.h"
#include "vm/swap.h"

#include "lib/kernel/list.h"
#include "filesys/filesys.h"

struct page_table {
  struct list pages;
};

struct page_entry {
  int tid;                  // ID of the owner of the page
  void *uaddr;              // User address
  bool writable;            // Whether the page is writable
  bool pinned;              // A pinned page cannot be evicted from main memory

  // Possible locations of the page
  struct frame *frame;      // Address of physical memory entry
  struct swap_slot *swap;   // Address of swap slot
  struct file* file;        // Address of file
  uint64_t offset;          // Offset of the page into the file
  uint32_t read_bytes;      // How many to read from the file starting at offset

  struct list_elem elem;    // List element for thread-based page table
};

void page_init(void);

// Page table operations
void page_table_init(struct page_table *pt);
void page_table_destroy(struct page_table* pt);
void page_table_print_safe(struct page_table *pt);
void page_table_print(struct page_table *pt);

// Page operations (for user and kernel processes)
struct page_entry* allocate_page(void* uaddr);
bool load_page(void *uaddr);
bool load_page_entry(struct page_entry *entry);


void free_page(void *uaddr);

// Page status query
bool is_pinned(struct page_entry *entry);
bool is_in_fs(struct page_entry *entry);
bool is_present(struct page_entry *entry);
bool is_swapped(struct page_entry *entry);

// Page entry operations (for VM internal operations)
struct page_entry* get_page_entry(void *uaddr);
void free_page_entry(struct page_entry *entry);


#endif /* vm/page.h */
