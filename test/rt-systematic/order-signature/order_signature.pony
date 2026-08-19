"""
Reproducer fixture for the systematic-testing reproducibility
check (#5560).

Eight Sender actors each send 64 messages to a single Collector.
The arrival order is a pure function of the scheduler's
interleaving; the Collector folds (sender_id, seq) of every
arrival into an order-sensitive hash and prints it as ORDER_SIG.
Under systematic testing a seed-deterministic scheduler must print
the same ORDER_SIG for two runs at the same
`--ponysystematictestingseed`, and different seeds must be free to
produce different ones.
"""
