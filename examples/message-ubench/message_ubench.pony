"""
A microbenchmark for measuring message passing rates in the Pony runtime.

This microbenchmark executes a sequence of intervals.  During an interval,
1 second long by default, the SyncLeader actor sends an initial
set of ping messages to a static set of Pinger actors.  When a Pinger
actor receives a ping() message, the Pinger will randomly choose
another Pinger to forward the ping() message.  This technique limits
the total number of messages "in flight" in the runtime to avoid
causing unnecessary memory consumption & overhead by the Pony runtime.

This small program has several intended uses:

* Demonstrate use of three types of actors in a Pony program: a timer,
  a SyncLeader, and many Pinger actors.

* As a stress test for Pony runtime development, for example, finding
  deadlocks caused by experiments in the "Generalized runtime
  backpressure" work in pull request
  https://github.com/ponylang/ponyc/pull/2264

* As a stress test for measuring message send & receive overhead for
  experiments in the "Add DTrace probes for all message push and pop
  operations" work in pull request
  https://github.com/ponylang/ponyc/pull/2295
"""
