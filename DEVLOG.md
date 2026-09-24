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
