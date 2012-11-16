#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "lib/kernel/list.h"
#include "vm/page.h"
#include "page.h"

struct frame {
  int tid;
  struct page_entry *upage;   // User page
  void* kpage;                // Kernel page = Physical address
  struct list_elem elem;
};

void frame_init(void);

struct frame* allocate_frame(struct page_entry* upage);
bool install_frame(struct frame *fp, int writable);
void free_frame(struct frame *fp);

struct frame* evict_frame(void);

#endif /* vm/frame.h */
