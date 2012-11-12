#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "vm/frame.h"

void swap_init(void);

void push_to_swap(struct frame* fp);
void pull_from_swap(struct frame* fp);

#endif /* vm/swap.h */
