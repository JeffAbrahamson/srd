#!/bin/bash

# Test that large records can be stored and read back.  Buffers on
# the stack used to overflow at a few megabytes.

pass=$(date +%s.%N)
echo setting pass=$pass
export EDITOR=./test_editor.sh
tmp_file=test_$pass

fail() {
    echo "$@"
    exit 1
}

{ echo '[big]'; head -c 12000000 /dev/zero | tr '\0' 'a'; echo; } > $tmp_file
./srd -T $pass --create -e < $tmp_file || fail "Failed to store a large record."
./srd -T $pass big | perl -pwe 's/^  //;' | cmp --quiet - $tmp_file ||
    fail "Large record did not read back intact."

rm $tmp_file
make clean-test
