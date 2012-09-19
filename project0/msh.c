/* 
 * msh - A mini shell program with job control
 * 
 * <Put your name and login ID here>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include "util.h"
#include "jobs.h"


/* Global variables */
int verbose = 0;            /* if true, print additional output */

extern char **environ;      /* defined in libc */
static char prompt[] = "msh> ";    /* command line prompt (DO NOT CHANGE) */
static struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
void usage(void);
void sigquit_handler(int sig);


/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
	default:
            usage();
	}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

        /* Read command line */
        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin)) { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
        } 

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
    char *argv[50];
    int backgroundp;

    backgroundp = parseline(cmdline, argv);

    if ((argv[0] != NULL) && (!builtin_cmd(argv))) {
        pid_t child;
        // Block SIGCHLD
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGCHLD);
        sigprocmask(SIG_BLOCK, &set, NULL);
        
        if ((child = fork()) == 0) {
            // Unblock SIGCHLD
            sigprocmask(SIG_UNBLOCK, &set, NULL);
            
            // Child process
            setpgid(0, 0);  /* put child in new process group (id = child)*/
            execv(argv[0], argv);
            printf("%s: Command not found\n", argv[0]);
            exit(125);      /* only if execv failed */
        }
        else {
            // Parent
            if (!backgroundp) {
                // Foreground job
                addjob(jobs, child, FG, cmdline);
                // Unblock SIGCHLD
                sigprocmask(SIG_UNBLOCK, &set, NULL);
                waitfg(child);
            }
            else {
                // Background job
                addjob(jobs, child, BG, cmdline);
                printf("[%d] (%d) ", pid2jid(jobs, child), child);
                printf("%s", cmdline);
            }
        }
    }
    return;
}


/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 * Return 1 if a builtin command was executed; return 0
 * if the argument passed in is *not* a builtin command.
 */
int builtin_cmd(char **argv) 
{
    char* cmd = argv[0];
    if (strcmp(cmd, "quit") == 0) {
        exit(0);
    }
    else if (strcmp(cmd, "jobs") == 0) {
        listjobs(jobs);
        return 1;
    }
    else if (strcmp(cmd, "bg") == 0) {
        do_bgfg(argv);
        return 1;
    }
    else if (strcmp(cmd, "fg") == 0) {
        do_bgfg(argv);
        return 1;
    }
    else {
        return 0;    /* not a builtin command */
    }
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    if(argv[1] == NULL) {
        // No argument
        printf("%s %s", argv[0], "command requires PID or %jobid argument\n");
        return;
    }
    else {
        pid_t jobPID = 0;
        // Extract pid from argument
        if (argv[1][0] == '%') {
            int jobJID = atoi(argv[1] + 1);
            if (jobJID == 0) {
                printf("%s %s", argv[0], "argument must be a PID or %jobid\n");
                return;
            }

            if (getjobjid(jobs, jobJID)) {
                jobPID = getjobjid(jobs, jobJID)->pid;
                if (strcmp(argv[0], "fg") == 0) {
                    // Bring a BG or ST job to FG and restart if (if necessary)
                    getjobpid(jobs, jobPID)->state = FG;
                    kill(jobPID * -1, SIGCONT);
                    waitfg(jobPID);
                }
                else if (strcmp(argv[0], "bg") == 0) {
                    // Bring a ST job to BG
                    getjobpid(jobs, jobPID)->state = BG;
                    kill(jobPID * -1, SIGCONT);
                    printf("[%d] (%d) %s", jobJID, jobPID, getjobpid(jobs, jobPID)->cmdline);
                }
            }
            else {
                printf("%s: No such job\n", argv[1]);
            }
        }
        else {
            jobPID = atoi(argv[1]);
            if (jobPID == 0) {
                printf("%s %s", argv[0], "argument must be a PID or %jobid\n");
                return;
            }

            if (getjobpid(jobs, jobPID)) {
                if (strcmp(argv[0], "fg") == 0) {
                    // Bring a BG or ST job to FG and restart if (if necessary)
                    getjobpid(jobs, jobPID)->state = FG;
                    kill(jobPID * -1, SIGCONT);
                    waitfg(jobPID);
                }
                else if (strcmp(argv[0], "bg") == 0) {
                    // Bring a ST job to BG
                    getjobpid(jobs, jobPID)->state = BG;
                    kill(jobPID * -1, SIGCONT);
                    printf("[%d] (%d) %s", getjobpid(jobs, jobPID)->jid, jobPID, getjobpid(jobs, jobPID)->cmdline);
                }
            }
            else {
                printf("(%s): No such process\n", argv[1]);
            }
        }
        return;
    }
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    struct timespec req;
    req.tv_sec = 1;
    while (getjobpid(jobs, pid)) {
        if (getjobpid(jobs, pid)->state == FG)
            nanosleep(&req, NULL);
        else
            return;
    }
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
    int child;
    int childsig;
    pid_t jobPID;
    int jobJID;
    const int STDOUT = 1;
    char msg[50];
    ssize_t bytes;

    jobPID = 1;
    // While loop because there may be more than one job that died
    while (jobPID > 0) {
        jobPID = waitpid(-1, &child, WNOHANG | WUNTRACED);
        if (WIFEXITED(child) || WIFSIGNALED(child) || WIFSTOPPED(child)) {
            pid2jid(jobs, jobJID);       // get JID before deleting job
            deletejob(jobs, jobPID);
            childsig = WTERMSIG(child);  // get signal that killed child

            if (childsig == SIGINT)
                sprintf(msg, "Job [%d] (%d) terminated by signal %d\n", jobJID, jobPID, childsig);
            else if (childsig == SIGTSTP)
                sprintf(msg, "Job [%d] (%d) stopped by signal %d\n", jobJID, jobPID, childsig);

            bytes = write(STDOUT, msg, strlen(msg));
        }
        else {
            bytes = write(STDOUT, "Job not deleted.", 20);
        }
    }
    bytes++;
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    pid_t fgPID;
    int fgJID;
    const int STDOUT = 1;
    char msg[50];
    ssize_t bytes;
    
    fgPID = fgpid(jobs);
    if (fgPID) {
        fgJID = pid2jid(jobs, fgPID);
        kill(fgPID * -1, SIGINT);
        sprintf(msg, "Job [%d] (%d) terminated by signal %d\n", fgJID, fgPID, sig);
        bytes = write(STDOUT, msg, strlen(msg));
        bytes++; // Just to get rid of warning
    }
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    pid_t fgPID;
    int fgJID;
    const int STDOUT = 1;
    char msg[50];
    ssize_t bytes;
    
    fgPID = fgpid(jobs);
    if (fgPID) {
        fgJID = pid2jid(jobs, fgPID);
        getjobpid(jobs, fgPID)->state = ST;
        kill(fgPID * -1, SIGTSTP);
        sprintf(msg, "Job [%d] (%d) stopped by signal %d\n", fgJID, fgPID, sig);
        bytes = write(STDOUT, msg, strlen(msg));
        bytes++; // Just to get rid of warning
    }
}

/*********************
 * End signal handlers
 *********************/



/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}



