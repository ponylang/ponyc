## Fix delivery of zero-byte UDP datagrams

UDP sockets now deliver zero-byte datagrams to `UDPNotify.received` with an empty payload instead of treating them as an error and tearing the socket down. A zero-length datagram is valid per RFC 768, and applications that use them as heartbeats, keepalives, or presence pings will now receive them like any other datagram. TCP zero-byte read handling, where a zero-byte read means the peer closed, is unchanged.
