#!/bin/bash

# Test that changing the password turns a format 1 database into a
# format 2 database with the same records.  test.d/format1-db.tgz was
# made by srd before format 2 existed; test.d/output/format1-db-output
# is what that srd printed for -f ''.

pass=format1-test-db
export LOGNAME=format1		# Names the fixture's directory.
db_dir=srd-test-0000-$LOGNAME
expected=test.d/output/format1-db-output
notice="Note:  this database is in srd's old, weaker format.  Change its password (srd --passwd) to convert it."

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
# Checksums survive the conversion, so copies can be compared across it.
checksum=$(./srd -T $pass --checksum 2> /dev/null)
by_key=$(./srd -T $pass --checksum-by-key 2> /dev/null)

# Changing the password converts.  (With -T, --passwd appends 2.)
./srd -T $pass --passwd > /dev/null 2>&1 || fail "Password change failed."
# Same records, and no more notice.
./srd -T ${pass}2 -f '' 2>&1 | cmp -s $expected - ||
    fail "The database after a password change reads differently."
./srd -T ${pass}2 --validate > /dev/null ||
    fail "The converted database didn't validate."
[ -s $db_dir/srd.header ] || fail "Converting made no header."
[ 1 = $(ls $db_dir | grep -cE '^[0-9a-f]{64}$') ] ||
    fail "Expected one format 2 root."
for f in $old_files; do
    [ ! -e "$db_dir/$f" ] ||
	fail "Format 1 file $f remains after password change."
done
./srd -T $pass -f '' > /dev/null 2>&1 &&
    fail "The old password still opens a database."

[ "$(./srd -T ${pass}2 --checksum)" = "$checksum" ] ||
    fail "The checksum changed on password change."
[ "$(./srd -T ${pass}2 --checksum-by-key)" = "$by_key" ] ||
    fail "Checksums by key changed on password change."

# The option that converted while keeping the password is gone.
./srd -T ${pass}2 --upgrade > /dev/null 2>&1 &&
    fail "--upgrade should no longer be accepted."

make clean-test
