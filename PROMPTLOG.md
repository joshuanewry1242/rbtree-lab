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
Judgment: The set up is good i just hope when it time to submit the github is found .




## Episode 2 --  M1 setup
:Prompt: I asked claude to make sure i was on the right path and that my formatting and what im doing is on track.
Result: Claude said im on the right track so far but my reflection is 75% blank, and that my episode one is good. Claude also told me i dont have alot of code write and that i need to edit rb_insert and that the only one that fail due to inserted "dup" "first" then "dup" "second". Second insert should return 0 instead of -1 and rb find returns first should be second.
Judgement: Ill fix these right now and also let them run test again.

## Episode 3
Prompt: Asked claude to help me in fixing rb_insert and rb_find. As there were the only test fails.
Result: Claude fix it and  I ran test again to make sure all cases pass and they pass.
Judgement : I am about to soon finish but claude told me that the only gap left is "valgrind" so im trying to figure that and the github history at the moment.

## Episode 4

