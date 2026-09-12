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
			/* Overwrite: the tree is the old value's only remaining
			 * owner, so it frees it before installing the new one.
			 * Key and topology are already correct -- nothing else
			 * to touch. */
			if (t->value_free != NULL)
				t->value_free(cur->value);
			cur->value = value;
			return 0;
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

/* Replaces the subtree at u with the subtree at v (v may be NULL): fixes
 * u->parent's child pointer, and v->parent when v is non-NULL. Does not
 * touch u's own fields -- callers extract whatever they need from u first. */
static void transplant(rbtree_t *t, struct rb_node *u, struct rb_node *v)
{
	if (u->parent == NULL)
		t->root = v;
	else if (u == u->parent->left)
		u->parent->left = v;
	else
		u->parent->right = v;

	if (v != NULL)
		v->parent = u->parent;
}

/* The leftmost node of the subtree at n: the in-order successor's start
 * point when n is some node's right child. */
static struct rb_node *tree_minimum(struct rb_node *n)
{
	while (n->left != NULL)
		n = n->left;
	return n;
}

/* Repairs the doubly-black debt left behind at the (parent, x) slot after
 * a black node was removed. x may be NULL, so parent is tracked explicitly
 * rather than read off x->parent. Invariant at the top of each iteration:
 * every path through x is one black short of every other path in the tree;
 * case 2 pushes that debt up toward the root (parent = x->parent is safe
 * there because x has just been set to the old, definitely non-NULL,
 * parent); cases 1/3/4 settle it in place with <=3 rotations total
 * (Section 3, "Keep the two repairs straight"). */
static void delete_fixup(rbtree_t *t, struct rb_node *parent, struct rb_node *x)
{
	while (x != t->root && (x == NULL || x->color == BLACK)) {
		if (x == parent->left) {
			struct rb_node *sib = parent->right;

			if (sib->color == RED) {
				/* Case 1: red sibling -- rotate to reach a black one */
				sib->color = BLACK;
				parent->color = RED;
				rotate_left(t, parent);
				sib = parent->right;
			}
			if ((sib->left == NULL || sib->left->color == BLACK) &&
			    (sib->right == NULL || sib->right->color == BLACK)) {
				/* Case 2: both nephews black -- recolor, move debt up */
				sib->color = RED;
				x = parent;
				parent = x->parent;
			} else {
				if (sib->right == NULL || sib->right->color == BLACK) {
					/* Case 3: near nephew red -- rotate it into place */
					if (sib->left != NULL)
						sib->left->color = BLACK;
					sib->color = RED;
					rotate_right(t, sib);
					sib = parent->right;
				}
				/* Case 4: far nephew red -- settle the debt, done */
				sib->color = parent->color;
				parent->color = BLACK;
				if (sib->right != NULL)
					sib->right->color = BLACK;
				rotate_left(t, parent);
				x = t->root;
			}
		} else {
			/* Mirror: x hangs off parent's right */
			struct rb_node *sib = parent->left;

			if (sib->color == RED) {
				sib->color = BLACK;
				parent->color = RED;
				rotate_right(t, parent);
				sib = parent->left;
			}
			if ((sib->right == NULL || sib->right->color == BLACK) &&
			    (sib->left == NULL || sib->left->color == BLACK)) {
				sib->color = RED;
				x = parent;
				parent = x->parent;
			} else {
				if (sib->left == NULL || sib->left->color == BLACK) {
					if (sib->right != NULL)
						sib->right->color = BLACK;
					sib->color = RED;
					rotate_left(t, sib);
					sib = parent->left;
				}
				sib->color = parent->color;
				parent->color = BLACK;
				if (sib->left != NULL)
					sib->left->color = BLACK;
				rotate_right(t, parent);
				x = t->root;
			}
		}
	}
	if (x != NULL)
		x->color = BLACK;
}

