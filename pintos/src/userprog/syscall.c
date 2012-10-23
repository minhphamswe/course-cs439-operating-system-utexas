#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void syshalt_handler(struct intr_frame *f);
void sysexit_handler(struct intr_frame *f);
void sysexec_handler(struct intr_frame *f);
void syswait_handler(struct intr_frame *f);
void syscreate_handler(struct intr_frame *f);
void sysremove_handler(struct intr_frame *f);
void sysopen_handler(struct intr_frame *f);
void sysfilesize_handler(struct intr_frame *f);
void sysread_handler(struct intr_frame *f);
void syswrite_handler(struct intr_frame *f);
void sysseek_handler(struct intr_frame *f);
void systell_handler(struct intr_frame *f);
void sysclose_handler(struct intr_frame *f);

/* Read a byte at the user virtual address UADDR
 * UADDR must be below PHYS_BASE
 * Returns the byte value if successful, -1 if a segfault occurred.
 */
static int
get_user_byte (const uint8_t *uaddr) {
  int result;
  asm ( "movl $1f, %0; movzbl %1, %0; 1:"
        : "=&a" (result)
        : "m" (*uaddr));
  return result;
}

/* Read 4 bytes at the user virtual address UADDR
 * UADDR must be below PHYS_BASE
 * Returns the byte value if successful, -1 if a segfault occurred.
 */
static int
get_user (const uint32_t *uaddr) {
  int result;
  asm ( "movl $1f, %0; movzbl %1, %0; 1:"
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

/* Given an interrupt frame pointer, return the value pointed to by f->esp,
 * and increment f->esp.
 */
uint32_t pop_stack(struct intr_frame *f) {
  uint32_t ret = *(uint32_t *)f->esp;
  f->esp = (uint32_t *) f->esp + 1;
  printf("f->esp is: %x\n", (unsigned int)f->esp);
  return ret;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
 /*  printf("edi: %d\n", f->edi);
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
  printf("esp: %x\n", (uint32_t) f->esp);
  printf("ss: %d\n", f->ss);
  
  printf("Value at esp: %x\n", *((int *) f->esp));
*/
//   hex_dump(f->esp, f->esp, 16 * sizeof(int), false);
  printf("Eflags is: %d\n", f->eflags);
  printf("Address of f is: %x\n", (unsigned int)f);
  hex_dump(f->esp, f->esp, 0xc0000000 - (unsigned int)f->esp, false);

  // Examine user memory to find out which system call gets called
  int syscall_number = pop_stack(f);
  printf("Syscal Number is: %d\n", syscall_number);
  switch (syscall_number) {
    case SYS_HALT:
      printf("SYS_HALT Called\n");
      syshalt_handler(f);
      break;
    case SYS_EXIT:
      printf("SYS_EXIT Called\n");
      sysexit_handler(f);
      break;
    case SYS_EXEC:
      printf("SYS_EXEC Called\n");
      sysexec_handler(f);
      break;
    case SYS_WAIT:
      printf("SYS_WAIT Called\n");
      syswait_handler(f);
      break;
    case SYS_CREATE:
      printf("SYS_CREATE Called\n");
      syscreate_handler(f);
      break;
    case SYS_REMOVE:
      printf("SYS_REMOVE Called\n");
      sysremove_handler(f);
      break;
    case SYS_OPEN:
      printf("SYS_OPEN Called\n");
      sysopen_handler(f);
      break;
    case SYS_FILESIZE:
      printf("SYS_FILESIZE Called\n");
      sysfilesize_handler(f);
      break;
    case SYS_READ:
      printf("SYS_READ Called\n");
      sysread_handler(f);
      break;
    case SYS_WRITE:
      printf("SYS_WRITE Called\n");
      syswrite_handler(f);
      break;
    case SYS_SEEK:
      printf("SYS_SEEK Called\n");
      sysseek_handler(f);
      break;
    case SYS_TELL:
      printf("SYS_TELL Called\n");
      systell_handler(f);
      break;
    case SYS_CLOSE:
      printf("SYS_ClOSE Called\n");
      sysclose_handler(f);
      break;
  }

  printf("Preparing to jump...\n");
  printf("f->eip is: %x\n", (unsigned int) f->eip);
  hex_dump(f->esp, f->esp, 0xc0000000 - (unsigned int)f->esp, false);
//   f->eip = f->esp;
//   asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (f) : "memory");
//   NOT_REACHED ();
//   thread_exit ();
}


// Thread turns off system
void syshalt_handler(struct intr_frame *f)
{
  // Turn off pintos
  shutdown_power_off();
}

// Thread calls exit()
void sysexit_handler(struct intr_frame *f)
{
  // Get the exit return value and set it.
  int exitValue = pop_stack(f);
	thread_current()->retVal = exitValue;
  
  // End the currently running thread
  thread_exit();
}

// Thread calls sysexec
void sysexec_handler(struct intr_frame *f)
{
}

// Thread calls syswait
void syswait_handler(struct intr_frame *f)
{
	// Get PID from stack
	tid_t child = pop_stack(f);
  int status;
  status = process_wait(child);
	printf("TID status: %d\n", status);
}

// Thread calls syscreate
void syscreate_handler(struct intr_frame *f)
{
}

// Thread calls sysremove
void sysremove_handler(struct intr_frame *f)
{
}

// Threa calls sysopen
void sysopen_handler(struct intr_frame *f)
{
}

// Thread call sysfilesize
void sysfilesize_handler(struct intr_frame *f)
{
}

// Thread calls sysread
void sysread_handler(struct intr_frame *f)
{
}

// Thread calls syswrite
void syswrite_handler(struct intr_frame *f)
{
  // Get the number of the file descriptor to write buffer to
  uint32_t fdnum = pop_stack(f);

  // Get the address in UVAS of the buffer to write
  uint32_t buffer = pop_stack(f);

  // Lastly, get the size of the buffer to write
  uint32_t bufferSize = pop_stack(f);

  // Check to see if it's a console out, and print if yes
  if(fdnum == 1) {
    printf("Printing to console\n");
//     putbuf((char *) buffer, bufferSize);
//	hex_dump(0, f->esp, 8 * sizeof(int), false);  
   }
  else {
    printf("Printing to file\n");
    // Write to file.  NEED TO IMPLEMENT
  }
  printf("Exiting Syswrite Handler\n");
}

// Thread calls sysseek
void sysseek_handler(struct intr_frame *f)
{
}

// Thread calls systell
void systell_handler(struct intr_frame *f)
{
}

// Thread calls sysclose
void sysclose_handler(struct intr_frame *f)
{
}