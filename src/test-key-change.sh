#!/bin/bash

# Test that changing a record's key does not invalidate the database
# and that we can retrieve the modified record.

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
