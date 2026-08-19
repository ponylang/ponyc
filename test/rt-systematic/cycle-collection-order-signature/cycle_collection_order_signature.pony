"""
Reproducer fixture for the systematic-testing reproducibility
check (#5560), covering the cycle detector's pointer-ordered
message sends and its cycle reclamation (#5569).

Successive groups of Worker actors form strongly connected
reference cycles that only the cycle detector can reclaim. Within
each group the Workers forward ping chains to random neighbors,
and the terminal hop of every chain reports to a Collector whose
order-sensitive ORDER_SIG captures the interleaving. The cycle
detector probes, confirms, and reclaims the groups while the
Workers are active, so the pointer-ordered sends the detector
makes are folded into the signature.

#5569 orders all of the detector's sends by a stable
creation-order id instead of the pointer-hash iteration order, so
a fixed `--ponysystematictestingseed` replays one interleaving
regardless of address layout.
"""
