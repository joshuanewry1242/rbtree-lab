#include "fault_alloc.h"

#include <stdlib.h>

/* Disarmed (countdown == 0) this is a plain forward to malloc/free, which
 * is what every non-sweep test runs against. Armed, the countdown-th call
 * from the arm returns NULL once, then allocation succeeds again. */
static long countdown = 0;
static long total = 0;
static int  fired = 0;

void *rb_malloc(size_t n)
{
	total++;
	if (countdown > 0 && --countdown == 0) {
		fired = 1;
		return NULL;
	}
	return malloc(n);
}

void rb_free(void *p)
{
	free(p);
}

void fault_alloc_arm(long n)
{
	countdown = n;
	fired = 0;
}

void fault_alloc_disarm(void)
{
	countdown = 0;
}

long fault_alloc_total(void)
{
	return total;
}

int fault_alloc_fired(void)
{
	return fired;
}
