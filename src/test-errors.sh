#!/bin/bash

# Test that errors are reported cleanly (message, exit status 1)
# rather than by abort(), and that srd cannot dump core.

pass=$(date +%s.%N)
echo setting pass=$pass
spy=test_$pass-spy
log=test_$pass-log

fail() {
    echo "$@"
    exit 1
}

ulimit -c unlimited 2>/dev/null

printf '[k]\nv\n' | EDITOR=./test_editor.sh ./srd -T $pass --create -e

results=$(./srd -T wrong-$pass k 2>&1)
status=$?
[ 1 = $status ] || fail "Wrong password:  expected exit 1, got $status."
[ "$results" = "srd:  Root doesn't exist.  (Incorrect password?)" ] ||
    fail "Wrong password:  unexpected output:  $results"

# LOGNAME names the test database directory; srd must cope without it.
env -u LOGNAME ./srd -T other-$pass --create -m '' > /dev/null 2>&1 ||
    fail "Failed with LOGNAME unset."

# srd's resource limits are inherited by the editor, which shows that
# srd turned core dumps off.
cat > $spy <<'EOF'
#!/bin/bash
ulimit -c > "$SPY_LOG"
EOF
chmod +x $spy
SPY_LOG=$PWD/$log EDITOR=./$spy ./srd -T $pass k -e
[ "$(cat $log)" = 0 ] || fail "Core dumps not disabled:  limit $(cat $log)."

# A database directory others can change is refused; one they can
# only list draws a warning.
db_dir=srd-test-0000-$LOGNAME
chmod 770 $db_dir
results=$(./srd -T $pass k 2>&1)
status=$?
chmod 700 $db_dir
[ 1 = $status ] || fail "Group-writable directory:  expected exit 1, got $status."
echo "$results" | grep -q "Others may write to the database directory" ||
    fail "Group-writable directory:  unexpected output:  $results"
chmod 750 $db_dir
results=$(./srd -T $pass k 2>&1 > /dev/null)
status=$?
chmod 700 $db_dir
[ 0 = $status ] || fail "Group-readable directory:  expected exit 0, got $status."
[ 1 = $(echo "$results" | grep -c "Warning:  others may list") ] ||
    fail "Group-readable directory:  expected one warning, got:  $results"
results=$(./srd -T $pass k 2>&1 > /dev/null)
[ -z "$results" ] || fail "Private directory:  unexpected output:  $results"

rm -f $spy $log
make clean-test
