#ifndef VM_SWAP_H
#define VM_SWAP_H

void swap_init(void);

void push_to_swap(void *frame);
void pull_from_swap(void *frame);

#endif /* vm/swap.h */
