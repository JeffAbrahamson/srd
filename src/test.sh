#!/bin/bash

pass=$(date +%s.%N)
tmp_file=test_$pass
echo setting pass=$pass
export EDITOR=./test_editor.sh
poems=test.d/poems/*

create=" --create "
# Stage database
for f in $poems; do
    ./srd -T $pass $create -e < $f
    create=
done

#echo Test key matching
for f in $poems; do
    key=$(head -1 $f | perl -pwe 's/^\[//; s/\]$//;')
    ./srd -T $pass "$key" | perl -pwe 's/^  //;' > $tmp_file
    cmp --quiet $f $tmp_file
    if [ 0 != $? ]; then 
	echo Match test failed.
	exit 1;
    fi
done

#echo Test exact key matching
for f in $poems; do
    key=$(head -1 $f | perl -pwe 's/^\[//; s/\]$//;')
    ./srd -T $pass "$key" -E | perl -pwe 's/^  //;' > $tmp_file
    cmp --quiet $f $tmp_file
    if [ 0 != $? ]; then 
	echo Exact match test failed.
	exit 1;
    fi
done

#echo Test partial key matching
for f in $poems; do
    key=$(head -1 $f | perl -pwe 's/^\[//; s/.\]$//;')
    ./srd -T $pass "$key" | perl -pwe 's/^  //;' > $tmp_file
    cmp --quiet $f $tmp_file
    if [ 0 != $? ]; then 
	echo Partial match test failed.
	exit 1;
    fi
done

#echo 'Test partial key matching (negative)'
for f in $poems; do
    key=$(head -1 $f | perl -pwe 's/^\[//; s/.\]$//;')
    ./srd -T $pass "$key" -E | perl -pwe 's/^  //;' > $tmp_file
    cmp --quiet $f $tmp_file
    if [ 0 = $? ]; then 
	echo Bad exact match test failed by succeeding.
	exit 1;
    fi
done

#echo Test payload matching
f=test.d/poems/A
payload_word="fired the shot"
./srd -T $pass -d "$payload_word" | perl -pwe 's/^  //;' > $tmp_file
cmp --quiet $f $tmp_file
if [ 0 != $? ]; then 
    echo Partial match test failed.
    exit 1;
fi

#echo Test payload matching with multiple hits
#./srd -T $pass -d By > test.d/output/multi-hit-keys
results="$(./srd -T $pass -d By)"
expected="$(cat test.d/output/multi-hit-keys)"
if [ "$results" != "$expected" ]; then
    echo Multi-hit partial match test failed.
    exit 1;
fi

#echo Test payload matching with multiple hits and full display
#./srd -T $pass -d By -f > test.d/output/multi-hit-full
results="$(./srd -T $pass -d By -f)"
expected=$(cat test.d/output/multi-hit-full);
if [ "$results" != "$expected" ]; then
    echo Multi-hit partial match full display test failed.
    exit 1;
fi

#echo Test payload matching with multiple hits and filtered full display
#./srd -T $pass -d By -f -g here > test.d/output/multi-hit-filtered
results="$(./srd -T $pass -d By -f -g here)"
expected=$(cat test.d/output/multi-hit-filtered);
if [ "$results" != "$expected" ]; then
    echo Multi-hit partial match test filtered display failed.
    exit 1;
fi


#echo Now test import on a new database
pass=$(date +%s.%N)

# Test import, first by saying no, then by saying yes.
results=$(echo n | ./srd -T $pass --create --import test.d/import-test)
expected=$(cat test.d/output/import-no)
if [ "$results" != "$expected" ]; then
    echo Import with negative response failed.
    exit 1;
fi

results=$(echo y | ./srd -T $pass --import test.d/import-test)
expected=$(cat test.d/output/import-yes)
if [ "$results" != "$expected" ]; then
    echo Import with positive response failed.
    exit 1;
fi

#./srd -T $pass --create > test.d/output/create-again 2>&1
results=$(./srd -T $pass --create 2>&1)
expected=$(cat test.d/output/create-again)
if [ "$results" != "$expected" ]; then
    echo Re-creation test failed.
    exit 1;
fi

# Compare sorted output, guarantees we have all the right lines at least
output=srd-import-test-output
output_sorted=srd-import-test-output-sorted
template=srd-import-test-template
./srd -T $pass '' -f > $output
sort < $output > $output_sorted
sort < test.d/import-test > $template
if ! cmp --silent $output_sorted $template; then
    echo Import test sorted output test failed.
    exit 1
fi
# This next is a repeat of the previous, but the previous confirms
# that we have the output template correct.
if ! cmp $output test.d/output/import; then
    echo Import output test failed.
    exit 1
fi
rm $output $output_sorted $template

# And clean up if all has gone well
rm $tmp_file
make clean-test
