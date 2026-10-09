#include "rbtree.h"

#include "fault_alloc.h"
#include "pool.h"

#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Assignment 2: the logical tree is unchanged from A1. All heap allocation
 * goes through rb_malloc / rb_free (tests/fault_alloc.h) -- the seam the
 * fault injector (M1) and the slab pool (M2) sit behind.
 * ------------------------------------------------------------------------- */

typedef enum { RED, BLACK } rb_color_t;

/* NIL is a NULL child, consistently treated as black. There is no parent
 * pointer (M4a): insert/delete record the root-to-node path in a small
 * on-stack array, so a node can later be shared between trees. */
struct rb_node {
	char           *key;    /* heap copy; the tree owns it */
	void           *value;  /* ownership per include/rbtree.h */
	struct rb_node *left;
	struct rb_node *right;
	rb_color_t      color;
};

struct rbtree {
	struct rb_node  *root;
	rb_value_free_fn value_free;
	size_t           size;
	rb_pool_t       *pool;  /* node storage; NULL = plain rb_malloc */
};

/* Node structs come from the tree's pool when it has one (M2). Keys always
 * come from rb_malloc. */
static struct rb_node *node_struct_alloc(rbtree_t *t)
{
	return t->pool != NULL ? pool_alloc(t->pool) : rb_malloc(sizeof(struct rb_node));
}

static void node_struct_free(rbtree_t *t, struct rb_node *n)
{
	if (t->pool != NULL)
		pool_free(t->pool, n);
	else
		rb_free(n);
}

/* The entire allocation dance for one node: node, then key copy, with
 * goto-cleanup if the second allocation fails. Colors the node RED (every
 * new key enters red -- Section 3) and NULLs its links; caller splices it
 * in. Returns NULL on any allocation failure with nothing left allocated. */
static struct rb_node *node_alloc(rbtree_t *t, const char *key, void *value)
{
	struct rb_node *n = node_struct_alloc(t);
	if (n == NULL)
		goto fail_node;

	size_t klen = strlen(key) + 1;
	n->key = rb_malloc(klen);
	if (n->key == NULL)
		goto fail_key;
	memcpy(n->key, key, klen);

	n->value  = value;
	n->left   = NULL;
	n->right  = NULL;
	n->color  = RED;
	return n;

fail_key:
	node_struct_free(t, n);
fail_node:
	return NULL;
}

/* Frees just the payload a node owns: its key copy, and its value via
 * value_free if the tree owns values. Not the node struct itself -- for
 * a node whose struct stays alive (rb_delete's two-children hoist). */
static void node_release_payload(struct rb_node *n, rb_value_free_fn value_free)
{
	rb_free(n->key);
	if (value_free != NULL)
		value_free(n->value);
}

/* Frees everything one node owns: its payload, then the struct itself.
 * Does not touch left/right; callers are responsible for detaching
 * n first. */
static void node_release(rbtree_t *t, struct rb_node *n)
{
	node_release_payload(n, t->value_free);
	node_struct_free(t, n);
}

/* Longest root-to-node path insert/delete ever record. A red-black tree has
 * height <= 2*log2(n+1), so 128 covers any size a 64-bit size_t can count. */
#define RB_MAX_DEPTH 128

/* Makes `neu` take `old`'s place under `p`; p == NULL means `old` is the root. */
static void replace_child(rbtree_t *t, struct rb_node *p,
                          struct rb_node *old, struct rb_node *neu)
{
	if (p == NULL)
		t->root = neu;
	else if (p->left == old)
		p->left = neu;
	else
		p->right = neu;
}

/* Left-rotate around x (Figure 2); xp is x's parent (NULL when x is the
 * root), so t->root is updated through replace_child. */
static void rotate_left(rbtree_t *t, struct rb_node *xp, struct rb_node *x)
{
	struct rb_node *y = x->right;

	x->right = y->left;
	replace_child(t, xp, x, y);
	y->left = x;
}

