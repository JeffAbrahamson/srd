#!/bin/bash

# Test that editing a record keeps the plaintext copy private and
# always cleans it up.

pass=$(date +%s.%N)
echo setting pass=$pass
spy=test_$pass-spy
log=test_$pass-log
export TMPDIR=$PWD/test_$pass-tmp
mkdir $TMPDIR

# An editor that records what it sees, then behaves like
# test_editor.sh.  It takes an extra argument to check that EDITOR
# may carry arguments.
cat > $spy <<'EOF'
#!/bin/bash
[ "$1" = "--flag" ] || { echo "bad args: $*" >> "$SPY_LOG"; exit 1; }
file=$2
stat -c '%a' "$(dirname "$file")" "$file" >> "$SPY_LOG"
[ -n "$SPY_SLEEP" ] && { touch "$SPY_LOG.started"; sleep "$SPY_SLEEP"; }
cat > "$file"
touch "$SPY_LOG.finished"
EOF
chmod +x $spy
export EDITOR="./$spy --flag"
export SPY_LOG=$PWD/$log

fail() {
    echo "$@"
    exit 1
}

printf '[secret]\nhunter2\n' | ./srd -T $pass --create -e
[ "$(cat $log)" = "$(printf '700\n600')" ] ||
    fail "Temp file permissions wrong:  $(cat $log)"
[ -z "$(ls -A $TMPDIR)" ] || fail "Temp files left behind:  $(ls -A $TMPDIR)"
[ "$(./srd -T $pass secret)" = "$(printf '[secret]\n  hunter2')" ] ||
    fail "Edited record not saved."

# SIGTERM while the editor runs: srd must outlive the editor, remove
# the plaintext, and only then die of the signal.
rm -f $log $log.*
printf '[other]\nx\n' | SPY_SLEEP=2 ./srd -T $pass -e &
srd_pid=$!
for i in $(seq 50); do [ -e $log.started ] && break; sleep 0.1; done
[ -e $log.started ] || fail "Editor never started."
kill -TERM $srd_pid
wait $srd_pid
status=$?
[ 143 = $status ] || fail "Expected srd to die of SIGTERM, got status $status."
[ -e $log.finished ] || fail "srd did not wait for the editor."
[ -z "$(ls -A $TMPDIR)" ] || fail "Temp files left after SIGTERM."

rm -rf $spy $log $log.* $TMPDIR
make clean-test
