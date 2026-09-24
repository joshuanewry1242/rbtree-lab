# REFLECTION: Assignment 2

## Slab pool vs. the kernel's kmem_cache
<!-- What does the kernel's version add (per-CPU caches, ...), and WHY at kernel scale? -->

## pool_stats definitions
<!-- slabs, live, free_objs; the live + free_objs == slabs * objs_per_slab invariant -->

## Slab-header placement and alignment decisions
<!-- in-band vs out-of-band and why; alignment guaranteed and how the stride enforces it -->

## Poisoning rationale
<!-- 0xDD on free: what bug class it catches that ASan no longer can; poison vs free-list link ordering -->

## Mutation 3 notes
<!-- teardown technique and its termination measure; rb_foreach technique and callback contract;
     state honestly if the 10^6-node case runs smaller anywhere (e.g., under valgrind) -->

## Where the agent was most and least reliable

## One bug the agent introduced that I caught, and how

## (Optional) COW snapshots vs. fork(): what plays the page fault? the struct page refcount?
