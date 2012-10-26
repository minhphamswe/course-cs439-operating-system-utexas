#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "lib/kernel/list.h"

static void syscall_handler (struct intr_frame *);

void syshalt_handler (struct intr_frame *f);
void sysexit_handler (struct intr_frame *f);
void sysexec_handler (struct intr_frame *f);
void syswait_handler (struct intr_frame *f);
void syscreate_handler (struct intr_frame *f);
void sysremove_handler (struct intr_frame *f);
void sysopen_handler (struct intr_frame *f);
void sysfilesize_handler (struct intr_frame *f);
void sysread_handler (struct intr_frame *f);
void syswrite_handler (struct intr_frame *f);
void sysseek_handler (struct intr_frame *f);
void systell_handler (struct intr_frame *f);
void sysclose_handler (struct intr_frame *f);

struct fileHandle* get_handle (int fd);
void terminate_thread();

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
  asm ( "movl $1f, %0; movl %1, %0; 1:"
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
uint32_t pop_stack(struct intr_frame *f)
{
  // Check if the address is below PHYS_BASE
  if (((uint32_t) f->esp < (uint32_t) PHYS_BASE) && f->esp != NULL) {
    uint32_t ret = *(uint32_t *)f->esp;
    f->esp = (uint32_t *) f->esp + 1;
    //printf("f->esp is: %x\n", (unsigned int)f->esp);
    return ret;
  }
  // Otherwise, bad access.  Terminate gracefully
  else {
    terminate_thread();
    return 0;
  }
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

  uint32_t tmpesp = f->esp;
  //printf("Eflags is: %d\n", f->eflags);
  //printf("Address of f is: %x\n", (unsigned int)f);
  //hex_dump(f->esp, f->esp, 0xc0000000 - (unsigned int)f->esp, false);

  // Examine user memory to find out which system call gets called
  int syscall_number = pop_stack(f);
  //printf("Syscal Number is: %d\n", syscall_number);
  switch (syscall_number) {
    case SYS_HALT:
      //printf("SYS_HALT Called\n");
      syshalt_handler(f);
      break;
    case SYS_EXIT:
      //printf("SYS_EXIT Called\n");
      sysexit_handler(f);
      break;
    case SYS_EXEC:
      //printf("SYS_EXEC Called\n");
      sysexec_handler(f);
      break;
    case SYS_WAIT:
      //printf("SYS_WAIT Called\n");
      syswait_handler(f);
      break;
    case SYS_CREATE:
      //printf("SYS_CREATE Called\n");
      syscreate_handler(f);
      break;
    case SYS_REMOVE:
      //printf("SYS_REMOVE Called\n");
      sysremove_handler(f);
      break;
    case SYS_OPEN:
      //printf("SYS_OPEN Called\n");
      sysopen_handler(f);
      break;
    case SYS_FILESIZE:
      //printf("SYS_FILESIZE Called\n");
      sysfilesize_handler(f);
      break;
    case SYS_READ:
      //printf("SYS_READ Called\n");
      sysread_handler(f);
      break;
    case SYS_WRITE:
      //printf("SYS_WRITE Called\n");
      syswrite_handler(f);
      break;
    case SYS_SEEK:
      //printf("SYS_SEEK Called\n");
      sysseek_handler(f);
      break;
    case SYS_TELL:
      //printf("SYS_TELL Called\n");
      systell_handler(f);
      break;
    case SYS_CLOSE:
      //printf("SYS_ClOSE Called\n");
      sysclose_handler(f);
      break;
  }

  //printf("Preparing to jump...\n");
  //printf("f->eip is: %x\n", (unsigned int) f->eip);
  //hex_dump(f->esp, f->esp, 0xc0000000 - (unsigned int)f->esp, false);
  f->esp = tmpesp;
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
  int exitValue = (int) pop_stack(f);
  thread_current()->retVal = exitValue;

  // End the currently running thread
  thread_exit();
}

// Thread calls sysexec
void sysexec_handler(struct intr_frame *f)
{
  char *cmdline = pop_stack(f);
  tid_t newtid;
  //printf("Command line: %s\n", cmdline);
  //printf("(%s) run\n", cmdline);

  newtid = process_execute(cmdline);
  f->eax = newtid;
}

// Thread calls syswait
void syswait_handler(struct intr_frame *f)
{
  // Get PID from stack
  tid_t child = pop_stack(f);
  f->eax = process_wait(child);
  //printf("TID status: %d\n", status);
}

// Thread calls syscreate
void syscreate_handler(struct intr_frame *f)
{
  // Get file name and size from stack
  char *filename = pop_stack(f);
  uint32_t filesize = pop_stack(f);

  if (filename == NULL || filesize < 0)
    terminate_thread();

  int status = 0;
  status = filesys_create(filename, filesize);

  // Return status
  f->eax = status;
}

// Thread calls sysremove
void sysremove_handler(struct intr_frame *f)
{
  // Get file name from stack
  char *filename = pop_stack(f);

  int status = 0;
  status = filesys_remove(filename);

  // Return status
  f->eax = status;
}

// Thread calls sysopen
void sysopen_handler(struct intr_frame *f)
{
  // Get file name
  char *file_name = pop_stack(f);

  if(file_name == NULL) {
    terminate_thread();
  }

  // Open file
  struct file *file = filesys_open(file_name);

  // File cannot be opened: return -1
  if (file == NULL) {
    f->eax = -1;
  }
  // File is opened: put it on the list of open file handles
  else {
    struct thread *t = thread_current();

    struct fileHandle *newFile = (struct fileHandle*) malloc(sizeof(struct fileHandle));
    newFile->file = file;
    //newFile->node = file->inode;
    newFile->fd = t->nextFD++;
    //strlcpy(newFile->name, file_name, 16);

    list_push_back(&t->handles, &newFile->fileElem);

    // return the file descriptor
    f->eax = newFile->fd;
  }
}

// Thread call sysfilesize
void sysfilesize_handler(struct intr_frame *f)
{
  // Get fd from stack
  int fd = (int) pop_stack(f);

  struct thread *tp = thread_current();
  struct list_elem *e;

  for (e = list_begin(&tp->handles); e != list_end(&tp->handles);
      e = list_next(e)) {
    struct fileHandle *fhp = list_entry(e, struct fileHandle, fileElem);
    if (fhp->fd == fd) {
      f->eax = file_length(fhp->file);
      return;
    }
  }

  f->eax -1;
}

// Thread calls sysread
void sysread_handler(struct intr_frame *f)
{
  // Get arguments from the stack
  int fd = (int) pop_stack(f);
  void *buffer = (void*) pop_stack(f);
  off_t size = pop_stack(f);

  // Number of characters read
  int ret = 0;

  // Check to make sure buffer is in user space
  if (buffer > PHYS_BASE || get_user(buffer) == -1) {
    terminate_thread();
  }

  // fd is 0: Read from stdin
  if (fd == 0) {
    *(char*) buffer = input_getc();
    f->eax = 1;
    return;
  }
  else if (fd == 1 || size < 0) {
    f->eax = -1;
    return;
  }
  else if (size == 0) {
    f->eax = 0;
    return;
  }
  else {
    struct thread *tp = thread_current();
    struct list_elem *e;

    // Search for file descriptor in the thread open-file handles
    for (e = list_begin(&tp->handles); e != list_end(&tp->handles);
        e = list_next(e)) {
      struct fileHandle *fhp = list_entry(e, struct fileHandle, fileElem);
      // File descriptor found: read file
      if (fhp->fd == fd) {
        f->eax = file_read(fhp->file, buffer, size);
        return;
      }
    }
    // File descriptor not found: return -1
    f->eax = -1;
  }
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
    putbuf((char *) buffer, bufferSize);
    f->eax = bufferSize;
  }
  // Not to console, so print to file
  else if(fdnum > 1){
    struct fileHandle *fhp = get_handle(fdnum);

    // Is the file currently executing?
//    if(is_executing(&fhp->name) == true)
//      f->eax = -1;
    if (fhp != NULL)
      f->eax = file_write(fhp->file, buffer, bufferSize);
    else
      f->eax = -1;
  }
  else {
    f->eax = -1;
  }
  //printf("Exiting Syswrite Handler\n");
}

