# This is a release version of srd.

srd is a secure rolodesk, which is a bit of a retro name for an
encrypted set of notes.  In modern parlance, it might be called a
password vault, but the idea is to be able to write and retrieve
arbitrary bits of text.

I developed srd on an ubuntu linux system.  There's no real reason to
suspect it wouldn't work elsewhere, but that hasn't been tested.

## You are looking at the conversion branch.

On 2 May 2015, the file format changed from Boost serialization to
Google protobufs.  This is the conversion version of the program.  It
is the last version to be able to read Boost serialization.  It writes
protobufs.  Use the --convert flag to upgrade your data files.  If you
have multiple databases, even in a single directory with multiple
passwords to distinguish them, you will need to convert each in turn.

Conversion is fast (nearly instant) and very safe.  That said, please
do back up your data before converting.  It would be a shame to be the
first to discover a bug in the conversion process.

When converting, the software first tries to read files as protobufs,
which leads to harmless but noisy errors like these:

````
libprotobuf ERROR google/protobuf/message_lite.cc:123] Can't parse message of type "srd.LeafData" because it is missing required fields: key, payload
````

This is unfortunate and, in retrospect, could have been avoided.  But
it is believed this affects a single digit number of people on the
planet, so it's not worth fixing.

## How to convert

````
./srd --convert
./srd --validate
````

If you see the error
````
exc: invalid signature
````
you are using the old version srd after conversion.  This version
should validate the new file format properly.  Once you have
converted, however, I recommend switching to the production version,
as this branch will probably see few if any updates.  You will know
you are using the production version if you no longer see the
--convert flag in the --help text.

If you have retrieved srd from github, then the production version is
on branch master whilst the conversion branch is called convert-to-protobuf.

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


Here's the usage message:
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
  --convert             Convert to new file format.  This is the only option
                        you
                        should mean to use in this version.  Be sure to back up
                        your srd data before converting.

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
  --export-as-url arg   Produce a URL of the form srd://d/ url, the tail of
                        which (after the "d/") is an appropriately escaped
                        representation of an srd record.  Prompts for two
                        passwords: the db password (to read the record) and a
                        URL password (to decrypt the URL).  Also takes matching
                        flags to narrow selection of records.  Write URL to
                        file arg.  If file is "-", write to stdout.
  --import-url arg      Import URL arg, which is either the URL itself or the
                        name of a file.  Requests two passwords, the local db
                        password and the URL password.  If a record with the
                        same key exists in the current db, offer to concatenate
                        the records or to cancel the operation.  If the URL
                        represents multiple records, the action is atomic and
                        does not take effect until after all user interaction.
  -V [ --validate ]     Confirm that all records are loadable and consistent
  --checksum            Compute and display whole database checksum (all keys
                        and payloads together)
  --checksum-by-key     Compute and display database checksum by key,
                        restrictable by matching options

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

#### Known issues / bugs

The code may not be thread-safe.  Indeed, it assumes a single event
stream, where it is impossible, say, to edit two leaves at the same
time.  (The conflict here is at the level of the root, since each
editor risks re-persisting the root differently.)  There are some
checks to avoid this happening, but no regtests rigorously confirm
that this always works.  Peer review and suggestions welcome.
Cf. issues #26, #27 (both closed).
