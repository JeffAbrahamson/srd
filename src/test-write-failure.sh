#!/bin/bash

# Test that a failed write (disk full, quota, ...) leaves the
# existing record intact rather than replacing it with a truncated
# file.

pass=$(date +%s.%N)
echo setting pass=$pass
export EDITOR=./test_editor.sh
tmp_file=test_$pass

fail() {
    echo "$@"
    exit 1
}

# Incompressible payloads, so the stored leaf is well over the limit.
{ echo '[big]'; head -c 30000 /dev/urandom | base64; } > $tmp_file.old
{ echo '[big]'; head -c 30000 /dev/urandom | base64; } > $tmp_file.new
./srd -T $pass --create -e < $tmp_file.old
./srd -T $pass big | perl -pwe 's/^  //;' | cmp --quiet - $tmp_file.old ||
    fail "Reality check failed."

# Limit file size to 8 KB.  With SIGXFSZ ignored, writes past the
# limit fail with EFBIG, as they would on a full disk.
(trap '' XFSZ; ulimit -f 8; ./srd -T $pass big -e < $tmp_file.new) \
    > /dev/null 2>&1 && fail "Write past the size limit should have failed."

./srd -T $pass --validate > /dev/null || fail "Database invalid after failed write."
./srd -T $pass big | perl -pwe 's/^  //;' | cmp --quiet - $tmp_file.old ||
    fail "Record damaged by failed write."
[ -z "$(ls srd-test-0000-$LOGNAME | grep '\.new$')" ] ||
    fail "Temporary .new file left behind."

rm $tmp_file.old $tmp_file.new
make clean-test
