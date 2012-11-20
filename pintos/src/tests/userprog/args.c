/* Prints the command-line arguments.
   This program is used for all of the args-* tests.  Grading is
   done differently for each of the args-* tests based on the
   output. */

#include "tests/lib.h"

// int buffer[128*1024];
// int buffer2[128*1024];

int
main (int argc, char *argv[]) 
{
  int i;

  int buffer[128*1024];
  int buffer2[128*1024];
//   buffer[128*1024 - 1] = 1;
//   buffer2[128*1024 - 1] = 1;
  for (i = 0; i < 128; i++) {
    buffer[i*1024] = i;
    buffer2[i*1024] = i;
    printf("%d %d\n", buffer[i*1024], buffer2[i*1024]);
  }

  test_name = "args";

  msg ("begin");
  msg ("argc = %d", argc);
  for (i = 0; i <= argc; i++)
    if (argv[i] != NULL)
      msg ("argv[%d] = '%s'", i, argv[i]);
    else
      msg ("argv[%d] = null", i);
  msg ("end");

  return 0;
}
