#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "util.h"

void sigint_handler(int sig);

/*
 * First, print out the process ID of this process.
 *
 * Then, set up the signal handler so that ^C causes
 * the program to print "Nice try.\n" and continue looping.
 *
 * Finally, loop forever, printing "Still here\n" once every
 * second.
 */
int main(int argc, char **argv)
{
    printf("My PID: %d\n", getpid());

    Signal(SIGINT, sigint_handler);
    
    struct timespec req;
    req.tv_sec = 1;
    while(1) {
        nanosleep(&req, NULL);
        printf("%s", "Still here\n");
    }

    return 0;
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigint_handler(int sig)
{
    printf("Nice try.\n");
}

