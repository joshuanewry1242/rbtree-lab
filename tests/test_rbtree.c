#include "rbtree.h"

#include <stdio.h>
#include <string.h>

/* M2: table-driven rb_delete case tests, written before rb_delete exists
 * (workflow Step 4) so they fail red first. Every row targets a *known*
 * shape, not a hoped-for one: both fixture trees below were built purely
 * through rb_insert (already implemented and verified), then inspected
 * with a throwaway whitebox dump tool to confirm each target node's exact
 * role before it went into this table. See PROMPTLOG.md. */

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

int main(void)
{
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

	if (failures == 0) {
		printf("test_rbtree: all cases passed\n");
		return 0;
	}
	printf("test_rbtree: %d failure(s)\n", failures);
	return 1;
}