// Thread calls sysseek
void sysseek_handler(struct intr_frame *f)
{
  // Get arguments from the stack
  int fd = pop_stack(f);
  off_t newpos = pop_stack(f);

  struct thread *tp = thread_current();
  struct list_elem *e;

  // Search for file descriptor in the thread open-file handles
  for (e = list_begin(&tp->handles); e != list_end(&tp->handles);
      e = list_next(e)) {
    struct fileHandle *fhp = list_entry(e, struct fileHandle, fileElem);
    // File descriptor found: read file
    if (fhp->fd == fd) {
      file_seek(fhp->file, newpos);
      return;
    }
  }
}

// Thread calls systell
void systell_handler(struct intr_frame *f)
{
  // Get the file descriptor from the stack
  int fd = pop_stack(f);

  struct thread *tp = thread_current();
  struct list_elem *e;

  // Search for file descriptor in the thread open-file handles
  for (e = list_begin(&tp->handles); e != list_end(&tp->handles);
      e = list_next(e)) {
    struct fileHandle *fhp = list_entry(e, struct fileHandle, fileElem);
    // File descriptor found: read file
    if (fhp->fd == fd) {
      f->eax = (uint32_t) file_tell(fhp->file);
      return;
    }
  }
  f->eax = -1;
}

// Thread calls sysclose
void sysclose_handler(struct intr_frame *f)
{
  // Get the file descriptor from the stack
  int fd = pop_stack(f);

  struct thread *tp = thread_current();
  struct list_elem *e;

  // Search for file descriptor in the thread open-file handles
  for (e = list_begin(&tp->handles); e != list_end(&tp->handles);
      e = list_next(e)) {
    struct fileHandle *fhp = list_entry(e, struct fileHandle, fileElem);
    // File descriptor found: read file
    if (fhp->fd == fd) {
      file_close(fhp->file);
      list_remove(e);
      return;
    }
  }
}

/* Given a file descriptor, search the current thread's open-file handler list
* and return a file handler corresponding to the file descriptor. If no such
* handler exists, return NULL.
*/
struct fileHandle* get_handle(int fd)
{
  struct thread *tp = thread_current();
  struct list_elem *e;

  // Search for file descriptor in the thread open-file handles
  for (e = list_begin(&tp->handles); e != list_end(&tp->handles);
      e = list_next(e)) {
    struct fileHandle *fhp = list_entry(e, struct fileHandle, fileElem);
    // File descriptor found: return file handle
    if (fhp->fd == fd) {
      return fhp;
    }
  }

  // File descriptor not found: return null
  return NULL;
}


// Fails gracefully whenever a program does something bad
void terminate_thread()
{
  thread_current()->retVal = -1;
  thread_exit();
}
