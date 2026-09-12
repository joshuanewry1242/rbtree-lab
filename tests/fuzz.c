#include "rbtree.h"

#include <stdio.h>
#include <stdlib.h>

/* Randomized stress driver. Ops are chosen against a fixed key universe
 * ("f0000".."f(N-1)") and checked against a reference model (present[i]:
 * is key i currently in the tree?) -- a second, dumber "implementation"
 * trusted completely, per the companion's glossary. N is kept modest on
 * purpose so make memcheck stays tractable under valgrind's slowdown; the
 * op count (the interesting knob) is CLI-configurable and the Makefile
 * already passes 100000 for make test, 20000 for make memcheck. */

#define N   500
#define SEED 20260910u

/* 16, not the ~6 bytes "f0499\0" actually needs: GCC's -Wformat-truncation
 * (part of -Wextra, but not something Apple clang flags the same way,
 * which is why this only showed up in CI) reasons about i's *declared*
 * type (plain int, full range) rather than the loop's actual bound, and
 * wants room for the worst case -- up to 11 digits/sign for INT_MIN. */
static char keys[N][16];

static void die(const char *why, long op, int i)
{
	fprintf(stderr, "fuzz: FAILED at op %ld (key %s): %s\n", op, keys[i], why);
	fprintf(stderr, "fuzz: seed=%u -- rerun to reproduce\n", SEED);
	exit(1);
}

int main(int argc, char **argv)
{
	long ops = (argc > 1) ? atol(argv[1]) : 100000;
	printf("fuzz: seed=%u, %ld ops, %d-key universe\n", SEED, ops, N);

	for (int i = 0; i < N; i++)
		snprintf(keys[i], sizeof keys[i], "f%04d", i);

	srand(SEED);
	static int present[N];
	rbtree_t *t = rb_create(NULL);
	if (t == NULL)
		die("rb_create returned NULL", 0, 0);

	for (long op = 1; op <= ops; op++) {
		int i    = rand() % N;
		int kind = rand() % 3; /* 0=insert 1=find 2=delete */

		switch (kind) {
		case 0: /* insert (or overwrite, if already present) */
			if (rb_insert(t, keys[i], keys[i]) != 0)
				die("rb_insert failed", op, i);
			present[i] = 1;
			break;
		case 1: /* find */
			if (present[i]) {
				if (rb_find(t, keys[i]) != keys[i])
					die("rb_find: present key missing or wrong value", op, i);
			} else {
				if (rb_find(t, keys[i]) != NULL)
					die("rb_find: absent key returned a value", op, i);
			}
			break;
		case 2: /* delete */
			if (present[i]) {
				if (rb_delete(t, keys[i]) != 0)
					die("rb_delete failed on a present key", op, i);
				present[i] = 0;
			} else {
				if (rb_delete(t, keys[i]) != -1)
					die("rb_delete succeeded on an absent key", op, i);
			}
			break;
		}

		/* rb_validate at least every 100 ops, per the testing bar */
		if (op % 100 == 0 && rb_validate(t) != 0)
			die("rb_validate failed", op, i);
	}

	if (rb_validate(t) != 0)
		die("final rb_validate failed", ops, 0);

	size_t expect = 0;
	for (int i = 0; i < N; i++) {
		void *v = rb_find(t, keys[i]);
		if (present[i]) {
			expect++;
			if (v != keys[i])
				die("final cross-check: present key missing/wrong", ops, i);
		} else if (v != NULL) {
			die("final cross-check: absent key still findable (ghost)", ops, i);
		}
	}
	if (rb_size(t) != expect) {
		fprintf(stderr, "fuzz: size mismatch: rb_size=%zu model=%zu\n", rb_size(t), expect);
		return 1;
	}

	rb_destroy(t);
	printf("fuzz: %ld ops, all cross-checks passed\n", ops);
	return 0;
}
