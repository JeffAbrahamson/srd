#!/bin/bash

pass=$(date +%s.%N)
pass2=${pass}2			# hard-coded test behavior in main.cpp
tmp_file=test_$pass
echo setting pass=$pass
echo setting pass2=$pass2
export EDITOR=./test_editor.sh
poems=test.d/poems/*

create=" --create "
# Stage database
for f in $poems; do
    ./srd -T $pass $create -e < $f
    create=
done

./srd -T $pass --passwd

# Test key matching
for f in $poems; do
    key=$(head -1 $f | perl -pwe 's/^\[//; s/\]$//;')
    ./srd -T $pass2 "$key" | perl -pwe 's/^  //;' > $tmp_file
    cmp --quiet $f $tmp_file
    if [ 0 != $? ]; then 
	echo Match test failed.
	exit 1;
    fi
done

# Test exact key matching
for f in $poems; do
    key=$(head -1 $f | perl -pwe 's/^\[//; s/\]$//;')
    ./srd -T $pass2 "$key" -E | perl -pwe 's/^  //;' > $tmp_file
    cmp --quiet $f $tmp_file
    if [ 0 != $? ]; then 
	echo Exact match test failed.
	exit 1;
    fi
done

# Test partial key matching
for f in $poems; do
    key=$(head -1 $f | perl -pwe 's/^\[//; s/.\]$//;')
    ./srd -T $pass2 "$key" | perl -pwe 's/^  //;' > $tmp_file
    cmp --quiet $f $tmp_file
    if [ 0 != $? ]; then 
	echo Partial match test failed.
	exit 1;
    fi
done

# Test partial key matching (negative)
for f in $poems; do
    key=$(head -1 $f | perl -pwe 's/^\[//; s/.\]$//;')
    ./srd -T $pass2 "$key" -E | perl -pwe 's/^  //;' > $tmp_file
    cmp --quiet $f $tmp_file
    if [ 0 = $? ]; then 
	echo Bad exact match test failed by succeeding.
	exit 1;
    fi
done

# Test payload matching
f=test.d/poems/A
payload_word="fired the shot"
./srd -T $pass2 -d "$payload_word" | perl -pwe 's/^  //;' > $tmp_file
cmp --quiet $f $tmp_file
if [ 0 != $? ]; then 
    echo Partial match test failed.
    exit 1;
fi

# Test payload matching with multiple hits
results="$(./srd -T $pass2 -d By | sort)"
expected='[Concord Hymn]
[The Raven]'
if [ "$results" != "$expected" ]; then
    echo Multi-hit partial match test failed.
    exit 1;
fi

# Test payload matching with multiple hits and full display
#./srd -T $pass2 -d By -f > test.d/output/multi-hit-full
results="$(./srd -T $pass2 -d By -f)"
expected=$(cat test.d/output/multi-hit-full);
if [ "$results" != "$expected" ]; then
    echo Multi-hit partial match full display test failed.
    exit 1;
fi

# Test payload matching with multiple hits and filtered full display
#./srd -T $pass2 -d By -f -g here > test.d/output/multi-hit-filtered
results="$(./srd -T $pass2 -d By -f -g here)"
expected=$(cat test.d/output/multi-hit-filtered);
if [ "$results" != "$expected" ]; then
    echo Multi-hit partial match test filtered display failed.
    exit 1;
fi


# Now test import on a new database
pass=$(date +%s.%N)

# Test import, first by saying no, then by saying yes.
results=$(echo n | ./srd -T $pass2 --create --import test.d/import-test)
expected=$(cat test.d/output/import-no)
if [ "$results" != "$expected" ]; then
    echo Import with negative response failed.
    exit 1;
fi

results=$(echo y | ./srd -T $pass2 --import test.d/import-test)
expected=$(cat test.d/output/import-yes)
if [ "$results" != "$expected" ]; then
    echo Import with positive response failed.
    exit 1;
fi

#./srd -T $pass2 --create > test.d/output/create-again 2>&1
results=$(./srd -T $pass2 --create 2>&1)
expected=$(cat test.d/output/create-again)
if [ "$results" != "$expected" ]; then
    echo Re-creation test failed.
    exit 1;
fi

# And clean up if all has gone well
rm $tmp_file
make clean-test
