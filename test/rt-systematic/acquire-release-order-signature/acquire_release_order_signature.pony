"""
Reproducer fixture for the systematic-testing reproducibility
check (#5560), covering the ORCA reference-counting send ordering
(#5568).

A mesh of Node actors forwards tokens on a spreading route
`(id + hops) % nodes`. Each hop carries a freshly allocated
`String val` payload owned by the forwarding node, so a receiving
node accumulates references to payloads owned by many distinct
nodes in its foreign GC map. When that map is swept, the runtime
sends one `ACTORMSG_RELEASE` per surviving distinct owner;
forwarding likewise sends one `ACTORMSG_ACQUIRE` per newly
referenced owner. #5568 orders these sends by a stable
creation-order id instead of the GC map's pointer-hash iteration
order, so a fixed `--ponysystematictestingseed` replays one
interleaving regardless of address layout.
"""
