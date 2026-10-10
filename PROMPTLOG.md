# PROMPTLOG: Assignment 2

6–10 annotated episodes. Each one: the prompt, what came back, and my judgment
(accepted / rejected / pushed back, and why), tied to a commit, test, or diff.

Required episodes (check off as they happen):
- [x] A plan I revised (Step 3)
- [x] A rejected / oversized diff (Step 4)
- [x] A tool-output debugging loop (Step 5)
- [x] A review finding I triaged: one real, one false positive (Step 7)

---

## Episode 1: "give me mutation 1" came back as a finished, committed diff (M1, step 4)
**Prompt:** "give me mutation 1"
**What came back:** A plan that audited rb_insert and listed its failure points
and commit point, then 127 new lines of fault-sweep tests in
tests/test_rbtree.c (create failure, insert sweep, no-alloc check for overwrite
and delete). The battery passed, and a planted leak in node_alloc was caught by
`make leaks` (1 leak, 64 bytes). It was committed as 6bb156f. I approved the plan,
but the diff was committed before I had read it.
**My judgment:** Pushed back. The diff was too big for the point I was at: M1
should have started with my paper pass and the Step 2 read-only walkthrough of
rb_insert. Going straight to tests skipped both, and the plan gave away the
answers the paper pass was supposed to find. I also approved that plan without
noticing it skipped those steps. I kept the commit rather than resetting it,
since the tests are sound and the battery is green. I then did the paper pass
and walkthrough properly. I got both failure points right (node_alloc L39 and
L44), but I marked only L227 as the first tree write, when it's
L225/L227/L229 depending on the branch.
**Evidence:** commit 6bb156f / tests test_fault_insert_sweep,
test_fault_no_alloc_paths / src/rbtree.c:219-229


## Episode 2: plan-mode M2 (slab pool) approved, then implemented in one pass (M2)
**Prompt:** "what the next step and how do we go about it", then "yes enter into
plan mode and record what you need to record in devlog.md".
**What came back:** A plan with six pool decisions (header inside the slab, stride
rounded to max_align_t, lazy first slab, free_objs = every unused slot, no slab
shrinking so pool_free/rb_delete can never allocate, keys stay on rb_malloc). I
approved it, and the agent then wrote src/pool.c, the rbtree.c wiring
(node_struct_alloc/node_struct_free, rb_create_pooled, rb_delete's successor
free now going through the pool) and pool/pooled-tree tests. The spec's section
8.2 text isn't in the repo, so the decisions came from src/pool.h and are flagged
in DEVLOG as revisitable.
**My judgment:** Accepted the plan. One mistake in the agent's new test: the
shared insert sweep asserted two failure points, but a pooled tree with free
slots only allocates the key, so it failed. The agent set the threshold per mode
(plain >= 2, pooled >= 1) and covered slab-growth failure in a separate test
rather than loosening the shared assertion. I also declined one command prompt
(asan + leaks) because I thought I was being asked to do work myself; the agent
explained they were required checks and I approved them. All green: make test,
make asan, make leaks (0 leaks).
**Evidence:** tests/test_rbtree.c test_pool_*, test_pooled_*; tests/fuzz.c run()
plain + pooled; src/pool.c; src/rbtree.c node_struct_alloc

## Episode 3: M3 plan revised after the work was already committed (M3)
**Prompt:** "i approve" on the M3 plan, then "lets make a plan on m3 and give me
an option so i can push back".
**What came back:** The agent implemented and committed M3 (96558d2) straight
after "i approve", leaving no point to push back. When asked, it re-entered plan
mode, listed the M3 decisions (parent-pointer vs Morris foreach, keeping
rb_destroy_reach, valgrind scale-down, 10^6 tests in default make test), and
offered accept / revise / redo.
**My judgment:** Chose "revise with follow-ups". Pushed back on the hard-coded
valgrind scale and asked for a separate knob; kept the parent-pointer foreach
over Morris (a Morris callback would see a rewired tree); kept the other
decisions. The agent found that RB_BIG_N was already separate for the big tests,
so the real change was making the value a Makefile variable (BIG_N).
**Evidence:** Makefile BIG_N; commit "M3: BIG_N knob for the valgrind scale";
96558d2 for the original M3 diff

## Episode 4: debugging the pooled insert sweep from test output (M2, step 5)
**Prompt:** (no new prompt; the agent ran `make test` after wiring in the pool.)
**What came back:** One failure line from the shared insert sweep: "sweep
covered fewer than 2 failure points (node + key)". The agent read it, reasoned
that a pooled tree with free slots only allocates the key, so there is one
failure point there, not two, and the slab-growth failure is a different case.
It fixed the assertion per mode (plain >= 2, pooled >= 1) and added a separate
test that fails allocation #1 of every insert across slab boundaries, then
re-ran test/asan/leaks green.
**My judgment:** This is a short loop (run, read output, diagnose, fix, re-run),
not a long chase, so I am claiming it only as a small example. The check was
changed because its expectation was wrong for pooled trees, not to make it pass;
the slab-growth case got its own stronger test.
**Evidence:** tests/test_rbtree.c test_fault_insert_sweep(pooled),
test_pooled_insert_faults; commit 9570872

## Episode 5: /code-review on M3, one real finding and one false positive (step 7)
**Prompt:** `/code-review`, then "fix the issues we found".
**What came back:** Two findings. (1) rb_snapshot is declared in
include/rbtree.h but defined nowhere. (2) src/ calls rb_malloc/rb_free, which
exist only in tests/fault_alloc.c, so src/ will not link without the test seam.
**My judgment:** Finding 1 is real but expected: rb_snapshot is Mutation 4 and
had not been started, so the fix is M4, not a patch to M3 (a NULL stub would
hide the gap). Finding 2 is a false positive: fault_alloc.c is the M0 allocation
seam, forwards to malloc/free when disarmed, and is linked into every Makefile
target and CI; adding a second definition in src/ would cause duplicate symbols.
The agent verified both against the code before touching anything, and refused
to "fix" finding 2. Finding 1 led to the M4 plan.
**Evidence:** include/rbtree.h:45; tests/fault_alloc.c:5-22; M4 plan

## Episode 6: auditing the repo against the spec PDF (M2/M3 gaps)
**Prompt:** I attached the assignment spec and companion and said "do 1-5" (the
list of remaining items the agent had given me).
**What came back:** The agent had worked from the header and CLAUDE.md only, so
reading the spec turned up gaps in already-"done" milestones: (1) no debug
poisoning in pool_free (Mutation 2 / grading table), (2) no 10^6-node
foreach + destroy test on a 64 KB pthread stack (Mutation 3), (3) the fault sweep
was one insert into a 10-key tree, not the spec's 200 inserts / 100 deletes / 50
overwrites with the unchanged-checks and a counting destructor. It added poisoning
plus an in-pool live + free_objs invariant assert, test_small_stack (plain and
pooled), and a full-scenario sweep (plain: 401 failing runs, pooled: 204), all green
under make test / asan / leaks.
**My judgment:** The earlier "M2 done" and "M3 done" claims were premature; I
should have had the agent read the spec before planning. Mutation 4 is ungraded, so
the agent's M4a refactor was optional work done on main.
**Evidence:** src/pool.c POISON/asserts; tests/test_rbtree.c test_pool_poison,
test_fault_sweep_scenario, test_small_stack
