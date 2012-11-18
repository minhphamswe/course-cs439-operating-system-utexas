#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "lib/kernel/list.h"
#include <lib/user/syscall.h>

#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;
  struct file *file;
  char *tmpfilename[16];
  char *saveptr;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL) {
    printf("process_execute(%s): Cannot get page for fn_copy\n", file_name);
    return TID_ERROR;
  }
  strlcpy (fn_copy, file_name, PGSIZE);

  // See if the file exists before trying to execute it
  strlcpy(tmpfilename, file_name, 16);

  struct semaphore s;
  sema_init(&s, 1);
  sema_down(&s);
  file = filesys_open (strtok_r(tmpfilename, " ", &saveptr));
  sema_up(&s);

  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      return TID_ERROR;
    }
  file_close(file);

  // Create a new thread to execute FILE_NAME.
  tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);
  if (tid == TID_ERROR) {
    palloc_free_page (fn_copy);
    return TID_ERROR;
  }

  // Check if the thread loaded properly
  struct thread *child = thread_by_tid(tid);
  if (child) {
    // Thread has not yet exited: wait for status of load
    printf("Thread is waiting for thread %d to report execution status\n", tid);
    sema_down(&child->exec_sema);

    if(thread_get_exit_status(tid)->status == -1) {
      return TID_ERROR;
    }
    else {
      return tid;
    }
  }
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (file_name, &if_.eip, &if_.esp);

  /* If load failed, quit. */
  palloc_free_page (file_name);
  if (!success) {
    thread_set_exit_status(thread_current()->tid, -1);
    sema_up(&thread_current()->exec_sema);
    thread_exit ();
  }

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  sema_up(&thread_current()->exec_sema);
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid)
{
//   printf("Starting process_wait(%d)\n", child_tid);
  // Check that the tid is a child of the requesting thread
  if (!thread_is_child(child_tid)) {
    // This thread is not your child, so you can't wait for it.
    return -1;
  }

  // Check that the tid has not been waited for by the requesting thread
  if (thread_has_waited(child_tid)) {
    // You have already waited for this tid, so go away!
    return -1;
  }

  // Wait until the thread with that tid exits
  struct thread *tp = thread_by_tid(child_tid);
  if (tp != NULL) {
    printf("Thread is waiting for thread %d to exit\n", child_tid);
    sema_down(&tp->wait_sema);
  }

  struct exit_status *es = thread_get_exit_status(child_tid);
  // Move the thread to the already-waited list
  thread_mark_waited(es);
  // Return exit status
//   printf("End process_wait(%d)\n", child_tid);
  return es->status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
//   printf("Start process_exit()\n");
  struct thread *cur = thread_current ();
  uint32_t *pd;

  char *saveptr;

  printf("%s: exit(%d)\n", strtok_r(cur->name, " ", &saveptr),
         thread_get_exit_status(cur->tid)->status);

  // Close open files
  file_close(cur->ownfile);
  struct list_elem *e;
  while (!list_empty (&cur->handles)) {
    e = list_pop_front (&cur->handles);
    struct fileHandle *s = list_entry (e, struct fileHandle, fileElem);
    file_close(s->file);
    free(s);
  }
  
  // Clear out our wait list
  while (!list_empty (&cur->wait_list)) {
    e = list_pop_front (&cur->wait_list);
    struct exit_status *s = list_entry (e, struct exit_status, wait_elem);
    free(s);
  }

  // Signal any waiting thread
  sema_up(&cur->wait_sema);      // signal before dying

  // Destroy the supplemental page table, which frees all pages and frames
  // in the process
  printf("Start page_table_destroy(): Thread %x(%d) has %d pages\n", thread_current(), thread_current()->tid, list_size(&(thread_current()->pages)));
  page_table_destroy(&cur->pages);   // FIXME: This causes a triple fault
  printf("End page_table_destroy(): Thread %x(%d) has %d pages\n", thread_current(), thread_current()->tid, list_size(&(thread_current()->pages)));

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }

  thread_clear_child_exit_status(cur);
