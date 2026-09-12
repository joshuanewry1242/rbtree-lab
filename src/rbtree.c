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

typedef enum { RED, BLACK } rb_color_t;

/* NIL is a NULL child, consistently treated as black. A parent pointer is
 * kept so insert/delete fixup can climb toward the root. */
struct rb_node {
	char           *key;    /* heap copy; the tree owns it */
	void           *value;  /* ownership per include/rbtree.h */
	struct rb_node *left;
	struct rb_node *right;
	struct rb_node *parent;
	rb_color_t      color;
};

struct rbtree {
	struct rb_node  *root;
	rb_value_free_fn value_free;
	size_t           size;
};

rbtree_t *rb_create(rb_value_free_fn value_free)
{
	rbtree_t *t = malloc(sizeof *t);
	if (t == NULL)
		return NULL; /* allocation failed: nothing to unwind */

	t->root = NULL;
	t->size = 0;
	t->value_free = value_free; /* may be NULL: tree does not own values */
	return t;
}

int rb_insert(rbtree_t *t, const char *key, void *value)
{
	struct rb_node *parent = NULL;
	struct rb_node *cur = t->root;
	int cmp = 0;

	/* BST descent to the insertion point.
	 * Invariant: `parent` is the last node above `cur` on the search path;
	 * `cur` is the subtree left to examine. */
	while (cur != NULL) {
		cmp = strcmp(key, cur->key);
		if (cmp == 0) {
			/* TODO(M1): overwrite -- free cur->value via
			 * t->value_free (if set), install `value`, return 0. */
			(void)value;
			return -1;
		}
		parent = cur;
		cur = (cmp < 0) ? cur->left : cur->right;
	}

	/* `cur` is the empty slot: the new node becomes `parent`'s child on
	 * the `cmp` side, or t->root when parent == NULL. */
	/* TODO(M1): node_alloc (node + key copy, goto-cleanup); link as a RED
	 * child; t->size++; insert_fixup(t, new_node). */
	(void)parent;
	(void)value;
	return -1;
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
