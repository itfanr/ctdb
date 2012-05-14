#!/bin/bash

test_info()
{
    cat <<EOF
Verify that IPs can be rearrranged using 'ctdb reloadips'.

Various sub-tests that remove addresses from the public_addresses file
on a node or delete the entire contents of the public_addresses file.

Prerequisites:

* An active CTDB cluster with at least 2 active nodes.

Expected results:

* When addresses are deconfigured "ctdb ip" no longer reports them and
  when added they are seen again.
EOF
}

. "${TEST_SCRIPTS_DIR}/integration.bash"

ctdb_test_init "$@"

set -e

ctdb_test_check_real_cluster

cluster_is_healthy

# Reset configuration
ctdb_restart_when_done

select_test_node_and_ips

echo "Emptying public addresses file on $test_node"

[ -n "$CTDB_BASE" ] || CTDB_BASE="/etc/ctdb"
addresses="${CTDB_BASE}/public_addresses"
backup="${addresses}.$$"

restore_public_addresses ()
{
    try_command_on_node $test_node "mv $backup $addresses >/dev/null 2>&1 || true"
}
ctdb_test_exit_hook_add restore_public_addresses

try_command_on_node $test_node "mv $addresses $backup && touch $addresses"

try_command_on_node any $CTDB reloadips -n all

echo "Getting list of public IPs on node $test_node"
try_command_on_node $test_node "$CTDB ip | tail -n +2"

if [ -n "$out" ] ; then
    cat <<EOF
BAD: node $test_node still has ips:
$out
EOF
    exit 1
fi

echo "GOOD: no IPs left on node $test_node"

echo "Restoring addresses"
restore_public_addresses

try_command_on_node any $CTDB reloadips -n all

echo "Getting list of public IPs on node $test_node"
try_command_on_node $test_node "$CTDB ip | tail -n +2"

if [ -z "$out" ] ; then
    echo "BAD: node $test_node has no ips"
    exit 1
fi

cat <<EOF
GOOD: node $test_node has these addresses:
$out
EOF

try_command_on_node any $CTDB sync

select_test_node_and_ips

first_ip=${test_node_ips%% *}
echo "Removing IP $first_ip from node $test_node"

try_command_on_node $test_node "mv $addresses $backup && grep -v '^${first_ip}/' $backup >$addresses"

try_command_on_node any $CTDB reloadips -n all

try_command_on_node $test_node $CTDB ip

if grep "^${first_ip} " <<<"$out" ; then
    cat <<EOF
BAD: node $test_node can still host IP $first_ip:
$out
EOF
    exit 1
fi

cat <<EOF
GOOD: node $test_node is no longer hosting IP $first_ip:
$out
EOF