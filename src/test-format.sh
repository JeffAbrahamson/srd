#!/bin/bash

# Test format 2 databases (srd.header, Argon2id, XChaCha20-Poly1305)
# and their coexistence with a format 1 database in one directory.

old_pass=binary-regression-test-db
pass=$(date +%s.%N)
tmp_file=test_$pass
echo setting pass=$pass
export EDITOR=./test_editor.sh
export LOGNAME=jeff		# Names the regression database's directory.
db_dir=srd-test-0000-$LOGNAME

fail() {
    echo "$@"
    exit 1
}

flip_last_byte() {
    python3 -I -c '
import sys
data = bytearray(open(sys.argv[1], "rb").read())
data[-1] ^= 1
open(sys.argv[1], "wb").write(data)' "$1"
}

make clean-test
tar xzf test.d/regression-db.tgz
[ ! -e $db_dir/srd.header ] || fail "The format 1 database has a header."
old_keys=$(./srd -T $old_pass -k '' | md5sum)

# Creating a format 2 database beside it creates the header.
printf '[k]\nv\n' | ./srd -T $pass --create -e > /dev/null ||
    fail "Creating a format 2 database failed."
[ -s $db_dir/srd.header ] || fail "Creating a database made no header."
printf '[k2]\nv2\n' | ./srd -T $pass -e > /dev/null || fail "Adding failed."
[ "$(./srd -T $pass -E k)" = "$(printf '[k]\n  v')" ] ||
    fail "Format 2 record didn't read back."

# The format 2 root's name is 64 hex digits, unrelated to the
# format 1 naming scheme.
root=$(ls $db_dir | grep -E '^[0-9a-f]{64}$')
[ 1 = $(echo "$root" | grep -c .) ] ||
    fail "Expected one format 2 root, found:  $root"

# Nothing else has changed since the header was made, so these are
# the format 2 database's records.
leaves=($(find $db_dir -type f -newer $db_dir/srd.header \
	       ! -name lock ! -name srd.header ! -name "$root"))
[ 2 = ${#leaves[@]} ] || fail "Expected two records, found:  ${leaves[*]}"

# The format 1 database is unaffected, and stays format 1 when changed.
[ "$(./srd -T $old_pass -k '' | md5sum)" = "$old_keys" ] ||
    fail "The format 1 database changed."
printf '[added]\nx\n' | ./srd -T $old_pass -e > /dev/null ||
    fail "Adding to the format 1 database failed."
./srd -T $old_pass --validate > /dev/null ||
    fail "The format 1 database didn't validate."

# Neither database may be created again.
./srd -T $old_pass --create -m '' > /dev/null 2>&1 &&
    fail "Created a database over a format 1 database."
./srd -T $pass --create -m '' > /dev/null 2>&1 &&
    fail "Created a database over a format 2 database."

# Copying one record's file over another's is detected.
cp ${leaves[0]} $tmp_file
cp ${leaves[1]} ${leaves[0]}
./srd -T $pass --validate > /dev/null 2>&1 &&
    fail "A record copied over another went unnoticed."
cp $tmp_file ${leaves[0]}
./srd -T $pass --validate > /dev/null ||
    fail "The restored record didn't validate."

# A changed header changes the keys, so the root can't be found.
cp $db_dir/srd.header $tmp_file
flip_last_byte $db_dir/srd.header
results=$(./srd -T $pass -E k 2>&1) && fail "Opened with a changed salt."
[ "$results" = "srd:  Root doesn't exist.  (Incorrect password?)" ] ||
    fail "Changed salt:  unexpected output:  $results"

# A damaged header is reported, and doesn't hide format 1 databases.
head -c 10 $tmp_file > $db_dir/srd.header
results=$(./srd -T $pass -E k 2>&1) && fail "Opened with a damaged header."
echo "$results" | grep -q "header (srd.header) is damaged" ||
    fail "Damaged header:  unexpected output:  $results"
./srd -T $old_pass --validate > /dev/null ||
    fail "A damaged header hid the format 1 database."
cp $tmp_file $db_dir/srd.header
./srd -T $pass --validate > /dev/null ||
    fail "The restored header didn't work."

rm $tmp_file
make clean-test
