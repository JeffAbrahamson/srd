# About the srd code

This is a quick tour of the srd source code.

## Functional overview

From a functional viewpoint, srd manages a set of text fragments.
Think of them as text on sticky (PostIt(TM)) notes.  Each text
fragment has a name, which is also text.  Text may be retrieved from a
single note or from multiple notes and is searchable.  We'll refer to
the name and the note as key and payload.

## Implementation overview

A database is a directory holding an index (called the root) and one
file per record (called a leaf).  Each leaf holds a record's key and
payload, and has a random file name that never changes.  The root
lists the leaves' file names, with each leaf's key cached so that
searching on keys needn't open every leaf.

The root doesn't store the password or a hash of it.  Instead, the
root's file name is derived from the password, so the password both
finds the root and decrypts it.  An incorrect password finds no root.
This is also how one directory can hold several databases:  each
password names a different root.

Every file is serialized with protobuf (`root.proto`, `leaf.proto`),
compressed, and then encrypted.

### File formats

Format 2 (`header.cc`, and the libsodium half of `crypt.cc`):

* The directory's header, `srd.header`, holds the Argon2id costs and a
  random salt.  It's written when the first format 2 database in the
  directory is created and never changes, so the databases in a
  directory share it.  It isn't authenticated, and needn't be:
  changing it changes every key derived from it, so no root can be
  found.  srd refuses costs below libsodium's "interactive" level
  (except in test mode), so a header planted before a database is
  created can't make guessing cheap.

* Argon2id turns the passphrase and salt into a master key, from which
  `crypto_kdf` derives an encryption key and a root-naming key.  The
  root's name is a keyed hash, so a directory listing doesn't let
  anyone test password guesses without paying for Argon2id.

* Each file is `SRD2`, a random 24-byte nonce, and the
  XChaCha20-Poly1305 ciphertext.  The authenticated context is the
  file's role and name (`root:<name>`, `leaf:<name>`), so a file that
  is changed, truncated, or copied over another doesn't decrypt.

* `--checksum` and `--checksum-by-key` print keyed hashes, under a
  random key stored in the root, so the output can't be used to test
  guesses at records' contents.  Password changes keep the key, so
  checksums stay comparable across them.  (Roots from before the key
  existed get one derived from the database key.)

* What isn't detected:  replacing a file with an older version of
  itself, or the whole directory with an older copy.  Detecting that
  would need state outside the directory or a root that changes on
  every edit, which the design criteria below rule out.

* What isn't hidden:  anyone who can list the directory (a sync
  service, say) sees how many records there are, roughly how large
  each is (files aren't padded), and when each was last written.
  Hiding that would mean padding files or rewriting more of them on
  each edit.

Format 1, still read so that old databases can be converted:  the key
is 50 rounds of unsalted SHA-256 of the passphrase (in test mode, the
passphrase itself), the root's name is 30 more rounds, and files are
AES-128-CBC with a fixed IV and no MAC.

To open a database, srd tries format 2 if the directory has a header,
then format 1.  New databases, including those made by a password
change, are always format 2, so changing the password converts a
format 1 database.  There's deliberately no way to convert and keep
the password, which old copies of the format 1 files would make cheap
to guess.  `test.d/format1-db.tgz` is a small format 1 database, made
by srd before format 2, for testing the conversion.

## Design criteria

Two properties shape the on-disk format.  Changes to the format
should preserve them.

* **One database, several machines.**  A user may use the same
  database from several machines, by sharing or synchronising its
  directory.  Nothing about a database may depend on state kept
  outside its directory, such as a per-machine counter or cache that
  the other machines don't share.

* **Friendly to backups and to sync between machines.**  A change
  rewrites as few files as possible:  editing a record's payload
  rewrites only that record's file, and adding, deleting, or renaming
  a record rewrites that record's file and the root.  So incremental
  backups stay small, and when two machines sync the same directory,
  most concurrent edits touch different files and don't conflict.
  Restoring a backup of the whole directory must give a usable
  database.

The second property is why each record lives in its own file, and why
data that would change on every edit (per-record version numbers or
MACs, for instance) doesn't belong in the root.  A known gap:  the
root isn't protected from two-sided changes.  If two machines each
add, delete, or rename a record before syncing, their roots conflict.
Only one root survives; the other machine's change is lost or left in
a conflict copy, depending on the sync tool.

## File overview

The principal entry point for the program is `main.cc`.  In other
words, it contains `main()`.  It handles option parsing, password
entry, and running the user's editor.  Everything else is (at least
conceptually) library.

The index object is `root.cc`.  It is tested by `root_test.cc`.

The root's main data structure is a leaf_proxy_map (implemented in
`leaf_proxy_map.cc`, tested by `leaf_proxy_test.cc`).  The
leaf_proxy_map is also the abstraction by which we return search
results, facilitating searching on existing results.  Said
differently, the root is a leaf_proxy_map with persistence.

A leaf proxy (`leaf_proxy.cc`) represents a leaf without loading the
data.  Once we load the file, the leaf proxy contains a pointer to the
leaf (`leaf.cc`, tested in `leaf_test.cc`).  This is the *virtual
proxy* pattern:  a stand-in that loads the real object on first use.

The files `compress.cc` (tested by `compress_test.cc`) and `crypt.cc`
(tested by `crypt_test.cc`) handle compression/decompression and
encryption/decryption respectively.  `header.cc` (tested by
`header_test.cc`) reads and writes the directory's header.  File
operations are abstracted
in the mixin class `file.cc` (tested by `file_test.cc`), with a few
free functions in `file_util.cc` (tested by `file_util_test.cc`).
`mode.cc` (tested in `mode_test.cc`) isolates a bit of global state:
verbose mode, test mode, and read-only mode.

`lock.cc` (tested by `lock_test.cc`) provides the database lock:  a
file named `lock` in the database directory, taken with `flock()` and
never removed.  Anything that reads the root, changes it, and writes
it back holds the lock throughout, so that concurrent srd processes
can't lose each other's changes.  Every file is replaced by an atomic
rename, so reading a record needs no lock.

Many of the tests require text to manipulate.  The file `test_text.cc`
provides that text.

## Building

`make` builds with `-O2`, which also turns on the compiler's
`_FORTIFY_SOURCE` checks where it enables them by default (as
Ubuntu's does).  `make OPTIMIZE=-O0` builds without optimizing, for
debugging.

## Testing

`make test` builds and runs the unit tests (`*_test.cc`), then the
end-to-end tests:  shell scripts (`test*.sh`) that drive the srd
binary in test mode (`-T`, which takes the password on the command
line and uses a database directory under the current directory), and
`test-tty.py`, which types passwords at srd on a pseudo-terminal.  Any
failure stops the build.

`make test-sanitize` runs the same tests with AddressSanitizer and
UndefinedBehaviorSanitizer.

GitHub Actions (`.github/workflows/test.yml`) runs both on Ubuntu
24.04 for every push and pull request.  Its package list should match
`README.packages`.
