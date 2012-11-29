#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "threads/malloc.h"

#include "lib/kernel/list.h"
#include "lib/string.h"
#include <lib/syscall-nr.h>
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "filesys/filesys.h"

// Forward declarations of functions
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
void sysmmap_handler (struct intr_frame *f);
void sysunmmap_handler (struct intr_frame *f);

uint32_t pop_stack(struct intr_frame *f);

struct fileHandle* get_handle (int fd);
void terminate_thread(void);

static struct semaphore filesys_sema;

/* Read 4 bytes at the user virtual address UADDR
* UADDR must be below PHYS_BASE
* Returns the byte value if successful, -1 if a segfault occurred.
*/
static int
get_user (const uint32_t *uaddr) {
  if (uaddr < PHYS_BASE) {
    int result;
    asm ( "movl $1f, %0; movl %1, %0; 1:"
          : "=&a" (result)
          : "m" (*uaddr));
    return result;
  }
  return -1;
}

/* Writes BYTE to user address UDST.
* UDST must be below PHYS_BASE
* Returns true if successful, false if a segfault occurred.
*/
static bool
put_user (uint8_t *udst, uint8_t byte) {
  if (udst < PHYS_BASE) {
    int error_code;
    asm ( "movl $1f, %0; movb %b2, %1; 1:"
	  : "=&a" (error_code), "=m" (*udst)
	  : "q" (byte));
    return error_code != -1;
  }
  return false;
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
  // Initialize semaphore(s)
  sema_init(&filesys_sema, 1);

  // Register handler
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  // It's easier to pop the stack as we go, so we will need to reset it
  void *tmpesp = f->esp;

  // Examine user memory to find out which system call gets called
  int syscall_number = pop_stack(f);

  switch (syscall_number) {
    case SYS_HALT:
      syshalt_handler(f);
      break;
    case SYS_EXIT:
      sysexit_handler(f);
      break;
    case SYS_EXEC:
      sysexec_handler(f);
      break;
    case SYS_WAIT:
      syswait_handler(f);
      break;
    case SYS_CREATE:
      syscreate_handler(f);
      break;
    case SYS_REMOVE:
      sysremove_handler(f);
      break;
    case SYS_OPEN:
      sysopen_handler(f);
      break;
    case SYS_FILESIZE:
      sysfilesize_handler(f);
      break;
    case SYS_READ:
      sysread_handler(f);
      break;
    case SYS_WRITE:
      syswrite_handler(f);
      break;
    case SYS_SEEK:
      sysseek_handler(f);
      break;
    case SYS_TELL:
      systell_handler(f);
      break;
    case SYS_CLOSE:
      sysclose_handler(f);
      break;
    case SYS_MMAP:
      sysmmap_handler(f);
      break;
    case SYS_MUNMAP:
      sysmunmap_handler(f);
      break;
  }

  // Replace stack pointer back to where it was for the application
  f->esp = tmpesp;
}


/**
 * Thread turns off system.
 * 
 * Terminates Pintos by calling shutdown_power_off() (declared in
 * devices/shutdown.h). This should be seldom used, because you lose some
 * information about possible deadlock situations, etc.
 */
void syshalt_handler(struct intr_frame *f)
{
  // Turn off pintos
  shutdown_power_off();
}


/**
 * Thread calls exit()
 *
 * Terminates the current user program, returning status to the kernel. If
 * the process's parent waits for it (see below), this is the status that
 * will be returned. Conventionally, a status of 0 indicates success and
 * nonzero values indicate errors.
 */
void sysexit_handler(struct intr_frame *f)
{
  // Get the exit value from the stack
  int exitValue = (int) pop_stack(f);

  // Set the exit value
  thread_set_exit_status(thread_current()->tid, exitValue);

  // End the currently running thread
  thread_exit();
}

/**
 * Thread calls sysexec
 *
 * Runs the executable whose name is given in cmd_line, passing any given
 * arguments, and returns the new process's program id (pid). Must return
 * pid -1, which otherwise should not be a valid pid, if the program cannot
 * load or run for any reason. Thus, the parent process cannot return from
 * the exec until it knows whether the child process successfully loaded its
 * executable. You must use appropriate synchronization to ensure this.
 */