/* Mirror of rotate_left. */
static void rotate_right(rbtree_t *t, struct rb_node *xp, struct rb_node *x)
{
	struct rb_node *y = x->left;

	x->left = y->right;
	replace_child(t, xp, x, y);
	y->right = x;
}

/* Repairs the red-red violation insertion may have introduced at p[i], where
 * p[0..i] is the root-to-z path (z included). Invariant at the top of each
 * iteration: p[i] is red, and the only possible rule-4 breach in the whole
 * tree is the edge p[i]--p[i-1]. Case 1 pushes the violation two levels
 * toward the root (so the climb terminates in O(log n) steps); cases 2-3 fix
 * it in place with <=2 rotations total (Section 3, Figures 3-5). uncle ==
 * NULL is read as black, per the NULL-is-NIL representation. */
static void insert_fixup(rbtree_t *t, struct rb_node **p, int i)
{
	while (i >= 1 && p[i - 1]->color == RED) {
		struct rb_node *parent = p[i - 1];
		struct rb_node *gp     = p[i - 2]; /* parent is red, so not the root */
		struct rb_node *ggp    = (i >= 3) ? p[i - 3] : NULL;

		if (parent == gp->left) {
			struct rb_node *uncle = gp->right;

			if (uncle != NULL && uncle->color == RED) {
				/* Case 1: red uncle -- recolor, move up */
				parent->color = BLACK;
				uncle->color = BLACK;
				gp->color = RED;
				i -= 2;
			} else {
				if (p[i] == parent->right) {
					/* Case 2: triangle -- rotate to a line; the old z now
					 * sits above `parent` in the path */
					rotate_left(t, gp, parent);
					p[i - 1] = p[i];
					p[i]     = parent;
					parent   = p[i - 1];
				}
				/* Case 3: line -- rotate at grandparent, swap colors */
				parent->color = BLACK;
				gp->color = RED;
				rotate_right(t, ggp, gp);
			}
		} else {
			/* Mirror: uncle hangs off the grandparent's left */
			struct rb_node *uncle = gp->left;

			if (uncle != NULL && uncle->color == RED) {
				parent->color = BLACK;
				uncle->color = BLACK;
				gp->color = RED;
				i -= 2;
			} else {
				if (p[i] == parent->left) {
					rotate_right(t, gp, parent);
					p[i - 1] = p[i];
					p[i]     = parent;
					parent   = p[i - 1];
				}
				parent->color = BLACK;
				gp->color = RED;
				rotate_left(t, ggp, gp);
			}
		}
	}
	t->root->color = BLACK; /* rule 2 */
}

rbtree_t *rb_create(rb_value_free_fn value_free)
{

	rbtree_t *t = rb_malloc(sizeof *t);
	if (t == NULL)
		return NULL; /* allocation failed: nothing to unwind */

	t->root = NULL;
	t->size = 0;
	t->value_free = value_free; /* may be NULL: tree does not own values */
	t->pool = NULL;
	return t;
}

rbtree_t *rb_create_pooled(rb_value_free_fn value_free)
{
	rbtree_t *t = rb_create(value_free);
	if (t == NULL)
		goto fail_tree;

	t->pool = pool_create(sizeof(struct rb_node));
	if (t->pool == NULL)
		goto fail_pool;
	return t;

fail_pool:
	rb_free(t);
fail_tree:
	return NULL;
}

int rb_insert(rbtree_t *t, const char *key, void *value)
{
	struct rb_node *path[RB_MAX_DEPTH];
	int depth = 0;
	struct rb_node *cur = t->root;
	int cmp = 0;

	/* BST descent to the insertion point.
	 * Invariant: path[0..depth) are the nodes above `cur` on the search
	 * path, root first; `cur` is the subtree left to examine. */
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
		path[depth++] = cur;
		cur = (cmp < 0) ? cur->left : cur->right;
	}

	/* `cur` is the empty slot: link a new red node as path[depth-1]'s child
	 * on the `cmp` side, or as t->root when the path is empty. */
	struct rb_node *node = node_alloc(t, key, value);
	if (node == NULL)
		return -1; /* allocation failed: tree unchanged, value not consumed */

	if (depth == 0)
		t->root = node;
	else if (cmp < 0)
		path[depth - 1]->left = node;
	else
		path[depth - 1]->right = node;

	path[depth] = node;
	t->size++;
	insert_fixup(t, path, depth);
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

