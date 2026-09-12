#include "rbtree.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The Reach (Section 10): not part of the public API, so not declared in
 * include/rbtree.h (frozen). Forward-declared here so this file can reach
 * it directly, per the companion's "behind a second function" suggestion. */
extern void rb_destroy_reach(rbtree_t *t);

/* M1 unit tests, M2's table-driven rb_delete cases (written before
 * rb_delete existed -- workflow Step 4 -- so they failed red first), and
 * M3 hardening/edge cases, in that order. The delete-case fixture trees
 * were built purely through rb_insert (already implemented and verified),
 * then inspected with a throwaway whitebox dump tool to confirm each
 * target node's exact role before it went into the table. See
 * PROMPTLOG.md. */

static int failures = 0;

#define CHECK(cond, msg)                          \
	do {                                        \
		if (!(cond)) {                       \
			printf("  FAIL: %s\n", msg); \
			failures++;                  \
		}                                    \
	} while (0)

struct delete_case {
	const char        *name;
	const char *const *build;      /* insertion order that builds the fixture */
	size_t             build_n;
	const char        *delete_key; /* which key this row deletes */
};

static rbtree_t *build_tree(const char *const *keys, size_t n)
{
	rbtree_t *t = rb_create(NULL);
	for (size_t i = 0; i < n; i++) {
		/* Value must be non-NULL: rb_find returns NULL both for "absent"
		 * and for "present with a NULL value" (see include/rbtree.h),
		 * and these tests tell the two apart by checking for NULL. The
		 * key's own storage is a convenient non-NULL, stable pointer. */
		if (rb_insert(t, keys[i], (void *)keys[i]) != 0) {
			printf("  setup FAILED: rb_insert(\"%s\") rejected\n", keys[i]);
			failures++;
		}
	}
	return t;
}

static void run_case(const struct delete_case *c)
{
	printf("case: %s (delete \"%s\")\n", c->name, c->delete_key);

	rbtree_t *t = build_tree(c->build, c->build_n);
	size_t    before = rb_size(t);

	CHECK(rb_validate(t) == 0, "fixture tree is invalid before the delete");

	int rc = rb_delete(t, c->delete_key);
	CHECK(rc == 0, "rb_delete returned nonzero");
	CHECK(rb_find(t, c->delete_key) == NULL, "deleted key is still findable");
	CHECK(rb_size(t) == before - 1, "rb_size did not drop by exactly one");
	CHECK(rb_validate(t) == 0, "rb_validate failed after the delete");

	/* invariant: every OTHER key from the build list must still resolve */
	for (size_t i = 0; i < c->build_n; i++) {
		if (strcmp(c->build[i], c->delete_key) == 0)
			continue;
		if (rb_find(t, c->build[i]) == NULL) {
			printf("  FAIL: key \"%s\" went missing\n", c->build[i]);
			failures++;
		}
	}

	rb_destroy(t);
}

/* ---------------------------------------------------------------------------
 * M1: insert / find / overwrite / foreach / destroy.
 * ------------------------------------------------------------------------- */

static void test_insert_find(void)
{
	printf("test: insert + find basics\n");
	rbtree_t *t = rb_create(NULL);
	static const char *const keys[] = { "m", "c", "x", "a", "g", "p", "z" };
	size_t n = sizeof keys / sizeof keys[0];

	for (size_t i = 0; i < n; i++)
		CHECK(rb_insert(t, keys[i], (void *)keys[i]) == 0, "insert failed");
	CHECK(rb_size(t) == n, "size mismatch after inserts");
	CHECK(rb_validate(t) == 0, "tree invalid after inserts");

	for (size_t i = 0; i < n; i++)
		CHECK(rb_find(t, keys[i]) == keys[i], "find returned wrong/missing value");
	CHECK(rb_find(t, "nope") == NULL, "find on an absent key returned non-NULL");

	rb_destroy(t);
}

static int owf_calls = 0;
static void owf_count(void *v) { owf_calls++; free(v); }

