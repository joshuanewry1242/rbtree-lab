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
Prompt: Asked Claude Code to make sure the repo had real git history and
that my Claude Code session transcripts would be captured for grading.
Result: Claude set up git identity, made two initial commits (repo
skeleton + logs/), added a Stop hook + logs/sync.sh so transcripts
auto-sync every turn, and then implemented rb_create (malloc the struct,
init root=NULL/size=0, store value_free, NULL-check the allocation).
Judgment: Accepted, but I didn't actually understand what had been set up
at first -- I thought "capturing transcripts" meant I'd have to write or
build a transcript file myself to show our conversation. Asked Claude to
clarify, and it turned out git history and the transcript are two
separate requirements, not the same thing: git history is commits I (or
Claude, on my instruction) deliberately make; the transcript is something
Claude Code was already recording automatically the entire time as a
.jsonl file on my machine. The actual work in this episode wasn't
"creating" a transcript, it was making sure the one that already existed
got copied into the repo (logs/) and kept in sync, since otherwise it
would've stayed stranded on my laptop where no grader could see it.




## Episode 2 -- status check before continuing M1
Prompt: Asked Claude to check whether everything built so far was correct
and complete before I kept going.
Result: Claude re-read src/rbtree.c and confirmed insert/find/delete/
validate/destroy/foreach all still matched earlier verification with no
regressions -- but flagged one real bug: rb_insert's overwrite path
(cmp == 0) still unconditionally returned -1 instead of updating the value
and returning 0, per Appendix A's contract. Also flagged REFLECTION.md was
still mostly blank.
Judgment: Accepted the finding, but not immediately -- I pushed back and
asked whether the old behavior (rb_insert returning -1, rb_find still
returning "first") wasn't actually correct. Claude pointed me at the
frozen header (include/rbtree.h): -1 is documented as meaning allocation
failure specifically, and the same comment says overwriting an existing
key frees the old value, which only makes sense if overwrite is a success
path, not a -1 path. I checked rb_insert myself and confirmed the cmp==0
branch really did just `return -1` unconditionally, matching neither of
those. That's what convinced me it was a real bug and not intended
behavior. Asked Claude to fix it next (became Episode 3).


## Episode 3 -- fixing the overwrite bug (tool-output debugging loop)
Prompt: Asked Claude to fix rb_insert's overwrite path and confirm rb_find
needed no change, since that was the one real gap found in Episode 2.
Result: Claude fixed rb_insert's cmp==0 branch (free the old value via
value_free if set, install the new value, return 0 -- it previously
returned -1 unconditionally) and re-ran make test / make asan.
test_rbtree: all cases passed on both.
Judgment: Accepted -- re-ran the exact broken scenario myself (insert
"dup"->"first", then "dup"->"second") and confirmed rc2 is now 0 and
rb_find returns "second" instead of "first". I initially pushed back and
asked whether rb_find/rb_insert weren't already good enough before this
fix. Claude walked me through the frozen header contract
(include/rbtree.h says overwriting an existing key must return 0 and free
the old value, not -1) and the concrete rc1/rc2/find before-and-after,
tied to a real committed regression test (test_insert_overwrite in
tests/test_rbtree.c). That evidence is what convinced me the bug was
real, not just Claude's claim -- I checked it before accepting. Then asked
Claude to look into the valgrind gap and git history next.

## Episode 4 -- pushing to GitHub, wiring up CI for valgrind
Prompt: Asked Claude to fix the missing-valgrind gap (no valgrind on this
Mac) and to help me actually push the repo to GitHub.
Result: Claude explained valgrind doesn't support Apple Silicon macOS at
all (not a package Homebrew could install), wrote a GitHub Actions
workflow (test+asan+memcheck on ubuntu-latest) instead, then found the
remote was already configured, split the large uncommitted diff into ~9
separate commits matching the real build order instead of one lump
commit, verified each intermediate state was green, and pushed. Pointed
me at the repo's Actions tab to check the real memcheck result.
Judgment: Accepted, but I initially thought "no valgrind" just meant I
needed to download it, like any other tool. Asked Claude to confirm --
it checked uname -m (confirmed arm64), checked whether Homebrew was even
installed (it wasn't), and checked whether Docker was available as an
alternative (also wasn't), before concluding that valgrind has never
built a working version for Apple Silicon macOS at all, through any
package manager. That's different from "you typed the wrong install
command" -- there's no download that works on this exact machine, which
is why the fix was routing memcheck to GitHub's Linux runners instead
(apt-get install valgrind there is the normal, boring download I was
picturing, just running somewhere valgrind actually exists).

