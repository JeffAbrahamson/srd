#!/bin/bash

# Test that records sharing a key are all shown and all counted.

pass=$(date +%s.%N)
echo setting pass=$pass
export EDITOR=./test_editor.sh

fail() {
    echo "$@"
    exit 1
}

printf '[dup]\nfirst\n' | ./srd -T $pass --create -e
printf '[dup]\nsecond\n' | ./srd -T $pass -e
printf '[other]\nthird\n' | ./srd -T $pass -e

results=$(./srd -T $pass -k '')
expected=$(printf '[dup]\n[dup]\n[other]')
[ "$results" = "$expected" ] || fail "Keys-only listing lost a duplicate:  $results"

results=$(./srd -T $pass -f dup | LC_ALL=C sort)
expected=$(printf '  first\n  second\n[dup]\n[dup]')
[ "$results" = "$expected" ] || fail "Full listing lost a duplicate:  $results"

[ 3 = $(./srd -T $pass --checksum-by-key | wc -l) ] ||
    fail "Checksum by key lost a duplicate."

# The whole-database checksum must cover both records.  Deleting
# either one must change it.
full=$(./srd -T $pass --checksum)
echo yes | ./srd -T $pass -X other > /dev/null
without_other=$(./srd -T $pass --checksum)
[ "$full" != "$without_other" ] || fail "Checksum ignores a record."
./srd -T $pass -d second | grep -q dup || fail "Reality check failed."
printf '[dup]\nchanged\n' | ./srd -T $pass -d second -e
[ "$(./srd -T $pass --checksum)" != "$without_other" ] ||
    fail "Checksum ignores a duplicate's payload."

# Checksums are keyed per database, so they can't be used to test
# guesses at records:  the same contents give different checksums in
# different databases.
printf '[same]\ncontents\n' | ./srd -T a-$pass --create -e
printf '[same]\ncontents\n' | ./srd -T b-$pass --create -e
[ "$(./srd -T a-$pass --checksum)" != "$(./srd -T b-$pass --checksum)" ] ||
    fail "Two databases share a checksum key."
[ "$(./srd -T a-$pass --checksum-by-key)" != \
  "$(./srd -T b-$pass --checksum-by-key)" ] ||
    fail "Two databases share a checksum-by-key key."

make clean-test
