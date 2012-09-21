/*
 * handle.c - Practice intercepting handles
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
#include "util.h"

void sigint_handler(int sig);
void sigusr_handler(int sig);

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
    // Print my PID
    printf("My PID: %d\n", getpid());

    // Set up signal handlers for SIGINT and SIGUSR1
    Signal(SIGINT, sigint_handler);
    Signal(SIGUSR1, sigusr_handler);

    // Required for nanosleep
    struct timespec req, rem;

    // Until someone sends us a SIGUSR1 or kill -9's us, just keep printing
    // out still here like a good little Troll >:O
    while(1) {
        req.tv_sec = 1;
        req.tv_nsec = 0;
        int sleepdone = -1;
        // Until we finish our entire second, don't quit on signals
        while(sleepdone == -1) {
          sleepdone = nanosleep(&req, &rem);
          req = rem;
        }
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
    // This will run whenever we are sent a SIGINT
    // Basically, just print out 'Nice Try.' to the console
    // Don't use printf since it is unsafe inside of signals
    ssize_t bytes;
    const int STDOUT = 1;
    bytes = write(STDOUT, "Nice Try.\n", 10);
    if(bytes != 10)
        exit(-999);
}

void sigusr_handler(int sig)
{
    // And this one will run whenever we receive SIGUSR1
    // We are actually going to behave and quit now
    ssize_t bytes;
    const int STDOUT = 1;
    bytes = write(STDOUT, "exiting\n", 8);
    if(bytes != 10)
        exit(-999);
    exit(1);
}

