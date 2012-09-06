#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>

#ifndef __cplusplus
#define timeval struct timeval
#endif

void my_getuid() {
	getuid();
}

int main(int argc, char** argv) {
	timeval start, end, fstart, fend;
	int i, num_iters = 100000;
	double sys_time, sysf_time, func_time;

	// Get system + function time
	gettimeofday(&fstart, NULL);
	for (i = 0; i < num_iters; i++) {
		my_getuid();
	}
	gettimeofday(&fend, NULL);

	sysf_time = fend.tv_usec - fstart.tv_usec;
	sysf_time = sysf_time / num_iters;
	
	// Get system time
	gettimeofday(&start, NULL);
	for (i = 0; i < num_iters; i++) {
		getuid();
	}
	gettimeofday(&end, NULL);

	sys_time = end.tv_usec - start.tv_usec;
	sys_time = sys_time / num_iters;
	
	// Calculate function time
	func_time = sysf_time - sys_time;

	printf("Avg system getuid() time = %f microsec\n", sys_time);
	printf("Avg sys+func getuid() time = %f microsec\n", sysf_time);
	printf("Avg function getuid() time = %f microsec\n", func_time);
}
