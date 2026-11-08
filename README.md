# This is the release branch of srd.

srd is a secure rolodesk, which is a bit of a retro name for an
encrypted set of notes.  In modern parlance, it might be called a
password vault, but the idea is to be able to write and retrieve
arbitrary bits of text.

I developed srd on an ubuntu linux system.  There's no real reason to
suspect it wouldn't work elsewhere, but that hasn't been tested.

There's a library if you want to use the code in another context with
minimal hacking.

*srd changed file format in November 2026, to stronger encryption
 and password hashing.  It still reads the old format, and says so
 when it does; run `srd --upgrade` to convert.  See below.*

*srd changed file format in May 2015.  See below if this affects you.
 You will know it affects you because you can't read your srd files
 anymore.*

## Want to use it?

The code is stable, and I believe it works.  There are reasonably
extensive regtests that convince me of this.  I use it and trust my
secrets (bank passwords, online account info, etc.) to it.  Send me
mail with feedback.  As always (see src/COPYING), you accept that it
may not work for you.  If it doesn't, I'd appreciate hearing of a
reproducible case of the failure.

## Why did I write this?

I wrote a short python script once upon a time to do something
similar.  I developed a bunch of features I wanted to add.  It became
a rewrite.  As an exercise, I decided to see how much longer it would
take to write it in C++ rather than python.  (Answer:  about five times
longer.)  And it amuses me.  And it pushes me to look at some new
libraries and language features that I don't get to use at work.

The other answer, since there's no shortage of password vaults out
there, is that (1) I wanted something that lives nicely on the
commandline, and (2) I wanted to understand the code so that I'm sure
it's doing what I want.  Those could probably both have been satisfied
more easily without writing code, but would have left me less happy.

# Here's the usage message:
````
srd <options> [key-match-pattern]

  Pattern matching options (-m, -D, -d) may be repeated.
    Multiple key patterns (-m) match as inclusive or.
    Multiple payload patterns (-d) match as and (but cf. -J).
    Multiple key-or-payload patterns (-D) match as inclusive or.
  Key-match-pattern, if present, is the same as specifying -m.
  To create a new record, use -e with no pattern.

Allowed options:

General options:
  -h [ --help ]                   Produce help message
  -R [ --read-only ]              Read-only, do not persist data
  --database-dir arg              Name of directory to use for database instead
                                  of default
  -v [ --verbose ]                Emit debugging information

Actions (if none, then match):
  -e [ --edit ]                   Edit record
  -x [ --delete ]                 Delete the results of an (exact) key match.
                                  Conceptually -E -m arg, can be used with
                                  -mDd.
  -X [ --delete-all ]             Delete all matches.  Conceptually equivalent
                                  to -m arg, can be used with -mDd.  If more
                                  than one match, will request confirmation.
  -p [ --passwd ]                 Change password (no other options permitted)
  --upgrade                       Convert a database in srd's old, weaker
                                  format to the current one, keeping its
                                  password (no other options permitted).
                                  Changing the password also converts it.
  --create                        Create database.  The database is identified
                                  by a hash of the password, so --create
                                  distinguishes between creating a new database
                                  and mistyping the password.
  --import arg                    Import a text file.  The format is the same
                                  as that output by -f:  a line "[KEY]" starts
                                  a record with key KEY, and each following
                                  line, less its two spaces of indent, is a
                                  line of that record's data (payload).  An
                                  empty line is an empty line of data.  Keys
                                  that already exist are pointed out before
                                  confirming; import never replaces a record.
  -V [ --validate ]               Confirm that all records are loadable and
                                  consistent
  --checksum                      Compute and display whole database checksum
                                  (all keys and payloads together)
  --checksum-by-key               Compute and display database checksum by key,
                                  restrictable by matching options

Matching options:
  -m [ --match-key ] arg          Restrict to records whose keys match
  -D [ --match-data-or-key ] arg  Restrict to records whose key or data match
  -d [ --match-data ] arg         Restrict to records whose data match all
                                  patterns (cf. -J)
  -E [ --exact-match ]            Exact key match
  -i [ --ignore-case ]            Match without case.  (Does not affect
                                  -g,--grep.)
  -J [ --disjunction ]            When matching multiple patterns against
                                  payloads with -d, --match-data, use inclusive
                                  or (any hit is sufficient)

Display options:
  -k [ --keys-only ]              Display keys only (default true if multiple
                                  matches, false for single match)
  -f [ --full-display ]           Display full data (default true for single
                                  match, false for multiple matches)
  -g [ --grep ] arg               Output filter for data

Test options (don't use outside regression tests):
  -T [ --TEST ] arg               Test mode, use local data directory, specify
                                  password on commandline
````

## Known issues / bugs

Several srd processes may use one database at once:  a lock in the
database directory serializes changes to the index, so they can't
lose each other's records (test-concurrency.sh checks this).  What
isn't detected is two people editing the *same* record at the same
time:  the later save wins.  Cf. issues #26, #27 (both closed).

The lock only works within one directory on one machine.  If you sync
the database directory between machines, edits to different records
sync cleanly, because each record is its own file.  But if two
machines each add, delete, or rename a record before syncing, both
rewrite the root.  Only one root survives; the other machine's change
is lost or left in a conflict copy, depending on the sync tool.
README.dev.md describes the design criteria behind this.

## File format conversion

### Format 2 (November 2026)

Databases created since November 2026 use format 2:  the password is
hashed with Argon2id and a random salt (kept in the database
directory's `srd.header`), and every file is encrypted and
authenticated with XChaCha20-Poly1305.  Format 1 hashed the password
without a salt, named the index file after that hash (so a directory
listing was enough to test password guesses), and didn't detect
changes to files.

srd still reads format 1 databases, and prints a note when it does.
To convert one, run

```
srd --upgrade
```

and give its password.  Changing the password (`srd --passwd`) also
converts it.  The conversion writes the new database before removing
the old one.  If you use the database on several machines, upgrade
on one and let the change sync; an older srd can't read format 2.

### Protobuf (May 2015)

If you began using srd before 2 May 2015, you used a file format based
on Boost serialization.  For [several
reasons](https://github.com/JeffAbrahamson/srd/issues/54), this caused
problems.  As a result, srd switched to using Google protobufs.

In the unlikely event you began using srd before the file format
change, you'll need to recover the conversion version of srd from
github.  It is on branch `convert_to_protobuf`.  README.md on that
branch explains how to convert.  After conversion, it is best to
switch back to the modern production version (branch master).