static void test_insert_overwrite(void)
{
	printf("test: insert overwrite (ownership)\n");
	rbtree_t *t = rb_create(owf_count);
	int *a = malloc(sizeof *a); *a = 1;
	int *b = malloc(sizeof *b); *b = 2;
	owf_calls = 0;

	CHECK(rb_insert(t, "dup", a) == 0, "first insert failed");
	CHECK(owf_calls == 0, "value_free called before any overwrite");

	CHECK(rb_insert(t, "dup", b) == 0, "overwrite insert returned nonzero");
	CHECK(owf_calls == 1, "overwrite should free the old value exactly once");
	CHECK(rb_size(t) == 1, "overwrite must not change tree size");

	int *found = rb_find(t, "dup");
	CHECK(found != NULL && *found == 2, "find after overwrite returned the old value");

	rb_destroy(t); /* should free b (owf_calls -> 2), never a again */
	CHECK(owf_calls == 2, "destroy should free exactly the current value once");
}

static char foreach_buf[256];
static void foreach_collect(const char *key, void *value, void *ctx)
{
	(void)value;
	strncat((char *)ctx, key, sizeof foreach_buf - strlen(ctx) - 2);
	strncat((char *)ctx, ",", sizeof foreach_buf - strlen(ctx) - 2);
}

static void test_foreach_inorder(void)
{
	printf("test: foreach visits in sorted order\n");
	rbtree_t *t = rb_create(NULL);
	static const char *const keys[] = { "m", "c", "x", "a", "g", "p", "z" };
	for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++)
		rb_insert(t, keys[i], NULL);

	foreach_buf[0] = '\0';
	rb_foreach(t, foreach_collect, foreach_buf);
	CHECK(strcmp(foreach_buf, "a,c,g,m,p,x,z,") == 0, "foreach did not visit in sorted order");

	rb_destroy(t);
}

static void test_destroy_null_and_empty(void)
{
	printf("test: destroy(NULL) and destroy(empty)\n");
	rb_destroy(NULL); /* must not crash */

	rbtree_t *t = rb_create(NULL);
	rb_destroy(t); /* empty tree, no nodes to leak */
}

/* The Reach: same coverage rb_destroy already gets (a real tree, shaped
 * by both inserts and deletes so it isn't just a straight spine already),
 * torn down with rb_destroy_reach instead. Nothing here checks the
 * rotation logic directly -- ASan is the oracle: if destroy_spine ever
 * drops a node's left subtree instead of rotating it up, that subtree
 * leaks, and make asan catches it. */
static void test_destroy_reach(void)
{
	printf("test: the Reach (stack-free teardown)\n");
	rbtree_t *t = rb_create(NULL);
	static const char *const keys[] = { "u", "q", "s", "c", "e", "g", "i", "m", "o", "k" };
	for (size_t i = 0; i < 10; i++)
		rb_insert(t, keys[i], (void *)keys[i]);
	rb_delete(t, "u");
	rb_delete(t, "m");
	rb_insert(t, "z", (void *)"z"); /* re-inserted after a delete, on purpose */

	rb_destroy_reach(t);

	/* empty tree through the same path */
	rbtree_t *empty = rb_create(NULL);
	rb_destroy_reach(empty);
}

/* ---------------------------------------------------------------------------
 * M3: hardening / edge cases.
 * ------------------------------------------------------------------------- */

static void test_empty_tree_ops(void)
{
	printf("test: M3 empty tree\n");
	rbtree_t *t = rb_create(NULL);
	CHECK(rb_find(t, "x") == NULL, "find on empty tree should be NULL");
	CHECK(rb_delete(t, "x") == -1, "delete on empty tree should be -1");
	CHECK(rb_validate(t) == 0, "empty tree should validate");
	CHECK(rb_size(t) == 0, "empty tree size should be 0");
	rb_destroy(t);
}

