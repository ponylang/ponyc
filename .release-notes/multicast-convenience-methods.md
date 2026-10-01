## Add multicast convenience methods to UDPSocket

`UDPSocket` now has methods for common multicast operations: joining and leaving groups, setting TTL and hop limits, enabling loopback, and selecting the outgoing interface. Each method takes plain types (a string address, an integer) and returns 0 on success or a non-zero errno.

```pony
udp.join_multicast_group_v4("239.1.2.3", "127.0.0.1")
udp.set_multicast_loopback_v4(true)
udp.set_multicast_ttl(4)
```

Previously, multicast setup required packing C structs in network byte order and selecting the correct protocol-level constants by hand through `setsockopt`. The convenience methods handle struct layout and platform differences internally. IPv4 and IPv6 have separate methods because the underlying socket options take different parameter types.
