#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>


int main(int argc, char **argv)
{
  if(argc != 2)
  {
      printf("Correct usage is 'mykill PID'");
  }

  pid_t killid;
  killid = atoi(argv[1]);

  kill(killid, SIGUSR1);
}
