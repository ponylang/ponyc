## Add Fedora 41 as a supported platform

We've added Fedora 41 as a supported platform. We'll be building ponyc releases for it until it stops receiving security updates in November 2025. At that point, we'll stop building releases for it.

## Drop Fedora 39 support

Fedora 39 has reached its end of life date. We've dropped it as a supported platform. That means, we no longer create prebuilt binaries for installation via `ponyup` for Fedora 39.

We will maintain best effort to keep Fedora 39 continuing to work for anyone who wants to use it and builds `ponyc` from source.

## Update Pony musl Docker images to Alpine 3.20

We've updated our `ponylang/ponyc:latest-alpine`, `ponylang/ponyc:release-alpine`, and `ponylang/ponyc:x.y.z-alpine` images to be based on Alpine 3.20. Previously, we were using Alpine 3.18 as the base.
## Fix rare termination logic failures that could result in early shutdown

There was a very rare edge case in the termination logic that could result in early shutdown resulting in a segfault.

The edge cases have been addressed and the shutdown/termination logic has been overhauled to make it simpler and more robust.

##  Add support for pinning actors to a dedicated scheduler thread

Pony programmers can now pin actors to a dedicated  scheduler thread. This can be required/used for interfacing with C libraries that rely on thread local storage. A common example of this is graphics/windowing libraries.

The way it works is that an actor can request that it be pinned (which may or may not happen immediately) and then it must wait and check to confirm that the pinning was successfully applied (prior to running any workload that required the actor to be pinned) after which all subsequent behaviors on that actor will run on the same scheduler thread until the actor is destroyed or the actor requests to be unpinned.

### Caveat

Due to the fact that Pony uses cooperative scheduling of actors and that all pinned actors run on a single shared scheduler thread, any "greedy" actors that monopolize the cpu (with long running behaviors) will negatively inmpact all other pinned actors by starving them of cpu.

### Example program

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

