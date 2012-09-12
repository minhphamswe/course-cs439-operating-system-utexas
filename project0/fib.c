#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

const int MAX = 13;

/*static void*/ int doFib(int n, int doPrint);


/*
 * unix_error - unix-style error routine.
 */
inline static void 
unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}


int main(int argc, char **argv)
{
  int arg;
  int print;

  if(argc < 2 || argc > 3){
    fprintf(stderr, "Usage: fib <num>\n");
    exit(-1);
  }

  if(argc == 3){
    print = 0;
  }
  else {
    print = 1;
  }

  arg = atoi(argv[1]);
  if(arg < 0 || arg > MAX){
    fprintf(stderr, "number must be between 0 and %d\n", MAX);
    exit(-1);
  }

//  doFib(arg, 1);
  
  return doFib(arg, print);
}

/* 
 * Recursively compute the specified number. If print is
 * true, print it. Otherwise, provide it to my parent process.
 *
 * NOTE: The solution must be recursive and it must fork
 * a new child for each call. Each process should call
 * doFib() exactly once.
 */
//static void
int 
doFib(int n, int doPrint)
{
  int returnValue = 0;
  if(n == 0)
    returnValue = 0;
  else if(n == 1)
    returnValue = 1;
  else
  {
    pid_t pid1, pid2;
    pid_t rpid1, rpid2;
    char arg[50];

    if ((pid1 = fork()) == 0) {
      // Child process
      sprintf(arg, "%d", n-1);
      execl("fib", "fib", arg, "NOPRINT", NULL);
    }
    else if ((pid2 = fork()) == 0) {
      // Second child process 
      sprintf(arg, "%d", n-2);
      execl("fib", "fib", arg, "NOPRINT", NULL);
    }
    else {
      waitpid(pid1, &rpid1, 0);
      waitpid(pid2, &rpid2, 0);
      if (!WIFEXITED(rpid1)) {
        printf("%s: %d\n", "Child 1 exited with error", WEXITSTATUS(rpid1));
      }

      if (!WIFEXITED(rpid2)) {
        printf("%s: %d\n", "Child 2 exited with error", WEXITSTATUS(rpid2));
      }

      returnValue = WEXITSTATUS(rpid1) + WEXITSTATUS(rpid2);
    }
  }
  if(doPrint)
    printf("%d\n", returnValue);
  return returnValue;
}


