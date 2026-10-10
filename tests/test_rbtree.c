#include "rbtree.h"

#include "fault_alloc.h"
#include "../src/pool.h"

#include <pthread.h>
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

/* ---------------------------------------------------------------------------
 * A2 M1: allocation-failure sweeps via the fault injector.
 * ------------------------------------------------------------------------- */

static const char *const fault_keys[] = { "u", "q", "s", "c", "e", "g", "i", "m", "o", "k" };
#define FAULT_NKEYS (sizeof fault_keys / sizeof fault_keys[0])

/* In-order key list, copied out so it can be compared across a failed call. */
struct key_snap {
	char   keys[FAULT_NKEYS + 1][8];
	size_t n;
};

static void snap_cb(const char *key, void *value, void *ctx)
{
	(void)value;
	struct key_snap *s = ctx;
	if (s->n < FAULT_NKEYS + 1)
		snprintf(s->keys[s->n], sizeof s->keys[0], "%s", key);
	s->n++;
}

static int snap_equal(const struct key_snap *a, const struct key_snap *b)
{
	if (a->n != b->n)
		return 0;
	for (size_t i = 0; i < a->n; i++)
		if (strcmp(a->keys[i], b->keys[i]) != 0)
			return 0;
	return 1;
}

/* Tree that owns heap values, so a leaked or double-freed value shows up
 * under asan / leaks. Built with the injector disarmed. */
static rbtree_t *build_owned_tree(int pooled)
{
	rbtree_t *t = pooled ? rb_create_pooled(free) : rb_create(free);
	for (size_t i = 0; i < FAULT_NKEYS; i++) {
		char *v = strdup(fault_keys[i]);
		if (rb_insert(t, fault_keys[i], v) != 0) {
			printf("  setup FAILED: rb_insert(\"%s\") rejected\n", fault_keys[i]);
			failures++;
			free(v);
		}
	}
	return t;
}

static void test_fault_create(void)
{
	printf("test: M1 rb_create under allocation failure\n");
	fault_alloc_arm(1);
	rbtree_t *t = rb_create(NULL);
	int fired = fault_alloc_fired();
	fault_alloc_disarm();
	CHECK(fired, "armed fault did not fire during rb_create");
	CHECK(t == NULL, "rb_create did not return NULL on allocation failure");
	rb_destroy(t);
}

static void test_fault_insert_sweep(int pooled)
{
	printf("test: %s rb_insert fault sweep\n", pooled ? "M2 pooled" : "M1");
	int failed_iters = 0;

	/* Invariant: at iteration n, allocations 1..n-1 of the insert succeed
	 * and the n-th fails; the loop ends on the first n that never fires. */
	for (long n = 1;; n++) {
		rbtree_t       *t = build_owned_tree(pooled);
		struct key_snap before = { 0 };
		rb_foreach(t, snap_cb, &before);
		size_t size_before = rb_size(t);

		char *v = strdup("new");
		fault_alloc_arm(n);
		int rc    = rb_insert(t, "new", v);
		int fired = fault_alloc_fired();
		fault_alloc_disarm();

		if (!fired) {
			CHECK(rc == 0, "rb_insert failed with no fault injected");
			CHECK(rb_find(t, "new") == v, "inserted key not findable");
			CHECK(rb_validate(t) == 0, "tree invalid after successful insert");
			rb_destroy(t);
			break;
		}

		failed_iters++;
		struct key_snap after = { 0 };
		rb_foreach(t, snap_cb, &after);
		CHECK(rc == -1, "rb_insert did not return -1 on allocation failure");
		CHECK(rb_size(t) == size_before, "size changed after failed insert");
		CHECK(snap_equal(&before, &after), "keys changed after failed insert");
		CHECK(rb_validate(t) == 0, "tree invalid after failed insert");
		CHECK(rb_find(t, "new") == NULL, "failed insert left the key findable");
		free(v); /* caller still owns it: double free here if the tree consumed it */
		rb_destroy(t);
	}

	/* Plain: node + key. Pooled with free slots: only the key allocates; the
	 * slab-growth failure point is covered by test_pooled_insert_faults. */
	CHECK(failed_iters >= (pooled ? 1 : 2),
	      "sweep missed a failure point (plain: node + key; pooled: key)");
}

