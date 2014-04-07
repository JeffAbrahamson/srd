#!/bin/bash

# Test that a database we have previously written is still readable
# and produces the results we expect.

pass=binary-regression-test-db
tmp_file=test_$pass
echo setting pass=$pass
export EDITOR=./test_editor.sh
result_file=test.d/output/regression-db-output
db_tgz=test.d/regression-db.tgz

# Set regen=1 to regenerate the db.
# Make sure the test passes before doing so!
regen=0


#rand-string() { head -c $((25 + $RANDOM / 1000)) < /dev/urandom | sha1sum | awk '{print $1;}'; }
rand-string() { head -c 20 < /dev/urandom | cat -v; }
make-entry() {
    echo '['$(rand-string)']'
    N=$(($RANDOM / 1000))
    for((n=1; $n < $N; n++)); do
	echo '  '$(rand-string)
    done
}

if [ 1 == $regen ]; then
    make clean-test

    # This generation step is rather long, but confirmation later is
    # not so bad.  On my current desktop machine, generation takes
    # about five minutes.
    echo Generating new reference db...
    date
    for((entry=1; $entry < 5000; entry++)); do
	make-entry
    done > $tmp_file
    date

    echo Importing...
    echo yes | ./srd -T $pass --create --import $tmp_file > /dev/null
    ./srd -T $pass -f ''> $tmp_file.2
    cmp --quiet $tmp_file $tmp_file.2
    if [ 0 = $? ]; then
	echo "Failed to generate database correctly."
	exit 1
    fi
    rm $tmp_file.2
    
    db_name=srd-test-0000-$LOGNAME
    rm -rf $db_dest $result_file
    tar czf $db_tgz $db_name/
    mv $tmp_file $result_file
fi

make clean-test
echo Comparing output from reference db to stored output
# Expect the next glob should only match one thing
tar xzf $db_tgz

./srd -T $pass --validate
if [ 0 != $? ]; then
    echo Validation failed.
    exit 1;
fi

./srd -T $pass -f ''> $tmp_file
cmp --quiet $result_file $tmp_file
if [ 0 = $? ]; then 
    echo Binary match failed \($tmp_file vs $result_file\)
    exit 1;
fi

make clean-test
rm $tmp_file
