#include <syscall.h>
#include <stdio.h>

int main (int, char *[]);
void _start (int argc, char *argv[]);

void
_start (int argc, char *argv[]) 
{
  //printf("Entering function\n");
  //printf("Argc is : %d\n", argc);
  exit (main (argc, argv));
}