static void test_fault_no_alloc_paths(int pooled)
{
	printf("test: %s overwrite and delete do not allocate\n", pooled ? "M2 pooled" : "M1");
	rbtree_t *t = build_owned_tree(pooled);

	fault_alloc_arm(1);
	int rc_over = rb_insert(t, "m", strdup("m2"));
	int rc_del  = rb_delete(t, "q");
	int fired   = fault_alloc_fired();
	fault_alloc_disarm();

	CHECK(!fired, "overwrite or delete performed an allocation");
	CHECK(rc_over == 0, "overwrite rb_insert failed");
	CHECK(rc_del == 0, "rb_delete failed");
	CHECK(rb_find(t, "m") != NULL && strcmp(rb_find(t, "m"), "m2") == 0,
	      "overwrite did not install the new value");
	CHECK(rb_validate(t) == 0, "tree invalid after overwrite + delete");
	rb_destroy(t);
}

/* ---------------------------------------------------------------------------
 * A2 M2: slab pool and pooled trees.
 * ------------------------------------------------------------------------- */

static void test_pool_basic(void)
{
	printf("test: M2 pool alloc/free/stats\n");
	rb_pool_t *p = pool_create(40);
	CHECK(p != NULL, "pool_create failed");
	CHECK(pool_create(5000) == NULL, "pool_create accepted an object larger than a slab");

	size_t slabs, live, freeo;
	pool_stats(p, &slabs, &live, &freeo);
	CHECK(slabs == 0 && live == 0 && freeo == 0, "fresh pool should own nothing");

	void *first = pool_alloc(p);
	CHECK(first != NULL, "first pool_alloc failed");
	pool_stats(p, &slabs, &live, &freeo);
	CHECK(slabs == 1 && live == 1, "first alloc should create exactly one slab");
	size_t per_slab = live + freeo;

	void *objs[1000];
	size_t n = 0;
	objs[n++] = first;
	/* Invariant: live + free_objs == slabs * per_slab, objs[0..n) are live. */
	while (n < 3 * per_slab) {
		objs[n] = pool_alloc(p);
		CHECK(objs[n] != NULL, "pool_alloc failed with no fault injected");
		for (size_t i = 0; i < n; i++)
			if (objs[i] == objs[n]) {
				CHECK(0, "pool_alloc returned a slot that is still live");
				break;
			}
		n++;
	}
	pool_stats(p, &slabs, &live, &freeo);
	CHECK(slabs == 3 && live == n && live + freeo == slabs * per_slab,
	      "stats invariant broken after spilling into more slabs");

	for (size_t i = 0; i < n; i++)
		pool_free(p, objs[i]);
	pool_stats(p, &slabs, &live, &freeo);
	CHECK(slabs == 3 && live == 0 && freeo == 3 * per_slab,
	      "freeing everything should keep the slabs and free every slot");

	fault_alloc_arm(1);
	void *again = pool_alloc(p);
	int fired = fault_alloc_fired();
	fault_alloc_disarm();
	CHECK(again != NULL && !fired, "reuse after free should not allocate");
	pool_free(p, again);
	pool_destroy(p);
	pool_destroy(NULL);
}

static void test_pool_poison(void)
{
	printf("test: M2 pool poisons dead slots (debug builds)\n");
#ifndef NDEBUG
	rb_pool_t *p = pool_create(48);
	unsigned char *a = pool_alloc(p);
	memset(a, 0x11, 48);
	pool_free(p, a);
	int ok = 1;
	/* the front sizeof(void*) bytes hold the free-list link; the rest is poison */
	for (size_t i = sizeof(void *); i < 48; i++)
		if (a[i] != 0xDD)
			ok = 0;
	CHECK(ok, "freed slot was not poisoned with 0xDD past the free-list link");
	pool_destroy(p);
#endif
}

static void test_pool_growth_fault(void)
{
	printf("test: M2 pool slab-growth failure leaves the pool untouched\n");
	rb_pool_t *p = pool_create(40);
	void *first = pool_alloc(p);
	size_t slabs, live, freeo;
	pool_stats(p, &slabs, &live, &freeo);
	size_t per_slab = live + freeo;
	for (size_t i = 1; i < per_slab; i++)
		pool_alloc(p); /* slab is now full: the next alloc must grow */

	fault_alloc_arm(1);
	void *r = pool_alloc(p);
	int fired = fault_alloc_fired();
	fault_alloc_disarm();
	CHECK(fired, "growing a full pool did not allocate");
	CHECK(r == NULL, "pool_alloc did not return NULL when the slab alloc failed");
	pool_stats(p, &slabs, &live, &freeo);
	CHECK(slabs == 1 && live == per_slab && freeo == 0, "failed growth changed pool stats");
	CHECK(pool_alloc(p) != NULL, "pool unusable after a failed growth");
	(void)first;
	pool_destroy(p); /* frees live objects wholesale */
}

