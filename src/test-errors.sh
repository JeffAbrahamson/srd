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

# srd's resource limits are inherited by the editor, which shows that
# srd turned core dumps off.
cat > $spy <<'EOF'
#!/bin/bash
ulimit -c > "$SPY_LOG"
EOF
chmod +x $spy
SPY_LOG=$PWD/$log EDITOR=./$spy ./srd -T $pass k -e
[ "$(cat $log)" = 0 ] || fail "Core dumps not disabled:  limit $(cat $log)."

rm -f $spy $log
make clean-test
