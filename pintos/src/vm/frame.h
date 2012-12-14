#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "lib/kernel/list.h"
#include "vm/page.h"
#include "page.h"

struct frame {
  int tid;
  struct page_entry *upage;   // User page
  void* kpage;                // Kernel page = Physical address
  bool pinned;                // A pinned frame cannot be evicted
  bool async_write;           // If true, must be written to swap on evict
  struct list_elem elem;
};

void frame_init(void);

struct frame* allocate_frame(struct page_entry* upage);
bool install_frame(struct frame *fp, int writable);
void free_frame(struct frame *fp);

void pin_frame(struct frame *fp);
void unpin_frame(struct frame *fp);

#endif /* vm/frame.h */
