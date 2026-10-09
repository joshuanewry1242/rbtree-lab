#include "pool.h"

#include "fault_alloc.h"

#include <stdalign.h>
#include <stddef.h>
#include <string.h>

/* Mutation 2: slab pool. See DEVLOG.md (2026-10-08) for the design record.
 * A slab is one SLAB_SIZE block from rb_malloc: a header (just the next-slab
 * link, padded to max_align_t) followed by equal-size slots. Free slots hold
 * the free-list link in their first bytes. */

#define SLAB_SIZE 4096u

struct slab_hdr {
	struct slab_hdr *next;
};

struct rb_pool {
	size_t           stride;    /* bytes per slot */
	size_t           per_slab;  /* slots per slab */
	size_t           slabs;
	size_t           live;
	size_t           free_objs; /* unused slots across all slabs */
	struct slab_hdr *slab_list;
	void            *free_list;
};

static size_t round_up(size_t n, size_t a)
{
	return (n + a - 1) / a * a;
}

static size_t slab_hdr_size(void)
{
	return round_up(sizeof(struct slab_hdr), alignof(max_align_t));
}

/* Free-list links live inside slots of arbitrary type, so go through memcpy
 * rather than a typed pointer. */
static void *slot_next(const void *slot)
{
	void *next;
	memcpy(&next, slot, sizeof next);
	return next;
}

static void slot_set_next(void *slot, void *next)
{
	memcpy(slot, &next, sizeof next);
}

rb_pool_t *pool_create(size_t obj_size)
{
	size_t stride = round_up(obj_size > sizeof(void *) ? obj_size : sizeof(void *),
	                         alignof(max_align_t));
	if (stride > SLAB_SIZE - slab_hdr_size())
		return NULL; /* not even one slot fits */

	rb_pool_t *p = rb_malloc(sizeof *p);
	if (p == NULL)
		return NULL;

	p->stride    = stride;
	p->per_slab  = (SLAB_SIZE - slab_hdr_size()) / stride;
	p->slabs     = 0;
	p->live      = 0;
	p->free_objs = 0;
	p->slab_list = NULL;
	p->free_list = NULL;
	return p;
}

/* Adds one slab and threads all its slots onto the free list. All state
 * changes come after the only allocation, so failure leaves p untouched. */
static int pool_grow(rb_pool_t *p)
{
	struct slab_hdr *s = rb_malloc(SLAB_SIZE);
	if (s == NULL)
		return -1;

	s->next      = p->slab_list;
	p->slab_list = s;

	char *base = (char *)s + slab_hdr_size();
	/* Invariant: slots [0, i) of this slab are on the free list. */
	for (size_t i = 0; i < p->per_slab; i++) {
		void *slot = base + i * p->stride;
		slot_set_next(slot, p->free_list);
		p->free_list = slot;
	}
	p->slabs++;
	p->free_objs += p->per_slab;
	return 0;
}

void *pool_alloc(rb_pool_t *p)
{
	if (p->free_list == NULL && pool_grow(p) != 0)
		return NULL;

	void *slot   = p->free_list;
	p->free_list = slot_next(slot);
	p->live++;
	p->free_objs--;
	return slot;
}

void pool_free(rb_pool_t *p, void *obj)
{
	slot_set_next(obj, p->free_list);
	p->free_list = obj;
	p->live--;
	p->free_objs++;
}

void pool_stats(const rb_pool_t *p,
                size_t *slabs, size_t *live, size_t *free_objs)
{
	if (slabs != NULL)
		*slabs = p->slabs;
	if (live != NULL)
		*live = p->live;
	if (free_objs != NULL)
		*free_objs = p->free_objs;
}

void pool_destroy(rb_pool_t *p)
{
	if (p == NULL)
		return;

	/* Invariant: slabs before s are already freed; s is still valid. */
	struct slab_hdr *s = p->slab_list;
	while (s != NULL) {
		struct slab_hdr *next = s->next;
		rb_free(s);
		s = next;
	}
	rb_free(p);
}
