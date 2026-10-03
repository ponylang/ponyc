## Fix socket option calls failing with WSAEINVAL on Windows

On Windows, `setsockopt` and `getsockopt` calls that specified a protocol level (`IPPROTO_TCP`, `IPPROTO_IPV6`, `IPPROTO_UDP`, etc.) or certain option constants (`IP_PMTUDISC_DO`, `MCAST_INCLUDE`, `MCAST_EXCLUDE`) failed with WSAEINVAL (10022). Affected operations include `set_nodelay`, `set_multicast_hops`, and other socket option methods in the `net` package. These calls now work correctly.
