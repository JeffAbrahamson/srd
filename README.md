# This is the release branch of srd.

srd is a secure rolodesk, which is a bit of a retro name for an
encrypted set of notes.  In modern parlance, it might be called a
password vault, but the idea is to be able to write and retrieve
arbitrary bits of text.

I developed srd on an ubuntu linux system.  There's no real reason to
suspect it wouldn't work elsewhere, but that hasn't been tested.

There's a library if you want to use the code in another context with
minimal hacking.

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
  -h [ --help ]         Produce help message
  -R [ --read-only ]    Read-only, do not persist data
  --database-dir arg    Name of directory to use for database instead of 
			default
  -v [ --verbose ]      Emit debugging information

Actions (if none, then match):
  -e [ --edit ]         Edit record
  -x [ --delete ]       Delete the results of an (exact) key match.  
			Conceptually -E -m arg, can be used with -mDd.
  -X [ --delete-all ]   Delete all matches.  Conceptually equivalent to -m arg,
			can be used with -mDd.  If more than one match, will 
			request confirmation.
  -p [ --passwd ]       Change password (no other options permitted)
  --create              Create database.  The database is identified by a hash 
			of the password, so --create distinguishes between 
			creating a new database and mistyping the password.
  --import arg          Import a text file.  The format is the same as that 
			output by -f:  each line must either be "[KEY]" for key
			KEY or else begin with two spaces of indent (which are 
			disgarded during the input) for data (payload) portion 
			associated with the most recent key line.
  -V [ --validate ]     Confirm that all records are loadable and consistent
  --checksum            Compute database checksum (keys and payload)
  --checksum-by-key     Compute database checksum by key, restrictable by 
			matching options

Matching options:
  -m [ --match-key ] arg         Restrict to records whose keys match
  -D [ --match-data-or-key ] arg Restrict to records whose key or data match
  -d [ --match-data ] arg        Restrict to records whose data match all 
				 patterns (cf. -J)
  -E [ --exact-match ]           Exact key match
  -i [ --ignore-case ]           Match without case.  (Does not affect 
				 -g,--grep.)
  -J [ --disjunction ]           When matching multiple patterns against 
				 payloads with -d, --match-data, use inclusive 
				 or (any hit is sufficient)

Display options:
  -k [ --keys-only ]     Display keys only (default true if multiple matches, 
			 false for single match)
  -f [ --full-display ]  Display full data (default true for single match, 
			 false for multiple matches)
  -g [ --grep ] arg      Output filter for data

Test options (don't use outside regression tests):
  -T [ --TEST ] arg     Test mode, use local data directory, specify password 
			on commandline
````

## Known issues / bugs

The code may not be thread-safe.  Indeed, it assumes a single event
stream, where it is impossible, say, to edit two leaves at the same
time.  (The conflict here is at the level of the root, since each
editor risks re-persisting the root differently.)  There are some
checks to avoid this happening, but no regtests rigorously confirm
that this always works.  Peer review and suggestions welcome.
Cf. issues #26, #27 (both closed).

## File format conversion

If you began using srd before 2 May 2015, you used a file format based
on Boost serialization.  For [several
reasons](https://github.com/JeffAbrahamson/srd/issues/54), this caused
problems.  As a result, srd switched to using Google protobufs.

In the unlikely event you began using srd before the file format
change, you'll need to recover the conversion version of srd from
github.  It is on branch `convert_to_protobuf`.  README.md on that
branch explains how to convert.  After conversion, it is best to
switch back to the modern production version (branch master).
