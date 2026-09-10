# PROMPTLOG

Annotated AI-interaction log. 4-6 episodes at submission. Each episode:
the prompt, what came back, and my judgment (accepted / rejected / pushed
back on, and why). Raw transcript dumps score nothing -- annotation is the
deliverable.

Must include, before submission:
- [ ] one plan I revised (workflow Step 3)
- [ ] one rejected / oversized diff (Step 4)
- [ ] one tool-output debugging loop (Step 5)
- [ ] one review finding I triaged, plus one false positive (Step 7)

---

## Episode 1 -- M0 setup
Prompt: (summary) asked Claude Code to scaffold the repo skeleton, frozen
header from Appendix A, Makefile from Appendix B, CLAUDE.md from Appendix C.
Result: created include/rbtree.h, Makefile, CLAUDE.md, stub src/rbtree.c,
stub tests. Build verified green.
Judgment: accepted. Setup only, no algorithm. Verified `make test` passes
against stubs before committing.
