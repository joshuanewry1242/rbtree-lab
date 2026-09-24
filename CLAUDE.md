# rbtree-lab: project rules

## Commands
- Build & unit tests: `make test`
- Sanitizers: `make asan`   Valgrind: `make memcheck`
- A change is DONE only when all three pass. Always run them; show output.
- valgrind does not run on this Mac (Apple Silicon): `make memcheck` runs in
  CI (.github/workflows/battery.yml). Locally, use `make leaks` for leak checks
  (Apple ASan has no LeakSanitizer). Filter output: paste failing lines, not PASS spam.

## Hard constraints
- NEVER modify include/rbtree.h. It is the graded contract.
- All heap allocation in src/ goes through rb_malloc/rb_free
  (tests/fault_alloc.h). Direct malloc/free in src/ is a defect.
- Any allocation may fail. Every failure path must unwind completely:
  no leaks, tree left exactly as before the call, documented error code,
  caller's value NOT consumed. Allocate everything before the first tree
  pointer is modified (commit point). rb_delete and the overwrite path
  must not allocate at all.
- rb_destroy and rb_foreach: no recursion, no heap stack/queue, O(1) space.
- NEVER weaken, skip, or delete a test to make the suite pass. If a test
  looks wrong, stop and explain why instead.

## Style
- C23. -Wall -Wextra -Werror must stay clean. No VLAs.
- Error handling: goto-cleanup pattern for multi-allocation functions.
- Prefer the smallest diff that passes. Do not refactor unrelated code.
- Every non-obvious loop gets a one-line invariant comment.

## Code map
- include/rbtree.h: frozen public API. src/rbtree.c: tree logic, node_alloc/
  node_release, rotations, fixups, validate, teardown.
- src/pool.c + src/pool.h: slab pool (M2). tests/fault_alloc.[ch]: the
  allocation seam + fault injector (M1).
- tests/test_rbtree.c: A1 unit tests (regression suite) + A2 tests.
  tests/fuzz.c: randomized driver vs. a reference model.
- Read only the file/function named in the prompt; don't spelunk.

## Workflow
- For any multi-file or algorithmic change: propose a plan and wait for
  approval before editing.
- Commit only from a green state; message format "M<n>: <what>".
- Do not write or edit PROMPTLOG.md, REFLECTION.md, or DEVLOG.md unless
  asked: those are the student's own annotations.

## Ownership model (the whole assignment)
- The tree owns: every node allocation, and every key copy.
- Ownership of `value` follows the public API in include/rbtree.h:
  - rb_insert success -> tree owns value (frees via value_free).
  - rb_insert failure -> caller still owns value; tree unchanged.
  - rb_insert overwrite -> tree frees the OLD value, then installs the new.
  - rb_delete / rb_destroy -> tree frees the value (if value_free given).
- left/right/parent are links, not ownership. Rotations change topology,
  never who frees what.