void sysexec_handler(struct intr_frame *f)
{
  char *cmdline = pop_stack(f);

  if (cmdline) {
    f->eax = process_execute(cmdline);
  }
}

/**
 * Thread calls syswait
 *
 * Waits for a child process pid and retrieves the child's exit status.
 * If pid is still alive, waits until it terminates. Then, returns the status
 * that pid passed to exit. If pid did not call exit(), but was terminated by
 * the kernel (e.g. killed due to an exception), wait(pid) must return -1.
 * It is perfectly legal for a parent process to wait for child processes
 * that have already terminated by the time the parent calls wait, but the
 * kernel must still allow the parent to retrieve its child's exit status, or
 * learn that the child was terminated by the kernel.
 *
 * wait must fail and return -1 immediately if any of the following conditions
 * are true:
 *   - pid does not refer to a direct child of the calling process. pid is a
 *     direct child of the calling process if and only if the calling process
 *     received pid as a return value from a successful call to exec.
 *     Note that children are not inherited: if A spawns child B and B spawns
 *     child process C, then A cannot wait for C, even if B is dead. A call
 *     to wait(C) by process A must fail. Similarly, orphaned processes are
 *     not assigned to a new parent if their parent process exits before they
 *     do.
 *   - The process that calls wait has already called wait on pid. That is, a
 *     process may wait for any given child at most once.
 * Processes may spawn any number of children, wait for them in any order,
 * and may even exit without having waited for some or all of their children.
 *
 * Your design should consider all the ways in which waits can occur. All of
 * a process's resources, including its struct thread, must be freed whether
 * its parent ever waits for it or not, and regardless of whether the child
 * exits before or after its parent.
 *
 * You must ensure that Pintos does not terminate until the initial process
 * exits. The supplied Pintos code tries to do this by calling process_wait()
 * (in userprog/process.c) from main() (in threads/init.c). We suggest that
 * you implement process_wait() according to the comment at the top of the
 * function and then implement the wait system call in terms of
 * process_wait().
 */
void syswait_handler(struct intr_frame *f)
{
  // Get PID from stack
  tid_t child = pop_stack(f);
  f->eax = process_wait(child);
}

/** Thread calls syscreate
 *
 * Creates a new file called file initially initial_size bytes in size.
 * Returns true if successful, false otherwise. Creating a new file does not
 * open it: opening the new file is a separate operation which would require
 * a open system call.
 */
void syscreate_handler(struct intr_frame *f)
{
  // Get file name and size from stack
  char *filename = pop_stack(f);
  off_t filesize = pop_stack(f);

  if (get_user(filename) == -1) 
    terminate_thread();

  if (filename == NULL || strlen(filename) <= 0 || filesize < 0)
    terminate_thread();

  sema_down(&filesys_sema);
  f->eax = filesys_create(filename, filesize);
  sema_up(&filesys_sema);
}

/**
 * Thread calls sysremove
 *
 * Deletes the file called file. Returns true if successful, false otherwise.
 * A file may be removed regardless of whether it is open or closed, and
 * removing an open file does not close it. See Removing an Open File, for
 * details.
 */
void sysremove_handler(struct intr_frame *f)
{
  // Get file name from stack
  char *filename = pop_stack(f);

  sema_down(&filesys_sema);
  f->eax = filesys_remove(filename);
  sema_up(&filesys_sema);
}

/** Thread calls sysopen
 *
 * Opens the file called file. Returns a nonnegative integer handle called a
 * "file descriptor" (fd), or -1 if the file could not be opened. File
 * descriptors numbered 0 and 1 are reserved for the console: fd 0
 * (STDIN_FILENO) is standard input, fd 1 (STDOUT_FILENO) is standard output.
 * The open system call will never return either of these file descriptors,
 * which are valid as system call arguments only as explicitly described
 * below.
 *
 * Each process has an independent set of file descriptors. File descriptors
 * are not inherited by child processes.
 *
 * When a single file is opened more than once, whether by a single process
 * or different processes, each open returns a new file descriptor. Different
 * file descriptors for a single file are closed independently in separate
 * calls to close and they do not share a file position.
 */
