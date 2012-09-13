#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

void *print_numbers(void *name_ptr) {
    char *name;
    int i;
    name = (char*) name_ptr;
    for (i = 1; i < 11; i++) {
        printf("%s: %d\n", name, i);
    }
    return 0;
}

int main(int argc, char** argv) {
    pthread_t thread1, thread2, thread3;

    // Create the threads
    pthread_create(&thread1, NULL, print_numbers, (void*)"1");
    pthread_create(&thread2, NULL, print_numbers, (void*)"2");
    pthread_create(&thread3, NULL, print_numbers, (void*)"3");

    // Wait for threads to finish
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
    
    // When all threads have finished, exit
    exit(0);
}