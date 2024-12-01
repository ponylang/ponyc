"""
# Actor Pinning Package

The Actor Pinning package allows Pony programmers to pin actors to a dedicated 
scheduler thread. This can be required/used for interfacing with C libraries
that rely on thread local storage. A common example of this is graphics/windowing
libraries.

The way it works is that an actor can request that it be pinned (which may or
may not happen immediately) and then it must wait and check to confirm that the
pinning was successfully applied (prior to running any workload that required the
actor to be pinned) after which all subsequent behaviors on that actor will run
on the same scheduler thread until the actor is destroyed or the actor requests
to be unpinned.

## Example program

```pony
// Here we have the Main actor that upon construction requests a PinUnpinActorAuth
// token from AmbientAuth and then requests that it be pinned. It then recursively
// calls the `check_pinned` behavior until the runtime reports that it has
// successfully been pinned after which it starts `do_stuff` to do whatever
// work it needs to do that requires it to be pinned. Once it has completed all
// of its work, it calls `done` to request that the runtime `unpin` it.

use "actor_pinning"

actor Main
  let _env: Env
  let _auth: PinUnpinActorAuth

  new create(env: Env) =>
    _env = env
    _auth = PinUnpinActorAuth(env.root)
    ActorPinning.request_pin(_auth)
    check_pinned()

  be check_pinned() =>
    if ActorPinning.is_successfully_pinned(_auth) then
      // do stuff that requires this actor to be pinned
      do_stuff(10)
    else
      check_pinned()
    end

  be do_stuff(i: I32) =>
    if i < 0 then
      done()
    else
      do_stuff(i - 1)
    end

  be done() =>
    ActorPinning.request_unpin(_auth)
```

## Caveat

Actor pinning is an experimental feature; use with caution.

The current implementation of pinning actors can interact in surprising ways with program
shutdown. If there are any live pinned actors in the program, it will continue to run even if
there is no additional work to do.

Actors that are garbage collected will be unpinned. This includes actors that are garbage
collected by the cycle detector.

At this time, using pinned actors is an advanced feature that requires knowledge of runtime to
use safely. We strongly suggest that as part of your program design, that you make sure that
you have a process for unpinning any actors that you have pinned. Designing actor unpinning
into your program will avoid any issues with program shutdown.

As we further implement actor pinning, we hope to remove this caveat.

Additionally, due to the fact
that Pony uses cooperative scheduling of actors and that all pinned actors run
on a single shared scheduler thread, any "greedy" actors that monopolize the
cpu (with long running behaviors) will negatively inmpact all other pinned
actors by starving them of cpu.
"""

use @pony_actor_set_pinned[None]()
use @pony_actor_unset_pinned[None]()
use @pony_scheduler_index[I32]()

primitive ActorPinning
  fun request_pin(auth: PinUnpinActorAuth) =>
    @pony_actor_set_pinned()

  fun request_unpin(auth: PinUnpinActorAuth) =>
    @pony_actor_unset_pinned()

  fun is_successfully_pinned(auth: PinUnpinActorAuth): Bool =>
    let sched: I32 = @pony_scheduler_index()

    // the `-999` constant is the same value as `PONY_PINNED_ACTOR_THREAD_INDEX`
    // defined in `scheduler.h` in the runtime
    sched == -999
