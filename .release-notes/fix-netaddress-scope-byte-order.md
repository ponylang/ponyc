## Fix NetAddress.scope() returning a byte-swapped value on little-endian platforms

`NetAddress.scope()` returned the IPv6 scope zone identifier -- the interface index of a scoped address, such as the `eth0` in `fe80::1%eth0` -- with its bytes reversed on little-endian platforms. An address scoped to interface index 2, for instance, returned 33554432 (0x02000000) instead of 2, making the value useless for identifying the interface.

`scope()` now returns the zone identifier correctly. Big-endian platforms were unaffected and are unchanged.
