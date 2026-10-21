#!/bin/bash

# Test that concurrent changes don't lose each other.  Each srd
# reloads the root, changes it and writes it back; without a lock
# around all of that, one process can overwrite another's change.

pass=$(date +%s.%N)
echo setting pass=$pass
export EDITOR=./test_editor.sh
n=30

fail() {
    echo "$@"
    exit 1
}

./srd -T $pass --create -m '' > /dev/null

for i in $(seq $n); do
    printf '[k%02d]\nv\n' $i | ./srd -T $pass -e > /dev/null &
done
wait

count=$(./srd -T $pass -k '' | wc -l)
[ $n = $count ] || fail "Concurrent adds lost records:  $count of $n remain."
./srd -T $pass --validate > /dev/null || fail "Validation failed after concurrent adds."

# Concurrent deletes of different records, likewise.
for i in $(seq 2 2 $n); do
    ./srd -T $pass -x $(printf 'k%02d' $i) > /dev/null &
done
wait

count=$(./srd -T $pass -k '' | wc -l)
[ $((n / 2)) = $count ] ||
    fail "Concurrent deletes went wrong:  $count of $((n / 2)) remain."
./srd -T $pass --validate > /dev/null || fail "Validation failed after concurrent deletes."

# Exactly one of two concurrent creators may establish a database.
create_pass=${pass}-create
./srd -T "$create_pass" --create -m '' > /dev/null 2>&1 &
create_one=$!
./srd -T "$create_pass" --create -m '' > /dev/null 2>&1 &
create_two=$!
status_one=0
status_two=0
wait "$create_one" || status_one=$?
wait "$create_two" || status_two=$?
[ $(((status_one == 0) + (status_two == 0))) -eq 1 ] ||
    fail "Concurrent create statuses were $status_one and $status_two."
./srd -T "$create_pass" --validate > /dev/null ||
    fail "Database is invalid after concurrent create."

# A process that loaded a database before its password changed must
# not recreate the removed old-password root when it eventually saves.
stale_pass=${pass}-stale
stale_editor=test_${pass}-stale-editor
stale_ready=test_${pass}-stale-ready
stale_release=test_${pass}-stale-release
stale_log=test_${pass}-stale-log

cat > "$stale_editor" <<'EOF'
#!/bin/bash
touch "$STALE_READY"
while [ ! -e "$STALE_RELEASE" ]; do sleep 0.05; done
cat > "$1"
EOF
chmod +x "$stale_editor"

printf '[old]\nold value\n' | ./srd -T "$stale_pass" --create -e > /dev/null
printf '[new]\nnew value\n' |
    STALE_READY="$PWD/$stale_ready" STALE_RELEASE="$PWD/$stale_release" \
    EDITOR="$PWD/$stale_editor" ./srd -T "$stale_pass" -e \
    > "$stale_log" 2>&1 &
stale_pid=$!
for i in $(seq 100); do
    [ -e "$stale_ready" ] && break
    sleep 0.05
done
[ -e "$stale_ready" ] || fail "Stale writer's editor never started."

./srd -T "$stale_pass" --passwd > /dev/null ||
    fail "Password change failed during stale-writer test."
touch "$stale_release"
if wait "$stale_pid"; then
    fail "Stale writer recreated the old-password database."
fi
grep -q 'Refusing to recreate' "$stale_log" ||
    fail "Stale writer failed for the wrong reason:  $(cat "$stale_log")"

if ./srd -T "$stale_pass" --validate > /dev/null 2>&1; then
    fail "Old password still opens a database after password change."
fi
./srd -T "${stale_pass}2" --validate > /dev/null ||
    fail "New-password database is invalid after rejecting stale writer."
keys=$(./srd -T "${stale_pass}2" -k '')
[ "$keys" = '[old]' ] ||
    fail "Stale writer changed the new-password database:  $keys"
[ -z "$(find "srd-test-0000-$LOGNAME" -name '*.new' -print -quit)" ] ||
    fail "Stale writer left a .new file behind."

make clean-test
