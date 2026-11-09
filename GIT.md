# Git and GitHub rules for srd

Read this before any git or gh command.

## Branches and worktrees

- **Work on a branch, never on `main`.**  It needn't be a new branch:
  if the current branch isn't `main`, use it.  If it is `main`, make a
  branch before the first commit.  `main` only moves through a merge
  after CI (`.github/workflows/test.yml`) has passed, and the human
  decides when to merge.  Never commit to `main` directly.
- Run git in the directory you mean, with `git -C <dir>` if it's
  elsewhere.  Never `cd <dir> && git ...`:  with more than one
  worktree or clone, that silently acts on the wrong branch.  This
  holds even when only one worktree exists today.

## Committing

- Ask before committing, unless the human asked for a sequence of
  steps that includes committing.  Your commits go through the same
  review as anyone's (see AGENTS.md).
- Commit only after the reviews and tests that AGENTS.md requires
  have passed.
- Stage files by name, never with `git add -A` or `git add .`, and
  check `git status` first.  Untracked transcripts or logs may hold
  secrets.
- When you create a file, run `git add --intent-to-add` on it straight
  away, so that `git diff` shows it to you and to reviewers.
- One logical change per commit.  Don't bundle refactoring,
  reformatting and behaviour changes.

## Commit messages

Match the existing history (`git log`):

- **Subject:**  imperative mood, no trailing period, about 50
  characters at most ("Size the IV copy by the cipher's block size").
- **Body:**  a blank line after the subject, then text wrapped at 72
  columns, except tables, code, URLs and error output.  Say why:  the
  problem, the approach, alternatives rejected, trade-offs, and any
  effect on existing databases or users.  Don't restate the diff.
- **Bullets:**  `*` indented two spaces, with a hanging indent for
  wrapped lines.  If every bullet in a list is one line, leave no
  blank lines between them; if any wraps, put a blank line between
  every bullet in that list.
- **Spelling:**  British.
- **Issues:**  reference them at the end as `Part of #N` or
  `Closes #N`.  A branch name with a number (`jeff-23`) is not
  reliably an issue number here:  issue #23 is old and unrelated.  So
  ask before citing an issue taken from a branch name.
- **No emojis.**  AI co-authorship gets the one plain
  `Co-Authored-By:` line.
- Pass multi-line messages with a heredoc
  (`git commit -F - <<'EOF'`), never `\n` inside `-m`.
- Check the subject length and body wrap in the draft *before*
  committing.  Don't commit first and fix it with `--amend`.

## Pushing, rewriting history, GitHub

- Never `git push`, in any form and to any branch, unless the human
  asked for it in this request.
- Force-pushing, `reset --hard`, rebasing published commits and
  amending pushed commits all need the human's explicit approval each
  time; approval for one push doesn't carry over to the next.  An
  approved force-push is always `git push --force-with-lease`, never
  `--force` or `-f`.
- git is local; GitHub is a separate, shared service.  Do anything on
  GitHub (`gh`, opening or merging PRs, commenting on issues) only
  when the human asked for it or agreed when you asked.  Prefer the
  local way:  to merge, `git merge --ff-only`, not `gh pr merge`.

When you have read this file, say so.