//   printf("End process_exit()\n");
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
//   printf("Starting load(%x, %x, %x)\n", file_name, eip, esp);
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i, argc;
  void *btop;
  char *save_ptr, *token, *argv[128];
  char **token_addr[128];
  struct semaphore s;

  sema_init(&s, 1);

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Copy commandline to avoid modification by strtok_r */
  char* fn_copy = palloc_get_page (0);
  if (fn_copy == NULL) {
    printf("load(%s, %x, %x): Could not get space for fn_copy\n", file_name, eip, esp);
    return TID_ERROR;
  }
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Parse arguments into tokens */
  argc = 0;
  for (token = strtok_r (fn_copy, " ", &save_ptr); token != NULL;
       token = strtok_r (NULL, " ", &save_ptr)) {
    argv[argc] = token;
    argc++;
  }

  /* Open executable file. */
  sema_down(&s);
  file = filesys_open (argv[0]);
  sema_up(&s);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", argv[0]);
      goto done; 
    }

  /* Read and verify executable header. */
  sema_down(&s);
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", argv[0]);
      goto done; 
    }
  sema_up(&s);

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file)) {
        printf("Reading past the end of file.\n");
        goto done;
      }
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr) {
        printf("Reading file did not complete.\n");
        goto done;
      }
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable)) {
                printf("failed to load segment\n");
                goto done;
              }
            }
          else {
            printf("segment is invalid\n");
            goto done;
          }
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp)) {
    printf("Setting up of the stack failed.\n");
    goto done;
  }

  /* Allocate space for stack */
  btop = *esp;

  /* Put tokens onto the stack */
  for (i = argc-1; i >= 0; i--) {
    // get the token
    token = argv[i];

    // move the stack pointer and copy token onto the stack
    *esp = (char*) *esp - (strlen(token) + 1);
    strlcpy(*esp, token, strlen(token) + 1);

    // save copy-to address
    token_addr[i] = (char**) *esp;
  }

  /* align stack by word size (4) */
  while (((unsigned int) *esp % 4) != 0) {
    *esp = (uint8_t*) *esp - 1;
    *(uint8_t*) *esp = 0;
  }

  // push null pointer at end of argument list
  *esp = (char**) *esp - 1;
  *(char**)*esp = (char*) 0;

  // push addresses (order from right to left)
  for (i = argc - 1; i >= 0; i--) {
    *esp = (char***) *esp - 1;
    *(char***)*esp = token_addr[i];
  }

  // push address of argument vector
  *esp = (char***) *esp - 1;
  *(char***) *esp = (char**) *esp + 1;

  // push token count
  *esp = (int*) *esp - 1;
  *(int*)*esp = argc;

  // push "return address" to end the "function call" on the stack
  typedef void (*void_fn_ptr)(void);  // declare type pointer to void function
  *esp = (void_fn_ptr*) *esp - 1;
  *(void_fn_ptr*)*esp = 0;

  /* Start address. */
  *eip = (void_fn_ptr*) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  if (file) {
    if (success) {
      strlcpy(t->name, argv[0], 16);  // Assign new name to thread
      t->ownfile = file;
      sema_down(&s);
      file_deny_write(file);
      sema_up(&s);
    }
    else {
      sema_down(&s);
      file_close(file);
      sema_up(&s);
    }
  }
  if (fn_copy != NULL)
    palloc_free_page(fn_copy);

  printf("load(): Thread %x(%d) has %d pages\n", thread_current(), thread_current()->tid, list_size(&(thread_current()->pages)));
//   printf("End load(%x, %x, %x)\n", file_name, eip, esp);
  return success;
}

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
//   printf("Start validate_segment(%x, %x)\n", phdr, file);
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) {
    printf("Test failed: p_offset and p_vaddr must have the same page offset\n");
    return false;
  }

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) {
    printf("Test failed: p_offset must point within FILE.\n");
    return false;
  }

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) {
    printf("Test failed: p_memsz must be at least as big as p_filesz.\n");
    return false;
  }

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0) {
    printf("Test failed: The segment must not be empty.\n");
    return false;
  }

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr)) {
    printf("Test failed: The virtual memory region must start within the user address space range.\n");
    return false;
  }
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz))) {
    printf("Test failed: The virtual memory region must end within the user address space range.\n");
    return false;
  }

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr) {
    printf("Test failed: The region cannot wrap around across the kernel virtual address space.\n");
    return false;
  }

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE) {
    printf("Test failed: Disallow mapping page 0.\n");
    return false;
  }

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

//   printf("Start load_segment(%x, %d, %x, %d, %d, %d)\n", file, ofs, upage, read_bytes, zero_bytes, writable);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      struct page_entry *entry = allocate_page(upage);
      if (entry != NULL) {
        void* kpage = entry->frame->kpage;
        if (file_read(file, kpage, page_read_bytes) != (int) page_read_bytes) {
          free_page_entry(entry);
//           printf("End load_segment(%x, %d, %x, %d, %d, %d)\n", file, ofs, upage, read_bytes, zero_bytes, writable);
          return false;
        }
        memset (kpage + page_read_bytes, 0, page_zero_bytes);
      }
      else {
//         printf("End load_segment(%x, %d, %x, %d, %d, %d)\n", file, ofs, upage, read_bytes, zero_bytes, writable);
        return false;
      }

      if (!install_page(entry, writable)) {
        free_page_entry(entry);
//         printf("End load_segment(%x, %d, %x, %d, %d, %d)\n", file, ofs, upage, read_bytes, zero_bytes, writable);
        return false;
      }
//       uint8_t *kpage = palloc_get_page (PAL_USER);
//       if (kpage == NULL)
//         return false;
// 
//       /* Load this page. */
//       if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
//         {
//           palloc_free_page (kpage);
//           return false;
//         }
//       memset (kpage + page_read_bytes, 0, page_zero_bytes);
// 
//       /* Add the page to the process's address space. */
//       if (!install_page (upage, kpage, writable))
//         {
//           palloc_free_page (kpage);
//           return false;
//         }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
//   printf("End load_segment(%x, %d, %x, %d, %d, %d)\n", file, ofs, upage, read_bytes, zero_bytes, writable);
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
//   printf("Start setup_stack(%x)\n", esp);
  printf("Start setup_stack(): Thread %x(%d) has %d pages\n", thread_current(), thread_current()->tid, list_size(&(thread_current()->pages)));
  bool success = false;
  struct page_entry *entry = allocate_page(((uint8_t *) PHYS_BASE) - PGSIZE);
  success = install_page(entry, true);

  if (success)
    *esp = PHYS_BASE;
  else
    printf("Setting up of the stack failed. In process.c/setup_stack()\n");

//   printf("End setup_stack(%x)\n", esp);
  printf("End setup_stack(): Thread %x(%d) has %d pages\n", thread_current(), thread_current()->tid, list_size(&(thread_current()->pages)));
  return success;
}
