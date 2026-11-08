#!/bin/bash

# Test that --validate and --checksum report corrupt records as
# failures.

pass=$(date +%s.%N)
echo setting pass=$pass
export EDITOR=./test_editor.sh
db_dir=srd-test-0000-$LOGNAME

fail() {
    echo "$@"
    exit 1
}

{ echo '[k]'; seq 1000; } | ./srd -T $pass --create -e
./srd -T $pass --validate > /dev/null || fail "Reality check failed."

# Corrupt the first cipher block of the leaf (the larger file).  The
# padding in the last block stays valid, so decryption "succeeds" and
# the damage shows up when bzip2 checks its data.
leaf=$db_dir/$(ls -S $db_dir | head -1)
printf '\xff' | dd of=$leaf bs=1 seek=5 conv=notrunc status=none

./srd -T $pass --validate > /dev/null &&
    fail "Validation of a corrupt record should fail."
./srd -T $pass --checksum > /dev/null &&
    fail "Checksum of a corrupt record should fail."

# A record whose file is missing is an error, not an empty record,
# whether or not its key is empty.  (The root caches keys, so an empty
# key used to read as an empty record that validated.)
make clean-test
./srd -T $pass --create > /dev/null
import_file=test_$(date +%s.%N)
printf '[]\n  empty key\n[k]\n  some key\n' > $import_file
echo yes | ./srd -T $pass --import $import_file > /dev/null
rm -f $import_file
# Leaves are everything but the lock, the header and the root.
leaves=$(ls $db_dir | grep -vE '^(lock|srd\.header|[0-9a-f]{64})$')
[ 2 = $(echo "$leaves" | wc -l) ] || fail "Expected two leaves:  $leaves"
for leaf in $leaves; do
    mv $db_dir/$leaf $db_dir/aside
    ./srd -T $pass --validate > /dev/null 2>&1 &&
        fail "Validation with a record's file missing should fail."
    results=$(./srd -T $pass -f '' 2>&1 > /dev/null)
    case "$results" in
        *"The file for a record is missing"*) ;;
        *) fail "Expected a missing file error, got:  $results" ;;
    esac
    mv $db_dir/aside $db_dir/$leaf
done
./srd -T $pass --validate > /dev/null ||
    fail "Validation failed with every record's file back."

make clean-test
