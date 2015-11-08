"""
An actor behaviour is intended for short lived finite interactions
executed asynchronously. Sometimes it is useful to be able to naturally
code behaviours of short lived finite signals punctuating over a
longer lived ( but finite ) behaviour. In actor implementations that
do not feature causal messaging this is fairly natural and idiomatic.
But in pony, without yield, this is impossible.

The causal messaging guarantee, and asynchronous execution means that
the messages enqueued in the actor's mailbox will never be scheduled
for execution if the receiving behaviour is infinite, which it can be
in the worst case ( bad code ).

By rediculo ad absurdum the simplest manifestation of this problem is
a signaling behaviour, say a 'kill' message, that sets a flag to conditionally
stop accepting messages. The runtime will only detect an actor as GCable if it
reaches quiescence *and* there are no pending messages waiting to be enqueued
to the actor in its mailbox. But, our 'kill' message can never proceed
from the mailbox as the currently active behaviour ( infinite ) never completes.

We call this the lonely pony problem. And, it can be solved in 1 line of pony.

Yield in pony is a simple clever trick. By transforming loops in long
running behaviours to yielding lazy tail-recursive behaviour calls composed,
we can yield conditionally whilst preserving causal messaging guarantees,
and additionally via consume, enforcing at-most-once delivery semantics.

Other interruptible behaviours can be implemented using variations of the
consume apply idiom on which it is based.  So if you're thinking in Erlang,
or some other actor implementation that does not feature causal messaging, you can still
write correct pony despite causal messaging through building on top
of yield or more generally the consume apply idiom and lazy tail-recursive
functional behaviour composition using lambda or object literals.

An example non-interruptible behaviour that exposes the lonely pony problem:

```pony
  actor LonelyPony
    var _alive : Bool = false
  be kill() =>
    Debug.out("Ugah!")
    _alive = false
  be forever() =>
    while _alive do
      Debug.out("Beep boop!") 
    end
  actor Main
    new stillborn(env : Env) =>
      LonelyPony.kill().forever() // Will never live, killed to soon!

    new immortal(env : Env) =>
      LonelyPony.forever().kill() // Will never die, can't kill!
```

And solving via yield:

```pony
actor InterruptiblePony
  var _alive : Bool = false
  be kill() =>
    _alive = false
  be _forever() =>
    match _alive
    | true => this.forever()
    | false => Debug.out("Ugah!")
    end
  be forever() =>
    this.yield(lambda iso()(wombat) => wombat._forever())
actor Main
  new mortal(env : Env) =>
    InterruptiblePony.forever().kill() // Once and only once kill
```

The benefits of causal messaging, garbage collectible actors, and safe mutable
actors far outweigh the small price manifested by the lonely pony problem. The
solution, that uncovered the consume apply idiom and its application to enable
interruptible behaviours that are easy to use are far more valuable at the cost to
the actor implementor of only a few extra lines of code per behaviour to enable
interruptible semantics with strong causal guarantees.
"""

interface iso Continuation
  """
  An apply continuation
  """
  fun iso apply()
  """
  Apply
  """

trait Yieldable
  """
  An interruptible trait
  """
  be yield(fn: Continuation) => (consume fn)() // Consume apply idiom
