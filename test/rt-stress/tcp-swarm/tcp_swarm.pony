"""
Swarm TCP stress engine.

A closed, count-driven TCP workload for stressing the runtime's
net stack. A fixed number of client connections is churned through
a listener at a bounded concurrency; each client sends a stamped
payload, the server echoes it, and the client verifies the echo
byte-for-byte before closing.

Every dimension of the swarm is a CLI flag set by the
orchestrator. See the README for a mapping from each flag to the
TCPConnection code path it exercises.
"""
