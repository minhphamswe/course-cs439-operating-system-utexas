#ifndef VM_VM_H
#define VM_VM_H

#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

void vm_init()
{
  frame_init();
}

#endif
