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

make clean-test
