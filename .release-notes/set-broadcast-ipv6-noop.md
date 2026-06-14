## Make `UDPSocket.set_broadcast` a no-op on IPv6 sockets

`UDPSocket.set_broadcast` promises to enable or disable broadcasting from a socket. On IPv4 sockets it does: it sets the `SO_BROADCAST` socket option to the value you pass. On IPv6 sockets it instead ignored its argument and joined the FF02::1 all-nodes multicast group — `set_broadcast(false)` joined the group too, and the group was never left.

IPv6 has no broadcast, and sending to a multicast address such as the all-nodes group (`DNS.broadcast_ip6`) requires no permission, so there is nothing for `set_broadcast` to enable on an IPv6 socket. It is now a documented no-op on IPv6 sockets. The removed join had no observable effect in practice — every IPv6 node is automatically a member of the all-nodes group — so existing programs should see no change in behavior. To receive traffic for a multicast group, use `multicast_join`.
