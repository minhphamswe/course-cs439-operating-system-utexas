#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "lib/kernel/list.h"

struct frame;
struct page_entry;

struct swap_slot {
  int tid;
  uint32_t sector;
  struct page_entry *upage;
  struct list_elem elem;
};

void swap_init(void);

bool push_to_swap(struct frame* fp);
bool pull_from_swap(struct page_entry* upage);

void free_swap(struct swap_slot* slot);

struct swap_slot* get_swapped_page(struct page_entry *upage);

bool clean_swap(int tid);

#endif /* vm/swap.h */