static void test_single_node(void)
{
	printf("test: M3 single-node tree\n");
	rbtree_t *t = rb_create(NULL);
	CHECK(rb_insert(t, "only", (void *)"only") == 0, "insert failed");
	CHECK(rb_validate(t) == 0, "single-node tree should validate");
	CHECK(rb_size(t) == 1, "single-node size should be 1");
	CHECK(rb_delete(t, "only") == 0, "deleting the only node should succeed");
	CHECK(rb_size(t) == 0, "size should be 0 after deleting the only node");
	CHECK(rb_validate(t) == 0, "tree should validate after emptying");
	rb_destroy(t);
}

static void test_overwrite_only_key(void)
{
	printf("test: M3 overwrite the only key\n");
	rbtree_t *t = rb_create(NULL);
	static char v1[] = "v1", v2[] = "v2";
	CHECK(rb_insert(t, "only", v1) == 0, "first insert failed");
	CHECK(rb_insert(t, "only", v2) == 0, "overwrite failed");
	CHECK(rb_size(t) == 1, "overwriting the only key must not change size");
	CHECK(rb_find(t, "only") == v2, "overwrite did not take effect");
	CHECK(rb_validate(t) == 0, "single overwritten node should validate");
	rb_destroy(t);
}

static void test_long_key(void)
{
	printf("test: M3 very long key\n");
	rbtree_t *t = rb_create(NULL);
	static char longkey[10000];
	memset(longkey, 'a', sizeof longkey - 1);
	longkey[sizeof longkey - 1] = '\0';

	CHECK(rb_insert(t, longkey, longkey) == 0, "insert of a 10000-byte key failed");
	CHECK(rb_find(t, longkey) == longkey, "find of a 10000-byte key failed");
	CHECK(rb_validate(t) == 0, "tree with a long key should validate");
	CHECK(rb_delete(t, longkey) == 0, "delete of a 10000-byte key failed");
	CHECK(rb_size(t) == 0, "size should be 0 after deleting the long key");
	rb_destroy(t);
}

int main(void)
{
	test_insert_find();
	test_insert_overwrite();
	test_foreach_inorder();
	test_destroy_null_and_empty();
	test_destroy_reach();

	static const char *const tree_a[] = { "u", "q", "s", "c", "e", "g", "i", "m", "o", "k" };
	static const char *const tree_b[] = { "e", "q", "c", "s", "u", "o", "m", "k", "g", "i" };

	struct delete_case cases[] = {
		/* tree_a: root i(B); left e(B)[c,g black leaves];
		 * right s(B)[o(R)[m(B)[k(R) leaf]], u(B) leaf]. */
		{ "red leaf",                            tree_a, 10, "k" },
		{ "black leaf, red sibling",              tree_a, 10, "u" },
		{ "two children",                         tree_a, 10, "e" },
		{ "root deletion",                        tree_a, 10, "i" },
		{ "black node, one red child",            tree_a, 10, "m" },
		/* tree_b: mirror of the two orientation-sensitive cases above. */
		{ "black leaf, red sibling (mirror)",     tree_b, 10, "c" },
		{ "black node, one red child (mirror)",   tree_b, 10, "g" },
	};

	for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++)
		run_case(&cases[i]);

	/* deleting a key that was never inserted must fail cleanly */
	{
		printf("case: absent key\n");
		rbtree_t *t      = build_tree(tree_a, 10);
		size_t    before = rb_size(t);
		CHECK(rb_delete(t, "zz") == -1, "rb_delete on an absent key did not return -1");
		CHECK(rb_size(t) == before, "tree size changed on a failed delete");
		CHECK(rb_validate(t) == 0, "tree invalid after a failed delete");
		rb_destroy(t);
	}

	test_empty_tree_ops();
	test_single_node();
	test_overwrite_only_key();
	test_long_key();

	if (failures == 0) {
		printf("test_rbtree: all cases passed\n");
		return 0;
	}
	printf("test_rbtree: %d failure(s)\n", failures);
	return 1;
}
