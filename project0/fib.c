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

static void doFib(int n, int doPrint);


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

  arg = atoi(argv[1]);
  if(arg < 0 || arg > MAX){
    fprintf(stderr, "number must be between 0 and %d\n", MAX);
    exit(-1);
  }

  doFib(arg, 1);
  
  return 0;
}

/* 
 * Recursively compute the specified number. If print is
 * true, print it. Otherwise, provide it to my parent process.
 *
 * NOTE: The solution must be recursive and it must fork
 * a new child for each call. Each process should call
 * doFib() exactly once.
 */
static void //returnValue
doFib(int n, int doPrint)
{
  // We will return two seperate values with this to the parent in order to not
  // have double recursive calls
  
  int comm1Pipe[2];     // Pipe to pass back from first child to parent process
  int comm2Pipe[2];     // And the second child
  char sendString[50];  // String that we are sending back
  char readBuffer[50];  // Buffer we are reading in from (temp only)
  
  int returnValue = 0;  // The fib number which is being returned
  
  pipe(comm1Pipe);	// Start the pipes up. Both parent and child will get same pipe
  pipe(comm2Pipe);
  
  if(n == 0) {
    // First fib number is 0
    if (doPrint) {
      printf("%d\n", 0);
      return 0;
    }
    returnValue = 0;
  }
  else if (n == 1) {
    // Second fib number is 1
    if (doPrint) {
      printf("%d\n", 1);
      return 1;
    }
    returnValue = 1;
  }
  else
  {
    pid_t pid1, pid2;
    pid_t rpid1, rpid2;

    if ((pid1 = fork()) == 0) {
      // If return value from fork is 0, we're in the first child
      // We need to send back doFib(n-1)
      
      close(comm1Pipe[0]);	// Child will close the read side
      
      returnValue = n - 1;
      doFib(n - 1, 0);
      
      // Send back our data
      sprintf(sendString, "%d", returnValue);
      write(comm1Pipe[1], sendString, (strlen(sendString)+1));
      
      exit(returnValue);
    }
    else if ((pid2 = fork()) == 0) {
      // We are the second child, send back doFib(n-2)
      
      close(comm2Pipe[0]);	// Close the read side
      
      returnValue = n - 2;
      doFib(n - 2, 0);
      
      // Send back our data
      sprintf(sendString, "%d", returnValue);
      write(comm2Pipe[1], sendString, (strlen(sendString)+1));
      
      exit(returnValue);
    }
    else {
      // We are the parent, wait for child then read it's input
      waitpid(pid1, &rpid1, 0);
      waitpid(pid2, &rpid2, 0);
      
      if (!WIFEXITED(rpid1)) {
        printf("%s: %d\n", "Child 1 exited with error", WEXITSTATUS(rpid1));
      }
      
      if (!WIFEXITED(rpid2)) {
        printf("%s: %d\n", "Child 2 exited with error", WEXITSTATUS(rpid2));
      }

      close(comm1Pipe[1]);	// Close the write side
      close(comm2Pipe[1]);
      
      // Get first return value from buffer
      read(comm1Pipe[0], readBuffer, sizeof(readBuffer));
      returnValue = atoi(readBuffer);
      
      // Get second return value from buffer
      read(comm2Pipe[0], readBuffer, sizeof(readBuffer));
      returnValue += atoi(readBuffer);
    }
  }

  if(doPrint) {
    // We are the first call, print out returnValue and we're done
    printf("%d\n", returnValue);
    return returnValue;
  }
  //return returnValue;
  //exit(returnValue);
  return;
}


