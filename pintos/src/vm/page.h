#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "lib/kernel/list.h"

enum page_status {
  PAGE_PRESENT,
  PAGE_SWAPPED
};

struct page_table {
  struct list pages;
};

void page_table_init(struct page_table *pt);
void page_table_destroy(void);

int allocate_page(void *upage);
void free_page(void *upage);


#endif /* vm/page.h */
