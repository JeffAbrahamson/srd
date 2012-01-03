#!/bin/bash

pass=$(date +%s.%N)
echo setting pass=$pass

# Import the animals
echo y | ./srd -T $pass --create --import test.d/import-animals

#./srd -T $pass -m '' -f > test.d/output/animals-all
results=$(./srd -T $pass -m '' -f)
expected=$(cat test.d/output/animals-all)
if [ "$results" != "$expected" ]; then
    echo Import reality check failed.
    exit 1;
fi

./srd -T $pass -x dog
#./srd -T $pass -m '' -f > test.d/output/animals-no-dogs
results=$(./srd -T $pass -m '' -f)
expected=$(cat test.d/output/animals-no-dogs)
if [ "$results" != "$expected" ]; then
    echo Dog-free test failed.
    exit 1;
fi

./srd -T $pass -x cat
#./srd -T $pass -m '' -f > test.d/output/animals-no-cats
results=$(./srd -T $pass -m '' -f)
expected=$(cat test.d/output/animals-no-cats)
if [ "$results" != "$expected" ]; then
    echo Dog-free test failed.
    exit 1;
fi

# And clean up if all has gone well
make clean-test
