#ifndef VM_VM_H
#define VM_VM_H

#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

/// Initialize all VM subsystems that need initialization
void vm_init(void)
{
  frame_init();
  swap_init();
}

#endif