void sysopen_handler(struct intr_frame *f)
{
  // Get file name
  char *file_name = pop_stack(f);

  if(file_name == NULL) {
    terminate_thread();
  }

  // Open file
  sema_down(&filesys_sema);
  struct file *file = filesys_open(file_name);
  sema_up(&filesys_sema);

  // File cannot be opened: return -1
  if (file == NULL) {
    f->eax = -1;
  }
  // File is opened: put it on the list of open file handles
  else {
    struct thread *t = thread_current();

    enum intr_level old_level = intr_disable();
    struct fileHandle *newHandle = malloc(sizeof(struct fileHandle));
    newHandle->file = file;
    newHandle->fd = t->nextFD++;
    list_push_back(&t->handles, &newHandle->fileElem);
    intr_set_level(old_level);

    // return the file descriptor
    f->eax = newHandle->fd;
  }
}

/**
 * Thread call sysfilesize
 *
 * Returns the size, in bytes, of the file open as fd.
 */
void sysfilesize_handler(struct intr_frame *f)
{
  // Get fd from stack
  int fd = (int) pop_stack(f);

  struct fileHandle *fhp = get_handle(fd);
  if (fhp) {
    sema_down(&filesys_sema);
    f->eax = file_length(fhp->file);
    sema_up(&filesys_sema);
  }
  else {
    f->eax -1;
  }
}

/**
 * Thread calls sysread
 *
 * Reads size bytes from the file open as fd into buffer. Returns the number
 * of bytes actually read (0 at end of file), or -1 if the file could not be
 * read (due to a condition other than end of file). Fd 0 reads from the
 * keyboard using input_getc().
 */
void sysread_handler(struct intr_frame *f)
{
  // Get arguments from the stack
  int fd = pop_stack(f);
  void *buffer = pop_stack(f);
  off_t size = pop_stack(f);

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
    // Check to make sure buffer is in user space
    if (get_user(buffer) == -1) {
      terminate_thread();
    }

    if (put_user(buffer, 0x1) == false) {
      terminate_thread();
    }

    struct fileHandle *fhp = get_handle(fd);
    if (fhp) {
      sema_down(&filesys_sema);
      f->eax = file_read(fhp->file, buffer, size);
      sema_up(&filesys_sema);
     }
    else {
      // File descriptor not found: return -1
      f->eax = -1;
    }
  }
}

/**
 * Thread calls syswrite
 *
 * Writes size bytes from buffer to the open file fd. Returns the number of
 * bytes actually written, which may be less than size if some bytes could
 * not be written.
 * 
 * Writing past end-of-file would normally extend the file, but file growth
 * is not implemented by the basic file system. The expected behavior is to
 * write as many bytes as possible up to end-of-file and return the actual
 * number written, or 0 if no bytes could be written at all.
 *
 * Fd 1 writes to the console. Your code to write to the console should write
 * all of buffer in one call to putbuf(), at least as long as size is not
 * bigger than a few hundred bytes. (It is reasonable to break up larger
 * buffers.) Otherwise, lines of text output by different processes may end
 * up interleaved on the console, confusing both human readers and our
 * grading scripts.
 */
void syswrite_handler(struct intr_frame *f)
{
  // Get arguments from the stack
  uint32_t fdnum = pop_stack(f);        // file descriptor number
  uint32_t buffer = pop_stack(f);       // address (in UVAS) of buffer
  uint32_t bufferSize = pop_stack(f);   // size of the buffer to write

  // Check to see if it's a console out, and print if yes
  if(fdnum == 1) {
    putbuf((char *) buffer, bufferSize);
    f->eax = bufferSize;
  }
  // Not to console, so print to file
  else if (fdnum > 1) {
    struct fileHandle *fhp = get_handle(fdnum);

    // Is the file currently executing?
    if (fhp != NULL) {
     sema_down(&filesys_sema);
      f->eax = file_write(fhp->file, (void*) buffer, bufferSize);
     sema_up(&filesys_sema);
     }
    else {
      f->eax = -1;
    }
  }
  else {
    f->eax = -1;
  }
}

