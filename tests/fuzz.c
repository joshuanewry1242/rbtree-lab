#include "rbtree.h"

#include <stdio.h>
#include <stdlib.h>

/* M0 skeleton. M2 makes this a real randomized stress driver: >= 1e5
 * insert/find/delete ops against a reference model (sorted array or linked
 * list), calling rb_validate at least every 100 ops. Print a fixed seed so
 * failures reproduce. */

int main(int argc, char **argv)
{
	long ops = (argc > 1) ? atol(argv[1]) : 100000;
	printf("fuzz: M0 skeleton, %ld ops requested, no-op\n", ops);
	return 0;
}
