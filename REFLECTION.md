# REFLECTION: Assignment 2

## Slab pool vs. the kernel's kmem_cache
Mine is one pool, one lock-free (single-threaded) free list, 4096-byte slabs, no
reclaim. kmem_cache adds: per-CPU free lists/magazines, so allocating cores don't
contend on one list or bounce its cache line (at kernel scale many cores allocate
the same object type constantly, so a shared list becomes the bottleneck);
per-NUMA-node partial lists, so memory stays near the CPU using it; slab states
(full/partial/empty) and shrinking, so empty slabs are returned to the page
allocator under memory pressure instead of being held forever like mine until
pool_destroy; constructors and cache coloring, so reused objects are pre-initialized
and slots spread across cache lines; and hardening/debug options (red zones,
poisoning, freelist randomization) that are switchable at runtime. Each exists
because the workload (many CPUs, memory pressure, security exposure) changed, not
for its own sake.

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
ASan tracks the 4096-byte slab as one live allocation; it doesn't know where my
slot boundaries are, so a stale pointer to a freed node still points at "valid"
memory and a use-after-free of a pooled node goes unreported. In debug builds
(no NDEBUG) pool_free and slab carving memset the whole slot to 0xDD and then
write the free-list link over the front of it. So a stale reader of the link bytes
sees a pointer that belongs to the pool, and a stale reader of any other field
sees 0xDD garbage (a key pointer of 0xDDDDDDDDDDDDDDDD faults immediately).
test_pool_poison checks the bytes after the link are 0xDD. Poisoning catches
stale reads, not stale writes into a dead slot that happen before it is
re-allocated.

## Mutation 3 notes
**Teardown:** rb_destroy calls destroy_spine, which rotates each left child up
onto a right spine and frees nodes as it goes: O(1) space, no recursion, no heap
stack. Termination measure: every loop step either frees one node or does one
right rotation that removes one left link from the remaining tree, so (nodes
remaining, left links remaining) strictly decreases lexicographically. For a
pooled tree each node is still returned with pool_free, then pool_destroy frees
the slabs.
**rb_foreach:** in-order walk, read-only (no temporary re-linking as in Morris
traversal). M3 used the parent pointers; M4a removed them (shared nodes can't
have one parent), so each step now finds the successor by a fresh search from
the root: O(1) space, O(n log n) time. Callback contract: fn must not
insert into or delete from the tree during the walk.
**Scale:** the 10^6-node build/foreach/destroy tests run at full size under
`make test`, `make asan` and `make leaks`. Under valgrind in CI (`make memcheck`)
the Makefile sets RB_BIG_N=20000 for the unit-test binary, so the 10^6 case does
not run there. The scale is the Makefile variable `BIG_N` (default 20000;
`make memcheck BIG_N=100000` to tune). validate_rec (rb_validate) is still recursive; it is outside this
mutation's scope and its depth is O(log n).

## Where the agent was most and least reliable
Most reliable: mechanical, well-specified changes with an oracle to check them,
e.g. removing parent pointers (M4a) and rewriting the fixups on a path array passed
the unit tests and both fuzz modes on the first build. Least reliable: scope and
process. It worked from the header and CLAUDE.md, not the spec PDF, so it silently
left out three graded items (debug poisoning, the 64 KB-stack 10^6-node test, and
the 200/100/50 fault-sweep scenario) until the spec was supplied; it also committed
M3 immediately after "approve" without leaving room to push back. (Edit this to
match your own experience.)

## One bug the agent introduced that I caught, and how
The documented one: the agent's shared insert-sweep asserted two failure points
(node and key) for pooled trees too, but a pooled tree with free slots only
allocates the key. The test output flagged it; the fix was a per-mode threshold
plus a separate slab-boundary test, not a loosened assertion (PROMPTLOG episode 4).
That was found by the test run, not by my reading a diff. (If you caught a different
agent bug yourself, replace this.)

## (Optional) COW snapshots vs. fork(): what plays the page fault? the struct page refcount?
