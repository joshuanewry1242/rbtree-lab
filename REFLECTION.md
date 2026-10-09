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
**Teardown:** rb_destroy calls destroy_spine, which rotates each left child up
onto a right spine and frees nodes as it goes: O(1) space, no recursion, no heap
stack. Termination measure: every loop step either frees one node or does one
right rotation that removes one left link from the remaining tree, so (nodes
remaining, left links remaining) strictly decreases lexicographically. For a
pooled tree each node is still returned with pool_free, then pool_destroy frees
the slabs.
**rb_foreach:** in-order walk using the parent pointers. Go to the leftmost node;
after visiting n, descend to the leftmost node of n's right subtree if there is
one, otherwise climb until arriving from a left child. O(1) space, read-only (no
temporary re-linking as in Morris traversal). Callback contract: fn must not
insert into or delete from the tree during the walk.
**Scale:** the 10^6-node build/foreach/destroy tests run at full size under
`make test`, `make asan` and `make leaks`. Under valgrind in CI (`make memcheck`)
the Makefile sets RB_BIG_N=20000 for the unit-test binary, so the 10^6 case does
not run there. validate_rec (rb_validate) is still recursive; it is outside this
mutation's scope and its depth is O(log n).

## Where the agent was most and least reliable

## One bug the agent introduced that I caught, and how

## (Optional) COW snapshots vs. fork(): what plays the page fault? the struct page refcount?
