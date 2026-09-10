#include "rbtree.h"

#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * M0 SKELETON. Nothing here is implemented yet.
 *
 * M1: define struct rb_node, then rb_create / rb_insert (+ fixup) / rb_find /
 *     rb_foreach / rb_validate / rb_size / rb_destroy.
 * M2: rb_delete with every deletion-fixup case + the fuzzer.
 * M3: hardening, edge cases, final clean sweep.
 *
 * Consider routing all allocation through rb_malloc / rb_free wrappers now
 * (Section 9, "A tip you may thank yourself for") -- the next assignment
 * requires that plumbing.
 * ------------------------------------------------------------------------- */

struct rb_node; /* defined in M1 */

struct rbtree {
	struct rb_node  *root;
	rb_value_free_fn value_free;
	size_t           size;
};

rbtree_t *rb_create(rb_value_free_fn value_free)
{
	(void)value_free;
	return NULL; /* TODO(M1) */
}

int rb_insert(rbtree_t *t, const char *key, void *value)
{
	(void)t;
	(void)key;
	(void)value;
	return -1; /* TODO(M1) */
}

void *rb_find(const rbtree_t *t, const char *key)
{
	(void)t;
	(void)key;
	return NULL; /* TODO(M1) */
}

int rb_delete(rbtree_t *t, const char *key)
{
	(void)t;
	(void)key;
	return -1; /* TODO(M2) */
}

size_t rb_size(const rbtree_t *t)
{
	(void)t;
	return 0; /* TODO(M1) */
}

void rb_foreach(const rbtree_t *t,
                void (*fn)(const char *key, void *value, void *ctx),
                void *ctx)
{
	(void)t;
	(void)fn;
	(void)ctx;
	/* TODO(M1) */
}

int rb_validate(const rbtree_t *t)
{
	(void)t;
	return -1; /* TODO(M1) */
}

void rb_destroy(rbtree_t *t)
{
	(void)t;
	/* TODO(M1) */
}