/**
 * Thread calls seek(int fd, unsigned position)
 *
 * Changes the next byte to be read or written in open file fd to position,
 * expressed in bytes from the beginning of the file. (Thus, a position of 0
 * is the file's start.)
 *
 * A seek past the current end of a file is not an error. A later read
 * obtains 0 bytes, indicating end of file. A later write extends the file,
 * filling any unwritten gap with zeros. (However, in Pintos, files will have
 * a fixed length until project 4 is complete, so writes past end of file
 * will return an error.) These semantics are implemented in the file system
 * and do not require any special effort in system call implementation.
 */
void sysseek_handler(struct intr_frame *f)
{
  // Get arguments from the stack
  int fd = pop_stack(f);
  off_t newpos = pop_stack(f);

  // Search for file descriptor in the thread open-file handles
  struct fileHandle *fhp = get_handle(fd);
  if (fhp) {
    sema_down(&filesys_sema);
    file_seek(fhp->file, newpos);
    sema_up(&filesys_sema);
  }

}

/**
 * Thread calls systell
 *
 * Returns the position of the next byte to be read or written in open file
 * fd, expressed in bytes from the beginning of the file.
 */
void systell_handler(struct intr_frame *f)
{
  // Get the file descriptor from the stack
  int fd = pop_stack(f);

  // Search for file descriptor in the thread open-file handles
  struct fileHandle *fhp = get_handle(fd);
  if (fhp) {
    // File descriptor found: read file
    sema_down(&filesys_sema);
    f->eax = (uint32_t) file_tell(fhp->file);
    sema_up(&filesys_sema);
  }
  else {
    f->eax = -1;
  }
}

/**
 * Thread calls sysclose
 *
 * Closes file descriptor fd. Exiting or terminating a process implicitly
 * closes all its open file descriptors, as if by calling this function for
 * each one.
 */
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
      sema_down(&filesys_sema);
      file_close(fhp->file);
      list_remove(e);
      sema_up(&filesys_sema);
      return;
    }
  }
}

/**
 * Thread class mmap (int fd, void *addr).
 *
 * Maps the file open as fd into the process's virtual address space. The
 * entire file is mapped into consecutive virtual pages starting at addr.
 *
 * Your VM system must lazily load pages in mmap regions and use the mmaped
 * file itself as backing store for the mapping. That is, evicting a page
 * mapped by mmap writes it back to the file it was mapped from.
 *
 * If the file's length is not a multiple of PGSIZE, then some bytes in the
 * final mapped page "stick out" beyond the end of the file. Set these bytes
 * to zero when the page is faulted in from the file system, and discard
 * them when the page is written back to disk.
 *
 * If successful, this function returns a "mapping ID" that uniquely
 * identifies the mapping within the process. On failure, it must return -1,
 * which otherwise should not be a valid mapping id, and the process's
 * mappings must be unchanged.
 *
 * A call to mmap may fail if the file open as fd has a length of zero bytes.
 * It must fail if addr is not page-aligned or if the range of pages mapped
 * overlaps any existing set of mapped pages, including the stack or pages
 * mapped at executable load time.
 * It must also fail if addr is 0, because some Pintos code assumes virtual
 * page 0 is not mapped.
 * Finally, file descriptors 0 and 1, representing console input and output,
 * are not mappable.
 */
void sysmmap_handler(struct intr_frame* f)
{
  int fd = pop_stack(f);
  void *addr = pop_stack(f);

  int ret = -1;     // return value, -1 for failure

  if (fd != 0 && fd != 1 && pg_ofs(addr) == 0) {
  }

  f->eax = ret;
}

/**
 * Thread calls munmap (mapid_t mapid).
 *
 * Unmaps the mapping designated by mapping, which must be a mapping ID
 * returned by a previous call to mmap by the same process that has not yet
 * been unmapped.
 */
void sysmunmap_handler(struct intr_frame* f)
{
  int mapid = pop_stack(f);
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
void terminate_thread(void)
{
  thread_set_exit_status(thread_current()->tid, -1);
  thread_exit();
}

