#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/frame.h"

void swap_init(void);

bool push_to_swap(struct frame* fp);
bool pull_from_swap(struct frame* fp);

struct swap_slot* get_slot_by_vaddr(void* uaddr);
bool get_from_swap(struct frame* fp, void* uaddr);

bool clean_swap(int tid);

#endif /* vm/swap.h */
