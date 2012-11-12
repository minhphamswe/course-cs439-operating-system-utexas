#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "lib/kernel/list.h"

struct frame {
  void* upage;		  // User space address
  void* kpage;		  // Physical address
  int writable;    // Whether the frame should be writable
  struct list_elem elem;
};

void frame_init(void);

int allocate_frame(void* upage, int writeble);

void set_frame(void* upage, void* kpage, int writable);
void unset_frame(struct frame *fp);

struct frame * get_frame(void *upage);
struct frame * get_kernel_frame(void *kpage);

void evict_frame();

#endif /* vm/frame.h */
