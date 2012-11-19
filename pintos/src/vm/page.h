#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "vm/frame.h"
#include "vm/swap.h"

#include "lib/kernel/list.h"
#include "filesys/filesys.h"

typedef enum {
  PAGE_NOT_EXIST = 0x01,
  PAGE_PRESENT = 0x02,
  PAGE_SWAPPED = 0x04,
  PAGE_IN_FILESYS = 0x08,
  PAGE_PINNED = 0x10,
} page_status;

struct page_table {
  struct list pages;
};

struct page_entry {
  int tid;                  // ID of the owner of the page
  void *uaddr;              // User address
  page_status status;       // Status of the page
  bool writable;            // Whether the page is writable

  // Possible locations of the page
  struct frame *frame;      // Address of physical memory entry
  struct swap_slot *swap;   // Address of swap slot
  struct file* file;        // Address of file
  uint32_t offset;          // Offset of the page into the file
  uint32_t read_bytes;      // How many to read from the file starting at offset

  struct list_elem elem;    // List element for thread-based page table
};

// Page table operations
void page_table_init(struct page_table *pt);
void page_table_destroy(struct page_table* pt);
void page_table_print_safe(struct page_table *pt);
void page_table_print(struct page_table *pt);

// Page operations (for user and kernel processes)
struct page_entry* allocate_page(void* uaddr);
bool install_page(struct page_entry *entry, int writable);

void free_page(void *uaddr);
bool load_page(void *uaddr);

// Page entry operations (for VM internal operations)
struct page_entry* get_page_entry(void *uaddr);
void free_page_entry(struct page_entry *entry);


#endif /* vm/page.h */
