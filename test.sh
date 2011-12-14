#!/bin/bash

pass=$(date +%s.%N)
tmp_file=test_$pass
echo setting pass=$pass
export EDITOR=./test_editor.sh
poems=test.d/poems/*

# Stage database
for f in $poems; do
    ./srd -T $pass -e < $f
done

# Test key matching
for f in $poems; do
    key=$(head -1 $f | perl -pwe 's/^\[//; s/\]$//;')
    ./srd -T $pass "$key" | perl -pwe 's/^  //;' > $tmp_file
    cmp --quiet $f $tmp_file
    if [ 0 != $? ]; then 
	echo Match test failed.
	exit 1;
    fi
done

# Test exact key matching
for f in $poems; do
    key=$(head -1 $f | perl -pwe 's/^\[//; s/\]$//;')
    ./srd -T $pass "$key" -E | perl -pwe 's/^  //;' > $tmp_file
    cmp --quiet $f $tmp_file
    if [ 0 != $? ]; then 
	echo Exact match test failed.
	exit 1;
    fi
done

# Test partial key matching
for f in $poems; do
    key=$(head -1 $f | perl -pwe 's/^\[//; s/.\]$//;')
    ./srd -T $pass "$key" | perl -pwe 's/^  //;' > $tmp_file
    cmp --quiet $f $tmp_file
    if [ 0 != $? ]; then 
	echo Partial match test failed.
	exit 1;
    fi
done

# Test partial key matching (negative)
for f in $poems; do
    key=$(head -1 $f | perl -pwe 's/^\[//; s/.\]$//;')
    ./srd -T $pass "$key" -E | perl -pwe 's/^  //;' > $tmp_file
    cmp --quiet $f $tmp_file
    if [ 0 = $? ]; then 
	echo Bad exact match test failed by succeeding.
	exit 1;
    fi
done

# Test payload matching
f=test.d/poems/A
payload_word="fired the shot"
./srd -T $pass -d "$payload_word" | perl -pwe 's/^  //;' > $tmp_file
cmp --quiet $f $tmp_file
if [ 0 != $? ]; then 
    echo Partial match test failed.
    exit 1;
fi

# Test payload matching with multiple hits
results="$(./srd -T $pass -d By | sort)"
expected='[Concord Hymn]
[The Raven]'
if [ "$results" != "$expected" ]; then
    echo Multi-hit partial match test failed.
    exit 1;
fi

# Test payload matching with multiple hits and full display
#./srd -T $pass -d By -f > test.d/output/multi-hit-full
results="$(./srd -T $pass -d By -f)"
expected=$(cat test.d/output/multi-hit-full);
if [ "$results" != "$expected" ]; then
    echo Multi-hit partial match full display test failed.
    exit 1;
fi

# Test payload matching with multiple hits and filtered full display
#./srd -T $pass -d By -f -g here > test.d/output/multi-hit-filtered
results="$(./srd -T $pass -d By -f -g here)"
expected=$(cat test.d/output/multi-hit-filtered);
if [ "$results" != "$expected" ]; then
    echo Multi-hit partial match test filtered display failed.
    exit 1;
fi


# And clean up
rm $tmp_file
