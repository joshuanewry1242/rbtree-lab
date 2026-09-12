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

/* The entire allocation dance for one node: node, then key copy, with
 * goto-cleanup if the second allocation fails. Colors the node RED (every
 * new key enters red -- Section 3) and NULLs its links; caller splices it
 * in. Returns NULL on any allocation failure with nothing left allocated. */
static struct rb_node *node_alloc(const char *key, void *value)
{
	struct rb_node *n = malloc(sizeof *n);
	if (n == NULL)
		goto fail_node;

	size_t klen = strlen(key) + 1;
	n->key = malloc(klen);
	if (n->key == NULL)
		goto fail_key;
	memcpy(n->key, key, klen);

	n->value  = value;
	n->left   = NULL;
	n->right  = NULL;
	n->parent = NULL;
	n->color  = RED;
	return n;

fail_key:
	free(n);
fail_node:
	return NULL;
}

/* Left-rotate around x (Figure 2). Owns updating t->root when x was the
 * root -- the exact bookkeeping the harder-insertion figure warns about. */
static void rotate_left(rbtree_t *t, struct rb_node *x)
{
	struct rb_node *y = x->right;

	x->right = y->left;
	if (y->left != NULL)
		y->left->parent = x;

	y->parent = x->parent;
	if (x->parent == NULL)
		t->root = y;
	else if (x == x->parent->left)
		x->parent->left = y;
	else
		x->parent->right = y;

	y->left = x;
	x->parent = y;
}

/* Mirror of rotate_left; also owns updating t->root. */
static void rotate_right(rbtree_t *t, struct rb_node *x)
{
	struct rb_node *y = x->left;

	x->left = y->right;
	if (y->right != NULL)
		y->right->parent = x;

	y->parent = x->parent;
	if (x->parent == NULL)
		t->root = y;
	else if (x == x->parent->right)
		x->parent->right = y;
	else
		x->parent->left = y;

	y->right = x;
	x->parent = y;
}

/* Repairs the red-red violation insertion may have introduced at z.
 * Invariant at the top of each iteration: z is red, and the only possible
 * rule-4 breach in the whole tree is the edge z--z->parent. Case 1 pushes
 * the violation two levels toward the root (so the climb terminates in
 * O(log n) steps); cases 2-3 fix it in place with <=2 rotations total
 * (Section 3, Figures 3-5). uncle == NULL is read as black, per the
 * NULL-is-NIL representation. */
static void insert_fixup(rbtree_t *t, struct rb_node *z)
{
	while (z->parent != NULL && z->parent->color == RED) {
		if (z->parent == z->parent->parent->left) {
			struct rb_node *uncle = z->parent->parent->right;

			if (uncle != NULL && uncle->color == RED) {
				/* Case 1: red uncle -- recolor, move up */
				z->parent->color = BLACK;
				uncle->color = BLACK;
				z->parent->parent->color = RED;
				z = z->parent->parent;
			} else {
				if (z == z->parent->right) {
					/* Case 2: triangle -- rotate to a line */
					z = z->parent;
					rotate_left(t, z);
				}
				/* Case 3: line -- rotate at grandparent, swap colors */
				z->parent->color = BLACK;
				z->parent->parent->color = RED;
				rotate_right(t, z->parent->parent);
			}
		} else {
			/* Mirror: uncle hangs off the grandparent's left */
			struct rb_node *uncle = z->parent->parent->left;

			if (uncle != NULL && uncle->color == RED) {
				z->parent->color = BLACK;
				uncle->color = BLACK;
				z->parent->parent->color = RED;
				z = z->parent->parent;
			} else {
				if (z == z->parent->left) {
					z = z->parent;
					rotate_right(t, z);
				}
				z->parent->color = BLACK;
				z->parent->parent->color = RED;
				rotate_left(t, z->parent->parent);
			}
		}
	}
	t->root->color = BLACK; /* rule 2 */
}

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

	/* `cur` is the empty slot: link a new red node as `parent`'s child on
	 * the `cmp` side, or as t->root when parent == NULL. */
	struct rb_node *node = node_alloc(key, value);
	if (node == NULL)
		return -1; /* allocation failed: tree unchanged, value not consumed */

	node->parent = parent;
	if (parent == NULL)
		t->root = node;
	else if (cmp < 0)
		parent->left = node;
	else
		parent->right = node;

	t->size++;
	insert_fixup(t, node);
	return 0;
}

void *rb_find(const rbtree_t *t, const char *key)
{
	struct rb_node *cur = t->root;

	/* Invariant: if `key` is present, it lies in the subtree at `cur`. */
	while (cur != NULL) {
		int cmp = strcmp(key, cur->key);
		if (cmp == 0)
			return cur->value;
		cur = (cmp < 0) ? cur->left : cur->right;
	}
	return NULL;
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
