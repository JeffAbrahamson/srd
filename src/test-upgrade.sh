#!/bin/bash

# Test that --upgrade (and --passwd) turn a format 1 database into a
# format 2 database with the same records.  test.d/format1-db.tgz was
# made by srd before format 2 existed; test.d/output/format1-db-output
# is what that srd printed for -f ''.

pass=format1-test-db
tmp_file=test_$(date +%s.%N)
export LOGNAME=format1		# Names the fixture's directory.
db_dir=srd-test-0000-$LOGNAME
expected=test.d/output/format1-db-output
notice="Note:  this database is in srd's old, weaker format.  srd --upgrade converts it."

fail() {
    echo "$@"
    exit 1
}

make clean-test
tar xzf test.d/format1-db.tgz
old_files=$(ls $db_dir | grep -v '^lock$')

# Opening a format 1 database works, with a notice on stderr.
results=$(./srd -T $pass -f '' 2>&1 > /dev/null)
[ "$results" = "$notice" ] ||
    fail "Expected the format 1 notice, got:  $results"
./srd -T $pass -f '' 2> /dev/null | cmp -s $expected - ||
    fail "The format 1 fixture didn't read as expected."

./srd -T $pass --upgrade > $tmp_file 2>&1 ||
    fail "Upgrade failed:  $(cat $tmp_file)"
# Same records, and no more notice.
./srd -T $pass -f '' 2>&1 | cmp -s $expected - ||
    fail "The upgraded database reads differently."
./srd -T $pass --validate > /dev/null ||
    fail "The upgraded database didn't validate."
[ -s $db_dir/srd.header ] || fail "Upgrading made no header."
[ 1 = $(ls $db_dir | grep -cE '^[0-9a-f]{64}$') ] ||
    fail "Expected one format 2 root."
for f in $old_files; do
    [ ! -e "$db_dir/$f" ] || fail "Format 1 file $f remains after upgrade."
done

results=$(./srd -T $pass --upgrade 2>&1)
[ "$results" = "The database is already in the current format." ] ||
    fail "Second upgrade:  unexpected output:  $results"

# Changing the password also upgrades.  (With -T, --passwd appends 2.)
make clean-test
tar xzf test.d/format1-db.tgz
./srd -T $pass --passwd > /dev/null 2>&1 || fail "Password change failed."
./srd -T ${pass}2 -f '' 2>&1 | cmp -s $expected - ||
    fail "The database after a password change reads differently."
for f in $old_files; do
    [ ! -e "$db_dir/$f" ] ||
	fail "Format 1 file $f remains after password change."
done

make clean-test		# Also removes $tmp_file.
