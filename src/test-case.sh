#!/bin/bash

pass=$(date +%s.%N)
tmp_file=test_$pass
echo setting pass=$pass
export EDITOR=./test_editor.sh
poems=test.d/poems/*

# Test to confirm that -i,--ignore-case works.
# This is, at its birth, just test.sh with some case fiddling.

# command to invert case
case="tr 'a-zA-Z' 'A-Za-z'"

create=" --create "
# Stage database
for f in $poems; do
    ./srd -T $pass $create -e < $f
    create=
done

#echo Test key matching
for f in $poems; do
    key=$(head -1 $f | perl -pwe 's/^\[//; s/\]$//;' | $case )
    ./srd -T $pass "$key" -i | perl -pwe 's/^  //;' > $tmp_file
    cmp --quiet $f $tmp_file
    if [ 0 != $? ]; then 
	echo Match test failed.
	exit 1;
    fi
done

#echo Test exact key matching
for f in $poems; do
    key=$(head -1 $f | perl -pwe 's/^\[//; s/\]$//;' | $case)
    ./srd -T $pass "$key" -E --ignore-case | perl -pwe 's/^  //;' > $tmp_file
    cmp --quiet $f $tmp_file
    if [ 0 != $? ]; then 
	echo Exact match test failed.
	exit 1;
    fi
done

#echo Test partial key matching
for f in $poems; do
    key=$(head -1 $f | perl -pwe 's/^\[//; s/.\]$//;' | $case)
    ./srd -T $pass "$key" -i | perl -pwe 's/^  //;' > $tmp_file
    cmp --quiet $f $tmp_file
    if [ 0 != $? ]; then 
	echo Partial match test failed: $f
	exit 1;
    fi
done

#echo 'Test partial key matching (negative)'
for f in $poems; do
    key=$(head -1 $f | perl -pwe 's/^\[//; s/.\]$//;' | $case)
    ./srd -T $pass "$key" -E --ignore-case | perl -pwe 's/^  //;' > $tmp_file
    cmp --quiet $f $tmp_file
    if [ 0 = $? ]; then 
	echo Bad exact match test failed by succeeding.
	exit 1;
    fi
done

#echo Test payload matching
f=test.d/poems/A
payload_word="Fired the SHOT"
./srd -T $pass -d "$payload_word" -i | perl -pwe 's/^  //;' > $tmp_file
cmp --quiet $f $tmp_file
if [ 0 != $? ]; then 
    echo Partial payload match test failed: $f
    exit 1;
fi

#echo Test payload matching with multiple hits
#./srd -T $pass -d By > test.d/output/multi-hit-keys
results="$(./srd -T $pass -d bY -i)"
expected="$(cat test.d/output/multi-hit-keys)"
if [ "$results" != "$expected" ]; then
    echo Multi-hit partial match test failed.
    exit 1;
fi

#echo Test payload matching with multiple hits and full display
#./srd -T $pass -d By -f > test.d/output/multi-hit-full
results="$(./srd -T $pass -d bY -f --ignore-case)"
expected=$(cat test.d/output/multi-hit-full);
if [ "$results" != "$expected" ]; then
    echo Multi-hit partial match full display test failed.
    exit 1;
fi

#echo Test payload matching with multiple hits and filtered full display
#./srd -T $pass -d By -f -g here > test.d/output/multi-hit-filtered
results="$(./srd -T $pass -d bY -fi -g here)"
expected=$(cat test.d/output/multi-hit-filtered);
if [ "$results" != "$expected" ]; then
    echo Multi-hit partial match test filtered display failed.
    exit 1;
fi


# And clean up if all has gone well
rm $tmp_file
make clean-test
