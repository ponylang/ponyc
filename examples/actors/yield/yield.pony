"""
An actor behaviour is intended for short lived finite interactions executed
asynchronously. Sometimes it is useful to be able to naturally code behaviours
of short lived finite signals punctuating over a longer lived (but finite)
behaviour. In actor implementations that do not feature causal messaging this is
fairly natural and idiomatic. But in pony, without yield, this is impossible.

The causal messaging guarantee, and asynchronous execution means that the
messages enqueued in the actor's mailbox will never be scheduled for execution
if the receiving behaviour is infinite, which it can be in the worst case (bad
code).

By rediculo ad absurdum the simplest manifestation of this problem is a
signaling behaviour, say a 'kill' message, that sets a flag to conditionally
stop accepting messages. The runtime will only detect an actor as GCable if it
reaches quiescence *and* there are no pending messages waiting to be enqueued to
the actor in its mailbox. But, our 'kill' message can never proceed from the
mailbox as the currently active behaviour (infinite) never completes.

We call this the lonely pony problem. And, it can be solved in 0 lines of pony.

Yield in pony is a simple clever trick. By transforming loops in long running
behaviours to lazy tail-recursive behaviour calls composed, we can yield
conditionally whilst preserving causal messaging guarantees, and enforcing at-
most-once delivery semantics.

The benefits of causal messaging, garbage collectible actors, and safe mutable
actors far outweigh the small price manifested by the lonely pony problem. The
solution, that uncovered the consume apply idiom and its application to enable
interruptible behaviours that are easy to use are far more valuable at the cost
to the actor implementor of only a few extra lines of code per behaviour to
enable interruptible semantics with strong causal guarantees.

In a nutshell, by avoiding for and while loops, and writing behaviours tail
recursively, the ability to compose long-lived with short-lived behaviours is a
builtin feature of pony.
"""
