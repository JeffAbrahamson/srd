#!/bin/bash

# Test that concurrent changes don't lose each other.  Each srd
# reloads the root, changes it and writes it back; without a lock
# around all of that, one process can overwrite another's change.

pass=$(date +%s.%N)
echo setting pass=$pass
export EDITOR=./test_editor.sh
n=30

fail() {
    echo "$@"
    exit 1
}

./srd -T $pass --create -m '' > /dev/null

for i in $(seq $n); do
    printf '[k%02d]\nv\n' $i | ./srd -T $pass -e > /dev/null &
done
wait

count=$(./srd -T $pass -k '' | wc -l)
[ $n = $count ] || fail "Concurrent adds lost records:  $count of $n remain."
./srd -T $pass --validate > /dev/null || fail "Validation failed after concurrent adds."

# Concurrent deletes of different records, likewise.
for i in $(seq 2 2 $n); do
    ./srd -T $pass -x $(printf 'k%02d' $i) > /dev/null &
done
wait

count=$(./srd -T $pass -k '' | wc -l)
[ $((n / 2)) = $count ] ||
    fail "Concurrent deletes went wrong:  $count of $((n / 2)) remain."
./srd -T $pass --validate > /dev/null || fail "Validation failed after concurrent deletes."

make clean-test