/* Replaces the subtree rooted at u with the one rooted at v (which may be
 * NULL): rewrites up's child pointer, or t->root when up is NULL. u's own
 * links are left alone. */
static void transplant(rbtree_t *t, struct rb_node *up,
                       struct rb_node *u, struct rb_node *v)
{
	replace_child(t, up, u, v);
}

/* Repairs the doubly-black debt left behind at slot x after a black node was
 * removed. path[0..d) are x's ancestors, root first, so path[d-1] is its
 * parent (x may be NULL, so it cannot be read off x). Invariant at the top of
 * each iteration: every path through x is one black short of every other path
 * in the tree; case 2 pushes that debt up toward the root (d--); cases 1/3/4
 * settle it in place with <=3 rotations total (Section 3, "Keep the two
 * repairs straight"). */
static void delete_fixup(rbtree_t *t, struct rb_node **path, int d,
                         struct rb_node *x)
{
	while (x != t->root && (x == NULL || x->color == BLACK)) {
		struct rb_node *parent = path[d - 1];
		struct rb_node *pp     = (d >= 2) ? path[d - 2] : NULL;

		if (x == parent->left) {
			struct rb_node *sib = parent->right;

			if (sib->color == RED) {
				/* Case 1: red sibling -- rotate to reach a black one; sib
				 * is now parent's parent in the path */
				sib->color = BLACK;
				parent->color = RED;
				rotate_left(t, pp, parent);
				path[d - 1] = sib;
				path[d]     = parent;
				d++;
				pp  = sib;
				sib = parent->right;
			}
			if ((sib->left == NULL || sib->left->color == BLACK) &&
			    (sib->right == NULL || sib->right->color == BLACK)) {
				/* Case 2: both nephews black -- recolor, move debt up */
				sib->color = RED;
				x = parent;
				d--;
			} else {
				if (sib->right == NULL || sib->right->color == BLACK) {
					/* Case 3: near nephew red -- rotate it into place */
					if (sib->left != NULL)
						sib->left->color = BLACK;
					sib->color = RED;
					rotate_right(t, parent, sib);
					sib = parent->right;
				}
				/* Case 4: far nephew red -- settle the debt, done */
				sib->color = parent->color;
				parent->color = BLACK;
				if (sib->right != NULL)
					sib->right->color = BLACK;
				rotate_left(t, pp, parent);
				x = t->root;
			}
		} else {
			/* Mirror: x hangs off parent's right */
			struct rb_node *sib = parent->left;

			if (sib->color == RED) {
				sib->color = BLACK;
				parent->color = RED;
				rotate_right(t, pp, parent);
				path[d - 1] = sib;
				path[d]     = parent;
				d++;
				pp  = sib;
				sib = parent->left;
			}
			if ((sib->right == NULL || sib->right->color == BLACK) &&
			    (sib->left == NULL || sib->left->color == BLACK)) {
				sib->color = RED;
				x = parent;
				d--;
			} else {
				if (sib->left == NULL || sib->left->color == BLACK) {
					if (sib->right != NULL)
						sib->right->color = BLACK;
					sib->color = RED;
					rotate_left(t, parent, sib);
					sib = parent->left;
				}
				sib->color = parent->color;
				parent->color = BLACK;
				if (sib->left != NULL)
					sib->left->color = BLACK;
				rotate_right(t, pp, parent);
				x = t->root;
			}
		}
	}
	if (x != NULL)
		x->color = BLACK;
}

