#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/synch.h"
#include "filesys/file.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */
#define PRI_DEPTH 8                     /* Number of possible donations per thread */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.


   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */

typedef struct priority_lock {
  struct lock* lock;
  struct thread* thread;                 /* Whose donation we are using */
} priority_lock;

// A single open file handler.
struct fileHandle {
//  char name[16];
  struct file *file;
  int fd;
  struct list_elem fileElem;
};

// A structure to hold the exit status for a thread
struct exit_status {
  tid_t tid;
  int status;

  /* List elemnets */
  struct list_elem exit_elem;   /* insert into global table of exit status */
  struct list_elem wait_elem;   /* insert into thread's list of waited pid */
  struct list_elem child_elem;  /* insert into thread's list of child pid */
};

#define MAXOPENFILES 16           /* Can only have 16 files in the OS for part 2 anyway */

struct thread
{
  /* Owned by thread.c. */
  tid_t tid;                      /* Thread identifier. */
  enum thread_status status;      /* Thread state. */
  char name[16];                  /* Name (for debugging purposes). */
  uint8_t *stack;                 /* Saved stack pointer. */
  int nativePriority;             /* Native (lowest) priority */
  int priority;                   /* Active Priority including donation. */
  int numDonors;                  /* Number of donors waiting */
  int nice;                       /* Niceness value of the thread */
  int recent_cpu;                 /* Recently-used CPU time (float) */
  struct file *ownfile;           /* My file, keep open to prevent writes */

  struct semaphore wait_sema;     /* Semaphore to signify the process waiter*/
  struct semaphore exec_sema;     /* Semaphore to signify the process executer*/
  bool exec_value;                /* Whether or not we could successfully execute */

  /* Keep track of who donated to us */
  struct priority_lock donors[PRI_DEPTH];
  struct priority_lock donees;    /* Keep track of who we donated to */

  struct list_elem allelem;       /* List element for all threads list. */

  /* Shared between thread.c and synch.c. */
  struct list_elem elem;          /* List element. */

  /* List element for the wait-on list of the parent thread */
  struct list_elem wait_elem;

  /* List element for the children list of the parent thread */
  struct list_elem child_elem;

  /* Keep track of open files */
  struct list handles;            /* List element for open files */
  struct list wait_list;          /* List of child threads waited on */
  struct list child_list;         /* List of all child threads */
  int nextFD;                     /* The next file, increment */

#ifdef USERPROG
  /* Owned by userprog/process.c. */
  uint32_t *pagedir;              /* Page directory. */
#endif

  /* Owned by thread.c. */
  unsigned magic;                 /* Detects stack overflow. */
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);
void updateActivePriority(struct thread *thread);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

struct thread* thread_by_tid(tid_t tid);
bool thread_is_child(tid_t tid);
bool thread_has_waited(tid_t tid);

struct exit_status* thread_get_exit_status(tid_t tid);
void thread_set_exit_status(tid_t tid, int status);

void thread_mark_waited(struct exit_status* es);

//bool is_executing(char *filename);

#endif /* threads/thread.h */