int rb_delete(rbtree_t *t, const char *key)
{
	struct rb_node *z = t->root;

	/* Same descent shape as rb_find. */
	while (z != NULL) {
		int cmp = strcmp(key, z->key);
		if (cmp == 0)
			break;
		z = (cmp < 0) ? z->left : z->right;
	}
	if (z == NULL)
		return -1; /* absent: tree unchanged */

	struct rb_node *fixup_parent;
	struct rb_node *x;
	rb_color_t      removed_color;

	if (z->left != NULL && z->right != NULL) {
		/* Two children: hoist the in-order successor's payload into z,
		 * then physically unlink the successor instead of z. Because
		 * z never moves, this needs no special case even when the
		 * successor is z's own right child. */
		struct rb_node *s = tree_minimum(z->right);

		free(z->key); /* old payload is about to be overwritten */
		if (t->value_free != NULL)
			t->value_free(z->value);

		z->key   = s->key;
		z->value = s->value;

		removed_color = s->color;
		fixup_parent  = s->parent;
		x             = s->right;
		transplant(t, s, s->right);
		free(s); /* struct only -- key/value now belong to z */
	} else {
		/* Zero or one child: z itself is physically unlinked. */
		x             = (z->left != NULL) ? z->left : z->right;
		fixup_parent  = z->parent;
		removed_color = z->color;
		transplant(t, z, x);

		free(z->key);
		if (t->value_free != NULL)
			t->value_free(z->value);
		free(z);
	}

	t->size--;
	if (removed_color == BLACK)
		delete_fixup(t, fixup_parent, x);
	return 0;
}

size_t rb_size(const rbtree_t *t)
{
	
	return t->size;
}

static void foreach_rec(const struct rb_node *n,
                         void (*fn)(const char *key, void *value, void *ctx),
                         void *ctx)
{
	if (n == NULL)
		return;
	foreach_rec(n->left, fn, ctx);
	fn(n->key, n->value, ctx);
	foreach_rec(n->right, fn, ctx);
}

void rb_foreach(const rbtree_t *t,
                void (*fn)(const char *key, void *value, void *ctx),
                void *ctx)
{
	foreach_rec(t->root, fn, ctx);
}

/* In-order walk that checks rules 4 and 5 together and counts nodes.
 * Returns the black-height of the subtree at n (NIL counts as black, so an
 * empty subtree is height 1); clears *ok the first time any rule breaks,
 * but keeps walking so the rest of the tree -- ordering included -- is
 * still checked. *prev is the previously-visited (in-order) node, so keys
 * can be compared as strictly increasing. */
static int validate_rec(const struct rb_node *n, const struct rb_node **prev,
                         size_t *count, int *ok)
{
	int lh, rh;

	if (n == NULL)
		return 1;

	lh = validate_rec(n->left, prev, count, ok);

	if (n->color == RED &&
	    ((n->left  != NULL && n->left->color  == RED) ||
	     (n->right != NULL && n->right->color == RED)))
		*ok = 0; /* rule 4: a red node has a red child */

	if (*prev != NULL && strcmp((*prev)->key, n->key) >= 0)
		*ok = 0; /* keys must be strictly increasing in-order */
	*prev = n;
	(*count)++;

	rh = validate_rec(n->right, prev, count, ok);

	if (lh != rh)
		*ok = 0; /* rule 5: black-height mismatch */

	return (n->color == BLACK) + lh;
}

int rb_validate(const rbtree_t *t)
{
	const struct rb_node *prev = NULL;
	size_t count = 0;
	int ok = 1;

	if (t->root != NULL && t->root->color != BLACK)
		return -1; /* rule 2: root must be black */

	validate_rec(t->root, &prev, &count, &ok);

	if (!ok || count != t->size)
		return -1;
	return 0;
}

/* Frees the allocations one node owns: its key copy and, if the tree owns
 * values, its value via value_free -- then the node struct itself. Does not
 * touch left/right/parent; callers are responsible for detaching n first. */
static void node_release(struct rb_node *n, rb_value_free_fn value_free)
{
	free(n->key);
	if (value_free != NULL)
		value_free(n->value);
	free(n);
}

static void destroy_rec(struct rb_node *n, rb_value_free_fn value_free)
{
	if (n == NULL)
		return;
	destroy_rec(n->left, value_free);
	destroy_rec(n->right, value_free);
	node_release(n, value_free);
}

void rb_destroy(rbtree_t *t)
{
	if (t == NULL)
		return; /* NULL-safe per include/rbtree.h */

	destroy_rec(t->root, t->value_free);
	free(t);
}
