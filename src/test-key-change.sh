#!/bin/bash

# Test that changing a record's key does not invalidate the database
# and that we can retrieve the modified record.

make clean-test

pass=$(date +%s.%N)
tmp_file=test_$pass
echo setting pass=$pass
export EDITOR=./test_editor.sh

echo y | ./srd -T $pass --create --import test.d/import-animals
expected=$(./srd -T $pass cow | perl -pwe 's/cow/cowl/;')
results=$(./srd -T $pass cow)
if [ "$results" = "$expected" ]; then
    echo Key change failed: before change
    exit 1;
fi

./srd -T $pass cow | perl -pwe 's/cow/cowl/; s/  //;' | ./srd -T $pass cow -e
results=$(./srd -T $pass cowl)
if [ "$results" != "$expected" ]; then
    echo Key change failed.
    exit 1;
fi

if ! ./srd -T $pass -V; then
    echo "Validation should have succeeded but didn't."
    exit 1;
fi
test_dir=srd-test-0000-$LOGNAME
sacrificial_file=$(ls -t $test_dir/ | tail -1)
rm $test_dir/$sacrificial_file

echo
echo This next test should fail with a message about validation failure.
if ./srd -T $pass -V; then
    echo Validation should have failed but instead succeeded.
    exit 1
fi
echo "Looks like it failed on schedule.  So this test succeeds."
