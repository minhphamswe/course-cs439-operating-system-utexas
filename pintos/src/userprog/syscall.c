#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  printf ("system call!\n");

  printf("edi: %d\n", f->edi);
  printf("esi: %d\n", f->esi);
  printf("ebp: %d\n", f->ebp);
  printf("ebx: %d\n", f->ebx);
  printf("edx: %d\n", f->edx);
  printf("ecx: %d\n", f->ecx);
  printf("eax: %d\n", f->eax);
  printf("gs: %d\n", f->gs);
  printf("fs: %d\n", f->fs);
  printf("es: %d\n", f->es);
  printf("ds: %d\n", f->ds);

  printf("vec_no: %d\n", f->vec_no);
  printf("error_code: %d\n", f->error_code);
  printf("frame ptr: %x\n", (uint32_t) f->frame_pointer);
  printf("esp: %x\n", (uint32_t)f->esp);
  printf("ss: %d\n", f->ss);

  hex_dump(0, f->esp, 4 * sizeof(int), false);
  
  // Examine user memory to find out which system call gets called
  thread_exit ();
}



/* Read a byte at the user virtual address UADDR
 * UADDR must be below PHYS_BASE
 * Returns the byte value if successful, -1 if a segfault occurred.
 */
static int
get_user (const uint8_t *uaddr) {
  int result;
  asm ( "movl $1f, %0; movzbl %1, %0, 1:"
        : "=&a" (result)
        : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
 * UDST must be below PHYS_BASE
 * Returns true if successful, false if a segfault occurred.
 */
static bool
put_user (uint8_t *udst, uint8_t byte) {
  int error_code;
  asm ( "movl $1f, %0; movb %b2, %1; 1:"
        : "=&a" (error_code), "=m" (*udst)
        : "q" (byte));
  return error_code != -1;
}