static void test_pooled_tree(void)
{
	printf("test: M2 pooled tree behaves like a plain tree\n");
	rbtree_t *t = rb_create_pooled(free);
	char key[32];
	for (int i = 0; i < 500; i++) {
		snprintf(key, sizeof key, "k%04d", (i * 7919) % 500);
		CHECK(rb_insert(t, key, strdup(key)) == 0, "pooled insert failed");
	}
	CHECK(rb_size(t) == 500, "pooled tree size mismatch");
	CHECK(rb_validate(t) == 0, "pooled tree invalid after inserts");
	for (int i = 0; i < 500; i += 2) {
		snprintf(key, sizeof key, "k%04d", i);
		CHECK(rb_delete(t, key) == 0, "pooled delete failed");
	}
	CHECK(rb_size(t) == 250 && rb_validate(t) == 0, "pooled tree wrong after deletes");
	for (int i = 0; i < 100; i++) { /* reuse freed slots */
		snprintf(key, sizeof key, "n%04d", i);
		CHECK(rb_insert(t, key, strdup(key)) == 0, "pooled re-insert failed");
	}
	CHECK(rb_validate(t) == 0, "pooled tree invalid after reuse");
	rb_destroy(t);

	t = rb_create_pooled(NULL);
	CHECK(rb_insert(t, "a", NULL) == 0, "pooled insert failed");
	rb_destroy_reach(t);
	rb_destroy(rb_create_pooled(NULL)); /* empty pooled tree: no slab yet */
}

static void test_pooled_create_fault(void)
{
	printf("test: M2 rb_create_pooled under allocation failure\n");
	int failed_iters = 0;
	/* Invariant: allocations 1..n-1 succeed, the n-th fails; ends on first n that never fires. */
	for (long n = 1;; n++) {
		fault_alloc_arm(n);
		rbtree_t *t = rb_create_pooled(NULL);
		int fired = fault_alloc_fired();
		fault_alloc_disarm();
		if (!fired) {
			CHECK(t != NULL, "rb_create_pooled failed with no fault injected");
			rb_destroy(t);
			break;
		}
		failed_iters++;
		CHECK(t == NULL, "rb_create_pooled did not return NULL on allocation failure");
	}
	CHECK(failed_iters >= 2, "sweep covered fewer than 2 failure points (tree + pool)");
}

/* Fail the first allocation of every insert into a growing pooled tree. That
 * is the slab when the pool is full, otherwise the key copy -- so both
 * failure points are hit without the test knowing the slab geometry. */
static void test_pooled_insert_faults(void)
{
	printf("test: M2 pooled insert failures across slab boundaries\n");
	rbtree_t *t = rb_create_pooled(free);
	char key[32];
	int failures_seen = 0;
	for (int i = 0; i < 300; i++) {
		snprintf(key, sizeof key, "k%04d", (i * 7919) % 300);
		size_t before = rb_size(t);
		char *v = strdup(key);

		fault_alloc_arm(1);
		int rc = rb_insert(t, key, v);
		int fired = fault_alloc_fired();
		fault_alloc_disarm();
		CHECK(fired, "insert of a new key performed no allocation");
		CHECK(rc == -1, "pooled insert did not return -1 on allocation failure");
		CHECK(rb_size(t) == before && rb_find(t, key) == NULL,
		      "failed pooled insert changed the tree");
		CHECK(rb_validate(t) == 0, "pooled tree invalid after failed insert");
		failures_seen++;

		CHECK(rb_insert(t, key, v) == 0, "retry after failed pooled insert failed");
	}
	CHECK(failures_seen == 300 && rb_size(t) == 300, "pooled fault loop miscounted");
	rb_destroy(t);
}

/* ---------------------------------------------------------------------------
 * A2 M3: iterative rb_destroy / rb_foreach at scale.
 * ------------------------------------------------------------------------- */

struct order_ctx {
	char   prev[16];
	size_t n;
	int    ok;
};

static void order_cb(const char *key, void *value, void *ctx)
{
	(void)value;
	struct order_ctx *c = ctx;
	if (c->n > 0 && strcmp(c->prev, key) >= 0)
		c->ok = 0;
	snprintf(c->prev, sizeof c->prev, "%s", key);
	c->n++;
}

