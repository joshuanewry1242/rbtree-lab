# PROMPTLOG: Assignment 2

6–10 annotated episodes. Each one: the prompt, what came back, and my judgment
(accepted / rejected / pushed back, and why), tied to a commit, test, or diff.

Required episodes (check off as they happen):
- [x] A plan I revised (Step 3)
- [x] A rejected / oversized diff (Step 4)
- [ ] A tool-output debugging loop (Step 5)
- [ ] A review finding I triaged: one real, one false positive (Step 7)

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
