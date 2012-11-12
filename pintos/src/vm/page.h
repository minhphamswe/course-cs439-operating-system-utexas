#ifndef VM_PAGE_H
#define VM_PAGE_H

struct page_table {

};

void page_table_init(struct page_table *pt);

int allocate_page(void *upage);
void free_page(void *upage);


#endif /* vm/page.h */
