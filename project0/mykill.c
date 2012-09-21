/*
 * mykill.c - Allows us to quit the handle process without using a kill -9
 *
 * Eric Aschner - easchner
 * Minh Pham - minhpham
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>


int main(int argc, char **argv)
{
  // Make sure we have a PID before continuing
  // We'll assume the second argument is valid, since the process is going
  // to terminate right after regardless of success
  if(argc != 2)
  {
      printf("Correct usage is 'mykill PID'");
  }

  // From the cmdline we get a string, let's make it a number
  pid_t killid;
  killid = atoi(argv[1]);

  // and send the signal off, level of SIGUSR1
  kill(killid, SIGUSR1);
}
