# Agent instructions for srd

srd is a small C++ command-line tool that keeps encrypted text
records.  README.md describes its use, and README.dev.md its design,
file formats and source layout.  Read README.dev.md before changing
anything in `src/`, and keep its design criteria and security claims
true:  if a change alters what srd protects or reveals, update
README.dev.md (and README.md's known issues) in the same change.

**Before any git or gh command** (status, diff, add, commit, branch,
merge, push, or anything touching GitHub), read GIT.md.  It holds the
branch, commit and push rules.

## Environment

There is no container:  srd builds and tests on the host.  Build
dependencies are listed in README.packages, which CI
(`.github/workflows/test.yml`) mirrors; keep the two in step.

## Building and testing

Work in `src/`:

    make                  # build srd (-O2; make OPTIMIZE=-O0 to debug)
    make test             # unit tests, then end-to-end tests
    make test-sanitize    # the same under ASan and UBSan

- Run tests whenever useful; no need to ask first.
- Before handing off or committing, run `make test` and
  `make test-sanitize`, and say whether they passed.  Never commit
  with failing tests.
- The build must stay warning-free under `-Wall -Wextra`.  Treat a new
  warning as a failure, even in code you didn't touch.
- If you can't run the tests (missing packages, no terminal for
  `test-tty.py`, ...), say so plainly, say why, and ask the human to
  run them.  Never claim success you haven't seen.
- A bug fix comes with a test that fails before the fix and passes
  after, where that's practical.  End-to-end tests are `test-*.sh`
  scripts run by `make test` in test mode (`-T`):  add new ones to
  `END_TO_END_TESTS` in the Makefile.

## Secrets

srd exists to guard secrets.  Never put a real password, database,
decrypted record, terminal transcript or log that might hold one into
a file, a commit, or a prompt (including a subagent's).  Use test mode
and the test databases in `src/test.d/`.

## Code style

- C++23, built with g++ (14 or later).  Follow the style of the
  surrounding code; don't reformat or re-indent code you aren't
  otherwise changing.
- Every `.cc` and `.h` file starts with the GPL copyright header that
  the others carry, with the current year for a new file.
- Comment why, not what.  Comments, docs and commit messages use
  British spelling, except for names fixed by an API
  (`SerializeToString`, `sodium_init`).
- Each `foo.cc` has a `foo_test.cc` unit test where practical;
  declarations go in `srd.h`.

## Subagents

Delegate to a subagent when a task would pull a lot of throwaway
output into the main context (broad searches, long logs, exploratory
reading), or when a narrow, well-specified job suits a cheaper model.
Do small, focused work inline:  a subagent starts without your
context, and briefing it costs more than a quick look yourself.
Reviews (below) always go to a subagent, so that they're independent.

## Review before commit or hand-off

Every change, including documentation, tests, build files and CI,
gets reviewed by a subagent before it is committed or handed off.
srd guards secrets, and a docs change can still misstate a security
property.  The procedure is the same whoever wrote the change:  you,
another session, or a human.

- **While implementing:**  for a non-trivial change, a quick
  correctness-and-security pass by one subagent once the first
  working version exists, to catch mistakes before they compound.
- **When believed complete:**  a **security review** of the change
  (the diff, plus what's needed to judge it), always.  For a
  non-trivial change, also a correctness review and an
  instruction-compliance review (this file and GIT.md), run in
  parallel.  Treat anything ambiguous as non-trivial.  Only typo
  fixes, comment tweaks and one-line non-logic edits are trivial, and
  even these get the security review.
- **Briefing the reviewer:**  give it the diff and point it at
  README.dev.md's file-format and design-criteria sections.  Tell it:
  - Report only problems the change introduces or exposes, on the
    lines it touches or that those lines rely on.  Skip pre-existing
    issues, nitpicks, what the compiler or CI would catch, and style
    preferences.
  - For each finding, give the file and line, the condition that
    triggers it, and the impact ("leaks the key when X, because Y"),
    ranked by severity.  If there is nothing, say so plainly.
  - Read only; don't edit, fix or commit.
- **Transparency:**  say when a review starts and that you're waiting
  on it, and count each launch aloud ("launching the second security
  review").  Report every finding to the human, and say how each was
  addressed, or why not.
- **Judgement:**  a reviewer, human or subagent, can be wrong.  If you
  disagree with a finding, say so and why; don't silently comply or
  silently ignore it.
- **Re-review:**  after fixing substantive findings, have a fresh
  subagent review again.  A review passes when it comes back clean,
  with only trivial points, or with only points deliberately left,
  each explained.  After about three rounds, suspect a loop or a
  deeper problem; stop and ask the human.

When you have read this file, say so.