/* RB_BIG_N overrides the node count: `make memcheck` sets it lower because
 * valgrind makes 10^6 nodes impractical. */
static size_t big_n(void)
{
	const char *e = getenv("RB_BIG_N");
	return e != NULL ? (size_t)atol(e) : 1000000;
}

static void test_big_tree(int pooled)
{
	size_t n = big_n();
	printf("test: M3 %zu-node %s tree: foreach order, then destroy\n",
	       n, pooled ? "pooled" : "plain");
	rbtree_t *t = pooled ? rb_create_pooled(NULL) : rb_create(NULL);
	char key[32];
	/* 1000003 is prime, so i * 7919 mod it visits distinct keys in scrambled order. */
	for (size_t i = 0; i < n; i++) {
		snprintf(key, sizeof key, "%07zu", (i * 7919) % 1000003);
		if (rb_insert(t, key, NULL) != 0) {
			CHECK(0, "insert failed while building the big tree");
			break;
		}
	}
	CHECK(rb_size(t) == n, "big tree has the wrong size");

	struct order_ctx c = { .ok = 1 };
	rb_foreach(t, order_cb, &c);
	CHECK(c.n == n, "foreach did not visit every node exactly once");
	CHECK(c.ok, "foreach visited keys out of order");
	rb_destroy(t);
}

/* ---------------------------------------------------------------------------
 * Spec 8.1 fault sweep: a fixed scenario (insert 200 keys, delete 100,
 * overwrite 50) re-run with the n-th allocation failing, n = 1, 2, ... until
 * a run completes without hitting the fault. Same scenario for plain and
 * pooled trees.
 * ------------------------------------------------------------------------- */

#define SW_KEYS 200

static long sw_freed; /* values the tree released through value_free */

static void sw_value_free(void *v)
{
	sw_freed++;
	free(v);
}

struct sweep {
	rbtree_t *t;
	void     *mv[SW_KEYS]; /* reference model: value for each key, NULL = absent */
	size_t    msize;
	long      accepted;    /* values the tree took ownership of */
};

static void sw_key(char *buf, size_t cap, int i)
{
	snprintf(buf, cap, "k%03d", (i * 73) % SW_KEYS); /* 73 is coprime to 200 */
}

/* One insert (new key or overwrite) with every "exactly as it was" check
 * after a failure: size, find, validate, and the caller's value not freed. */
static void sw_insert(struct sweep *w, int i)
{
	char key[32];
	sw_key(key, sizeof key, i);
	int idx = (i * 73) % SW_KEYS;

	int *v = malloc(sizeof *v);
	*v = i;
	long   freed_before = sw_freed;
	size_t size_before  = rb_size(w->t);
	void  *found_before = rb_find(w->t, key);

	int rc = rb_insert(w->t, key, v);
	if (rc == 0) {
		if (w->mv[idx] != NULL)
			CHECK(sw_freed == freed_before + 1, "overwrite did not free exactly the old value");
		else
			w->msize++;
		w->mv[idx] = v;
		w->accepted++;
		return;
	}
	CHECK(rc == -1, "rb_insert returned something other than 0 or -1");
	CHECK(fault_alloc_fired(), "rb_insert failed with no fault injected");
	CHECK(w->mv[idx] == NULL, "an overwrite (which must not allocate) failed");
	CHECK(sw_freed == freed_before, "failed insert consumed the caller's value");
	CHECK(rb_size(w->t) == size_before, "size changed after failed insert");
	CHECK(rb_find(w->t, key) == found_before, "find changed after failed insert");
	CHECK(rb_validate(w->t) == 0, "tree invalid after failed insert");
	free(v); /* still the caller's */
}

static void sw_delete(struct sweep *w, int i)
{
	char key[32];
	sw_key(key, sizeof key, i);
	int idx = (i * 73) % SW_KEYS;

	long freed_before = sw_freed;
	int  rc = rb_delete(w->t, key);
	if (w->mv[idx] != NULL) {
		CHECK(rc == 0, "rb_delete of a present key failed");
		CHECK(sw_freed == freed_before + 1, "rb_delete did not free exactly its value");
		w->mv[idx] = NULL;
		w->msize--;
	} else {
		CHECK(rc == -1, "rb_delete of an absent key did not return -1");
	}
}

