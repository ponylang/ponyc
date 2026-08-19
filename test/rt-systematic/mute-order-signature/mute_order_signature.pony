"""
Reproducer fixture for the systematic-testing reproducibility
check (#5560), covering the actor muting / unmuting scheduling
path.

A ring of 48 Node actors each forwards a token to the next node a
fixed number of times (deterministic `(id + 1) % n` routing), then
reports the token's arrival to a single Collector. The Collector
folds (token_id, node_id) of every arrival into an order-sensitive
hash printed as ORDER_SIG, a pure function of the scheduler
interleaving.

Unlike order-signature (whose senders only self-send and fan into
the collector, so no actor ever overloads another), the volume of
foreign actor-to-actor sends here drives backpressure: nodes
overload, their senders get muted, and are later rescheduled when
the overloaded node drains. That unmute-rescheduling step is a
distinct part of the scheduler that order-signature does not
exercise, so this fixture guards a different slice of the "fixed
seed replays one interleaving" property.
"""
