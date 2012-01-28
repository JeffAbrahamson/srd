#!/bin/bash

# Test that a database we have previously written is still readable
# and produces the resuls we expect.

pass=binaray-regression-test-db
tmp_file=test_$pass
echo setting pass=$pass
export EDITOR=./test_editor.sh
result_file=test.d/output/regression-db-output

# Set regen=1 to regenerate the db.
# Make sure the test passes before doing so!
regen=1


rand-string() { head -c $((25 + $RANDOM / 1000)) < /dev/urandom | sha1sum | awk '{print $1;}'; }
#rand-string() { head -c 25 < /dev/urandom | cat -v; }
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
    # not so bad.
    make-entry | ./srd -T $pass --create -e
    for((n=1; $n < 500; n++)); do
	make-entry | time ./srd -T $pass -e
	if [ $((n % 50)) == 0 ]; then
	    echo -n .
	    date
	fi
    done

    ./srd -f ''> $tmp_file
    
    db_name=srd-test-0000-$LOGNAME
    db_dest=test.d/regression-db/$db_name
    rm -rf $db_dest $result_file
    mv $db_name $db_dest
    mv $tmp_file $result_file
fi

make clean-test
# Expect the next glob should only match one thing
cp -r test.d/regression-db/srd-test-0000-* ./
./srd -f ''> $tmp_file
cmp --quiet $result_file $tmp_file
if [ 0 = $? ]; then 
    echo Binary match failed ($tmp_file vs $result_file)
    exit 1;
fi

make clean-test