/* Returns 1 if the armed fault fired somewhere in the run. */
static int sw_run(int pooled, long n)
{
	sw_freed = 0;
	struct sweep w = { 0 };

	fault_alloc_arm(n);
	w.t = pooled ? rb_create_pooled(sw_value_free) : rb_create(sw_value_free);
	if (w.t == NULL) {
		int fired = fault_alloc_fired();
		fault_alloc_disarm();
		CHECK(fired, "create returned NULL with no fault injected");
		return 1;
	}

	for (int i = 0; i < SW_KEYS; i++)
		sw_insert(&w, i);
	for (int i = 0; i < 100; i++)
		sw_delete(&w, i);
	for (int i = 100; i < 150; i++)
		sw_insert(&w, i); /* overwrite: these keys are present unless their first insert failed */
	int fired = fault_alloc_fired();
	fault_alloc_disarm();

	CHECK(rb_validate(w.t) == 0, "tree invalid at end of scenario");
	CHECK(rb_size(w.t) == w.msize, "size differs from the reference model");
	for (int i = 0; i < SW_KEYS; i++) {
		char key[32];
		snprintf(key, sizeof key, "k%03d", i);
		CHECK(rb_find(w.t, key) == w.mv[i], "model key missing or wrong value");
	}
	rb_destroy(w.t);
	CHECK(sw_freed == w.accepted,
	      "destroy did not release every value the tree owned");
	return fired;
}

static void test_fault_sweep_scenario(int pooled)
{
	printf("test: spec 8.1 full-scenario fault sweep (%s)\n", pooled ? "pooled" : "plain");
	long n = 1;
	/* Invariant: runs 1..n-1 each hit their fault; stops at the first n that never fires. */
	while (sw_run(pooled, n))
		n++;
	printf("  sweep %s: %ld failing runs, completed clean at n=%ld\n",
	       pooled ? "pooled" : "plain", n - 1, n);
	CHECK(n > 10, "sweep covered suspiciously few allocations");
}

/* ---------------------------------------------------------------------------
 * Spec 8.3: 10^6 nodes torn down (and walked) on a 64 KB pthread stack.
 * ------------------------------------------------------------------------- */

struct stack_job {
	rbtree_t        *t;
	struct order_ctx c;
};

static void *small_stack_worker(void *arg)
{
	struct stack_job *j = arg;
	rb_foreach(j->t, order_cb, &j->c);
	rb_destroy(j->t);
	return NULL;
}

static void test_small_stack(int pooled)
{
	size_t n = big_n();
	printf("test: spec 8.3 %zu-node %s tree, foreach + destroy on a 64 KB stack\n",
	       n, pooled ? "pooled" : "plain");
	struct stack_job j = { .t = pooled ? rb_create_pooled(NULL) : rb_create(NULL),
	                       .c = { .ok = 1 } };
	char key[32];
	for (size_t i = 0; i < n; i++) {
		snprintf(key, sizeof key, "%07zu", (i * 7919) % 1000003);
		if (rb_insert(j.t, key, NULL) != 0) {
			CHECK(0, "insert failed while building the small-stack tree");
			break;
		}
	}

	pthread_attr_t attr;
	pthread_t      th;
	pthread_attr_init(&attr);
	CHECK(pthread_attr_setstacksize(&attr, 64 * 1024) == 0, "could not set a 64 KB stack");
	CHECK(pthread_create(&th, &attr, small_stack_worker, &j) == 0, "pthread_create failed");
	pthread_join(th, NULL);
	pthread_attr_destroy(&attr);

	CHECK(j.c.n == n, "foreach on the small stack missed nodes");
	CHECK(j.c.ok, "foreach on the small stack visited keys out of order");
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
	test_fault_create();
	test_fault_insert_sweep(0);
	test_fault_no_alloc_paths(0);
	test_pool_basic();
	test_pool_poison();
	test_pool_growth_fault();
	test_pooled_tree();
	test_pooled_create_fault();
	test_fault_insert_sweep(1);
	test_fault_no_alloc_paths(1);
	test_pooled_insert_faults();
	test_big_tree(0);
	test_big_tree(1);
	test_fault_sweep_scenario(0);
	test_fault_sweep_scenario(1);
	test_small_stack(0);
	test_small_stack(1);

	if (failures == 0) {
		printf("test_rbtree: all cases passed\n");
		return 0;
	}
	printf("test_rbtree: %d failure(s)\n", failures);
	return 1;
}
