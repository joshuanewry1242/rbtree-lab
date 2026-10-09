# REFLECTION: Assignment 2

## Slab pool vs. the kernel's kmem_cache
<!-- What does the kernel's version add (per-CPU caches, ...), and WHY at kernel scale? -->

## pool_stats definitions
`slabs` is the number of 4096-byte slabs the pool owns. `live` is the number of
slots handed out by pool_alloc and not yet returned by pool_free. `free_objs` is
every unused slot across all slabs (all of them sit on the free list, because a
new slab's slots are threaded onto it immediately). So
`live + free_objs == slabs * objs_per_slab` always; test_pool_basic checks this
after spilling into three slabs and after freeing everything. Slabs are never
released before pool_destroy, so freeing every object leaves `free_objs` at
`slabs * objs_per_slab`, not zero.

## Slab-header placement and alignment decisions
In-band: the header (one `next` link) sits at the start of the slab's 4096-byte
rb_malloc block. That makes each slab a single allocation, so growth has one
failure point and a failed pool_alloc has nothing to unwind. The header is padded
to `alignof(max_align_t)`, and the slot stride is `max(obj_size, sizeof(void*))`
rounded up to the same alignment, so every slot starts max_align_t-aligned as
long as the block from rb_malloc is (malloc guarantees that). Free-list links are
stored with memcpy to avoid aliasing a typed pointer onto arbitrary slot memory.
The cost of in-band placement is that the header takes a few bytes off each slab.

## Poisoning rationale
<!-- 0xDD on free: what bug class it catches that ASan no longer can; poison vs free-list link ordering -->

## Mutation 3 notes
<!-- teardown technique and its termination measure; rb_foreach technique and callback contract;
     state honestly if the 10^6-node case runs smaller anywhere (e.g., under valgrind) -->

## Where the agent was most and least reliable

## One bug the agent introduced that I caught, and how

## (Optional) COW snapshots vs. fork(): what plays the page fault? the struct page refcount?
