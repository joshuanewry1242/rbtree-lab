# DEVLOG

Five minutes at the end of every session. Not a deliverable; it's the raw
material for PROMPTLOG.md and REFLECTION.md.

<!-- template
## YYYY-MM-DD (week W, evening E, ~Nh)
STATE:
DID:
DECIDED:
LEARNED:
NEXT FIRST STEP:
OPEN:
-->

## 2026-09-23 (week 1, evening 1: M0 setup)
STATE: M0 done. A1 battery green against the new frozen header. asan + leaks clean.
DID: tagged a1-final; archived A1 PROMPTLOG/REFLECTION/transcript to docs/a1/;
  new frozen header; rb_malloc/rb_free seam (tests/fault_alloc.[ch]) with all
  src/ allocation routed through it; pool.h/pool.c placeholder; Makefile per
  Appendix B + `make leaks`; CLAUDE.md amended; /battery command.
DECIDED: stay in the A1 repo (history continues). memcheck runs in CI because
  valgrind can't run on Apple Silicon; `make leaks` is the local stand-in.
NEXT FIRST STEP: M1 exploration session (Step 2), no edits. Before it, mark by
  hand in rb_insert: every call that can fail + the first line that modifies the tree.
OPEN: rb_destroy is still recursive (destroy_rec); the A1 Reach's destroy_spine
  exists but isn't wired in. That's M3's job; don't touch it yet.

## 2026-10-08 (week 2: M2 plan mode)
STATE: M0 + M1 committed. pool.c still a placeholder; rb_create_pooled not implemented.
DID: plan-mode session for M2; plan approved (no code yet).
DECIDED (pool design; the spec's section 8.2 text isn't in the repo, so these
  come from src/pool.h and may need revisiting if the spec says otherwise):
  1. Slab header lives inside the 4096-byte slab (one rb_malloc per slab = one
     failure point, trivial unwind). Header = `next` link, padded to max_align_t.
  2. Slot stride = max(obj_size, sizeof(void*)) rounded up to alignof(max_align_t);
     free slots hold the free-list link in their first bytes. pool_create fails
     (NULL) if one slot doesn't fit in a slab.
  3. Growth only when the free list is empty; a new slab's slots are all
     threaded onto the free list at once. Failed slab alloc => NULL, pool state
     untouched. pool_create allocates only the pool struct (first slab lazy).
  4. pool_stats: slabs = slabs owned; live = handed out; free_objs = every unused
     slot across all slabs. Invariant: live + free_objs == slabs * slots_per_slab.
  5. Slabs are never shrunk before pool_destroy, so pool_free / rb_delete can't
     allocate or fail.
  6. Only struct rb_node comes from the pool; key copies stay on rb_malloc.
LEARNED: rb_delete's two-child path frees the successor struct directly
  (rb_free(s)); that call must go through the pool for pooled trees.
NEXT FIRST STEP: implement src/pool.c + pool unit tests, `make test`.
OPEN: destroy_rec is still recursive (M3). Check the spec text for 8.2 semantics.

## 2026-10-08 (week 2: M2 implementation)
STATE: M2 done and committed. make test, asan, leaks green (memcheck runs in CI).
DID: src/pool.c; rbtree.c wiring (pool field, node_struct_alloc/free,
  rb_create_pooled, destroy paths take the tree); pool + pooled-tree tests and
  fault sweeps; fuzz.c now runs plain then pooled against the model.
DECIDED: shared insert sweep threshold is per mode (plain >= 2 failure points,
  pooled >= 1); slab-growth failure is covered by test_pooled_insert_faults.
LEARNED: with a pool, the first allocation of an insert is either the slab (full
  pool) or the key, so "fail allocation #1 on every insert" covers both.
NEXT FIRST STEP: M3 plan: iterative rb_destroy (wire in destroy_spine) and
  O(1)-space rb_foreach via parent pointers.
OPEN: REFLECTION poisoning section (0xDD on free) is not implemented; check the
  spec for whether M2 requires it. destroy_rec still recursive until M3.

