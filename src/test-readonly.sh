#!/bin/bash

pass=$(date +%s.%N)
tmp_file=test_$pass
echo setting pass=$pass
export EDITOR=./test_editor.sh
poems=test.d/poems/*

# Check that read-only works.

create=" --create "
# Stage database
for f in $poems; do
    ./srd -T $pass $create -e < $f
    create=
done

dbdir=srd-test-0000-$LOGNAME

results=$(./srd -T $pass -R -e 2>&1)
expected="Database is read-only.  Edit not permitted."
if [ "$results" != "$expected" ]; then
    echo Read-only edit test failed.
    exit 1
fi

# Deletion must not bypass read-only mode.  This used to skip the root
# commit but still remove the leaf, leaving the database corrupt.
checksum=$(./srd -T $pass --checksum)
if ./srd -T $pass -R -x 'Concord Hymn' > /dev/null 2>&1; then
    echo Read-only single delete unexpectedly succeeded.
    exit 1
fi
if [ "$(./srd -T $pass --checksum)" != "$checksum" ]; then
    echo Read-only single delete changed the database.
    exit 1
fi

if printf 'yes\n' | ./srd -T $pass -R -X '' > /dev/null 2>&1; then
    echo Read-only multiple delete unexpectedly succeeded.
    exit 1
fi
if [ "$(./srd -T $pass --checksum)" != "$checksum" ]; then
    echo Read-only multiple delete changed the database.
    exit 1
fi
./srd -T $pass --validate > /dev/null || {
    echo Read-only delete left the database invalid.
    exit 1
}

chmod 500 $dbdir
results=$(./srd -T $pass -e 2>&1)
expected='Opening database in read-only mode.
Database is read-only.  Edit not permitted.'
if [ "$results" != "$expected" ]; then
    echo Dir 500 edit test failed.
    chmod 700 $dbdir
    exit 1
fi

chmod 700 $dbdir
rm -rf $dbdir
