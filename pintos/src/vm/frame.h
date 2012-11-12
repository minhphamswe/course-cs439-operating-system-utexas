#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "lib/kernel/list.h"
#include "vm/page.h"
#include "page.h"

struct frame {
  struct page_entry *upage;   // User page
  void* kpage;                // Kernel page = Physical address
  int writable;               // Whether the frame should be writable
  struct list_elem elem;
};

void frame_init(void);

int allocate_frame(struct page_entry *upage, int writable);

void set_frame(struct page_entry *upage, void* kpage, int writable);
void unset_frame(struct frame *fp);

struct frame * get_frame(struct page_entry *upage);
struct frame * get_kernel_frame(void *kpage);

void* evict_frame(void);

#endif /* vm/frame.h */
