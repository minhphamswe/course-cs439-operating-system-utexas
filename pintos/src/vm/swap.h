#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/frame.h"

void swap_init(void);

bool push_to_swap(struct frame* fp);
bool pull_from_swap(struct page_entry* upage);

struct swap_slot* get_slot_by_vaddr(void* uaddr);
struct swap_slot* get_swapped_page(struct page_entry *upage);
bool get_from_swap(struct frame* fp, void* uaddr);

bool clean_swap(int tid);

#endif /* vm/swap.h */
