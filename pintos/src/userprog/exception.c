#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "lib/debug.h"
#ifdef VM
  #include "vm/frame.h"
  #include "vm/page.h"
  #include "vm/swap.h"
#endif

/* Number of page faults processed. */
static long long page_fault_cnt;

static struct semaphore extend_sema;    // Protect stack extension
static struct semaphore fault_sema;     // Protect page fault handler

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);
#ifdef VM
static void extend_stack (struct intr_frame *, void *fault_addr);
#endif
static void kill_process (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");

  sema_init(&extend_sema, 1);
  sema_init(&fault_sema, 1);
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

#ifdef VM
  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  if (not_present && write && !user) {
    // System write: try to allocate more space if page not owned
    if (!load_page(fault_addr)) {
      extend_stack(f, fault_addr);
    }
  }
  else if (not_present && write && user) {
    // User write: try to allocate more space if page not owned
    if (!load_page(fault_addr)) {
      extend_stack(f, fault_addr);
    }
  }
  else if (not_present && !write && !user) {
    // System read: kill if page not owned
    if (!load_page(fault_addr)) {
      kill_process(f);
    }
  }
  else if (not_present && !write && user) {
    // User read: kill if page not owned
    if (!load_page(fault_addr)) {
      kill_process(f);
    }
  }
  else if (!not_present && !write && user) {
    // User read access violation: kill if page not owned
    if (!load_page(fault_addr)) {
      kill_process(f);
    }
  }
  else if (!not_present && write && user) {
    // User write access violation: kill unconditionally
    kill_process(f);
  }
  else {
    // Illegal access: kill process
    kill_process(f);
  }
#else
  kill_process(f);
#endif
}

#ifdef VM
/// Extend the stack from the faulting addres up to the base pointer of the faulting thread
/// Called by page_fault
static void
extend_stack (struct intr_frame *f, void *fault_addr) {
  ASSERT(f != NULL);
  ASSERT(f->frame_pointer != NULL);
  uint32_t bottom = (((uint32_t) fault_addr) / PGSIZE) * PGSIZE;
  uint32_t top = (((uint32_t) f->frame_pointer) / PGSIZE) * PGSIZE;
  uint32_t addr;
  bool success = false;

  // Only one process should extend the stack at a time
  sema_down(&extend_sema);

  for (addr = bottom; addr < top ; addr += PGSIZE) {
    // Obtain the page associated with the faulting address
    struct page_entry *entry = allocate_page((void*) addr);

    if (!load_page_entry(entry)) {
      break;
    }
    success = true;
  }
  sema_up(&extend_sema);

  // If the extension fails, this is an access violation: kill process
  if (!success) {
    kill_process(f);
  }
}
#endif

/// Kill a process
static void
kill_process (struct intr_frame *f UNUSED)
{
  thread_set_exit_status(thread_current()->tid, -1);
  thread_exit();
}
