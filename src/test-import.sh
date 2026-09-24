#!/bin/bash

# Test import parsing corner cases.

pass=$(date +%s.%N)
echo setting pass=$pass
tmp_file=test_$pass

fail() {
    echo "$@"
    exit 1
}

./srd -T $pass --create -m '' > /dev/null
printf '[existing]\n  x\n' > $tmp_file
echo y | ./srd -T $pass --import $tmp_file > /dev/null

# A record with no payload lines is kept.  An empty line is an empty
# payload line (an editor may strip the indent of "  ").
printf '[empty]\n[blank lines]\n  one\n\n  three\n[existing]\n  y\n[empty]\n' \
    > $tmp_file
results=$(echo y | ./srd -T $pass --import $tmp_file)
expected=$(printf 'Found 4 keys:\n\tempty\n\tblank lines\n\texisting  (already exists)\n\tempty  (repeated in file)\nImport?  (yes/no)  ')
[ "$results" = "$expected" ] || fail "Unexpected import listing:  $results"

[ 5 = $(./srd -T $pass -k '' | wc -l) ] || fail "Import lost a record."
[ "$(./srd -T $pass -E 'blank lines')" = "$(printf '[blank lines]\n  one\n  \n  three')" ] ||
    fail "Blank payload line not imported."

# Data before the first key is an error, and nothing is imported.
printf '  orphan\n[k]\n  v\n' > $tmp_file
results=$(echo y | ./srd -T $pass --import $tmp_file 2>&1)
echo "$results" | grep -q 'expected a \[KEY\] line' ||
    fail "Data before first key not rejected:  $results"
[ 5 = $(./srd -T $pass -k '' | wc -l) ] || fail "Rejected import changed the database."

rm $tmp_file
make clean-test