int rb_delete(rbtree_t *t, const char *key)
{
	struct rb_node *path[RB_MAX_DEPTH];
	int depth = 0;
	struct rb_node *z = t->root;

	/* Invariant: if key is present, it lies in the subtree at z; path[0..depth)
	 * are the nodes above z (same shape as rb_find's descent). */
	while (z != NULL) {
		int cmp = strcmp(key, z->key);
		if (cmp == 0)
			break;
		path[depth++] = z;
		z = (cmp < 0) ? z->left : z->right;
	}
	if (z == NULL)
		return -1; /* absent: tree unchanged */

	struct rb_node *x;
	rb_color_t      removed_color;

	if (z->left != NULL && z->right != NULL) {
		/* Two children: hoist the in-order successor's payload into z,
		 * then physically unlink the successor instead of z. Because
		 * z never moves, this needs no special case even when the
		 * successor is z's own right child. */
		struct rb_node *s = z->right;

		path[depth++] = z;
		/* Invariant: path[0..depth) are the ancestors of s. */
		while (s->left != NULL) {
			path[depth++] = s;
			s = s->left;
		}

		node_release_payload(z, t->value_free); /* old payload, z survives */

		z->key   = s->key;
		z->value = s->value;

		removed_color = s->color;
		x             = s->right;
		transplant(t, path[depth - 1], s, s->right);
		node_struct_free(t, s); /* struct only -- key/value now belong to z */
	} else {
		/* Zero or one child: z itself is physically unlinked. */
		x             = (z->left != NULL) ? z->left : z->right;
		removed_color = z->color;
		transplant(t, depth > 0 ? path[depth - 1] : NULL, z, x);

		node_release(t, z); /* z itself is being destroyed */
	}

	t->size--;
	if (removed_color == BLACK)
		delete_fixup(t, path, depth, x);
	return 0;
}

size_t rb_size(const rbtree_t *t)
{
	
	return t->size;
}

/* In-order walk in O(1) space with no parent pointers: each step finds the
 * successor by a fresh search from the root (O(log n)), so the whole walk is
 * O(n log n) time but needs no recursion, stack, or tree mutation. `fn` must
 * not insert or delete during the walk. */
void rb_foreach(const rbtree_t *t,
                void (*fn)(const char *key, void *value, void *ctx),
                void *ctx)
{
	const struct rb_node *n = t->root;
	while (n != NULL && n->left != NULL)
		n = n->left;

	/* Invariant: every key before n has been visited, none after; n is next. */
	while (n != NULL) {
		fn(n->key, n->value, ctx);

		/* Successor: the smallest key greater than n's. */
		const struct rb_node *next = NULL;
		const struct rb_node *cur  = t->root;
		while (cur != NULL) {
			if (strcmp(n->key, cur->key) < 0) {
				next = cur;
				cur  = cur->left;
			} else {
				cur = cur->right;
			}
		}
		n = next;
	}
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

/* The Reach (Section 10): tears down n in O(1) auxiliary space and no
 * recursion by rotating every node onto a right spine as it's freed
 * instead of stacking unvisited left subtrees. Invariant: everything left
 * to free is still reachable from n, so nothing needs remembering. */
static void destroy_spine(rbtree_t *t, struct rb_node *n)
{
	while (n != NULL) {
		if (n->left == NULL) {
			struct rb_node *next = n->right;
			node_release(t, n);
			n = next;
		} else {
			struct rb_node *l = n->left;
			n->left  = l->right;
			l->right = n;
			n = l;
		}
	}
}

void rb_destroy(rbtree_t *t)
{
	if (t == NULL)
		return; /* NULL-safe per include/rbtree.h */

	destroy_spine(t, t->root);
	pool_destroy(t->pool); /* NULL-safe */
	rb_free(t);
}

/* Not part of the public API (include/rbtree.h is frozen) -- has external
 * linkage only so tests can reach it via their own extern declaration,
 * per the companion's "behind a second function" suggestion. Ungraded;
 * touches nothing rb_destroy or its callers depend on. */
void rb_destroy_reach(rbtree_t *t)
{
	if (t == NULL)
		return;

	destroy_spine(t, t->root);
	pool_destroy(t->pool);
	rb_free(t);
}