## 2026-10-08 (week 2: M3)
STATE: M3 done and committed. make test, asan, leaks green; 10^6-node tests run in ~1s locally.
DID: rb_destroy now uses destroy_spine; destroy_rec and foreach_rec removed;
  rb_foreach is an iterative parent-pointer walk; 10^6-node plain + pooled tests
  (order check + destroy); RB_BIG_N override, set to 20000 in make memcheck.
DECIDED: kept rb_destroy_reach (still tested; duplicates rb_destroy now).
OPEN: rb_validate is still recursive (depth O(log n)). rb_destroy_reach could be
  removed. Poisoning (0xDD) in REFLECTION still unimplemented. M3 diff is a good
  candidate for the /code-review episode in PROMPTLOG.
NEXT FIRST STEP: /code-review on the M3 diff, triage findings (PROMPTLOG episode).

## 2026-10-08 (week 2: M3 follow-up)
DID: reviewed M3 decisions with the plan-mode pushback option. Kept parent-pointer
  foreach (Morris would expose a rewired tree to callbacks), kept rb_destroy_reach,
  kept 10^6 tests in make test. Made the valgrind scale a Makefile variable BIG_N.
LEARNED: I committed M3 right after "approve" with no room to push back; ask for
  an explicit go-ahead on the committed-vs-reviewed distinction next time.
NEXT FIRST STEP: /code-review on the M3 diff (PROMPTLOG review-finding episode).

## 2026-10-09 (week 2: M4a)
STATE: M4a done: parent pointers removed. make test, asan, leaks green.
DID: rotations take the node's parent explicitly (replace_child); insert_fixup and
  delete_fixup run on an on-stack path array (RB_MAX_DEPTH 128); transplant reduced
  to replace_child; tree_minimum folded into rb_delete; rb_foreach is now a
  successor search from the root (O(1) space, O(n log n) time).
DECIDED: foreach goes from parent walk to successor search because the approved
  M4 path-copying plan needs nodes with several parents. Insert case 2 / delete
  case 1 swap/shift path entries to keep the path right after a rotation.
NEXT FIRST STEP: M4b plan check-in: refs on nodes, payload split (key + value
  refcounted), refcount-aware destroy_spine, pool refcount. No snapshots yet.
OPEN: PROMPTLOG episodes 4-5 are factual drafts for you to edit.

## 2026-10-09 (week 2: spec audit)
STATE: spec PDF read; three graded gaps closed. make test, asan, leaks green.
DID: debug poisoning (0xDD) + live+free_objs asserts in pool.c; 10^6-node
  foreach+destroy on a 64 KB pthread stack (plain+pooled); full-scenario fault
  sweep (200 ins / 100 del / 50 overwrite) for plain and pooled.
LEARNED: spec says Mutation 4 is ungraded and to do it after M3 on a branch or
  behind a flag; M4a (parent pointers removed) is on main.
NEXT FIRST STEP: push and confirm CI (make memcheck) is green; decide whether M4a
  stays on main or moves to a branch.
OPEN: memcheck never run (CI only); logs/*.jsonl need copying into the submission.

## 2026-10-09 (week 2: M4a moved to a branch)
DID: kept the M4a work (parent pointers removed, successor-search foreach) on
  branch `reach`; main's src/rbtree.c is back to the M3 version (parent-pointer
  foreach). Spec: Mutation 4 is ungraded, do it after M3 on a branch. CLAUDE.md
  amended (poisoning, RB_BIG_N, Reach lives on `reach`).
NEXT FIRST STEP: confirm CI green on main (make memcheck); then optionally resume
  M4b on `reach`.

## 2026-10-09 (week 2: first CI run)
LEARNED: CI (gcc 14) failed `make asan` with -Werror=format-truncation in my new
  sweep test: key[8] vs snprintf("k%03d", int). Apple clang doesn't flag it. Same
  family as the fuzz.c key[16] comment. Widened all test key buffers to 32.
NEXT FIRST STEP: confirm the CI run for this commit gets through asan and memcheck.
