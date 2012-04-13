#!/bin/sh

. "${EVENTSCRIPTS_TESTS_DIR}/common.sh"

define_test "1 IP configured, takeip, releaseip, ipreallocated"

# This partly tests the test infrastructure.  If the (stub) "ctdb
# moveip" doesn't do anything then the IP being released will still be
# on the node and the ipreallocated event will add the routes back.

setup_ctdb
setup_ctdb_policy_routing

ctdb_get_1_public_address |
{
    read dev ip bits

    net=$(ipv4_host_addr_to_net "$ip" "$bits")
    gw="${net%.*}.1" # a dumb, calculated default

    cat >"$CTDB_PER_IP_ROUTING_CONF" <<EOF
$ip $net
$ip 0.0.0.0/0 $gw
EOF

    ok_null

    simple_test_event "takeip" $dev $ip $bits

    ok_null

    ctdb moveip $ip 1
    simple_test_event "releaseip" $dev $ip $bits

    ok_null

    # This will cause any
    simple_test_event "ipreallocated"

    ok <<EOF
# ip rule show
0:	from all lookup local 
32766:	from all lookup main 
32767:	from all lookup default 
EOF

    simple_test_command dump_routes
}