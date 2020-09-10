## Speed up cycle detector reaping some actors

When an actor is done running, we now do a check similar to the check we do for when --ponynoblock is on:

- Check to see if the actor's message queue is empty
- Check to see if it has a reference count of 0

If both are true, then the actor can be deleted. When --ponynoblock is used, this is done immediately. It can't be done immediately when the cycle detector is in use because, it might have references to the actor in question.

In order to safely cleanup and reap actors, the cycle detector must be the one to do the reaping. This would eventually be done without this change. However, it is possible to write programs where the speed at which the cycle detectors protocol will operate is unable to keep up with the speed at which new "one shot" actors are created.

Here's an example of such a program:

```pony
actor Main
  new create(e: Env) =>
    Spawner.run()

actor Spawner
  var _living_children: U64 = 0

  new create() =>
    None

  be run() =>
    _living_children = _living_children + 1
    Spawnee(this).run()

  be collect() =>
    _living_children = _living_children - 1
    run()

actor Spawnee
  let _parent: Spawner

  new create(parent: Spawner) =>
    _parent = parent

  be run() =>
    _parent.collect()
```

Without this patch, that program will have large amount of memory growth and eventually, run out of memory. With the patch, Spawnee actors will be reaped much more quickly and stable memory growth can be achieved.

This is not a complete solution to the problem as issue #1007 is still a problem due to reasons I outlined at https://github.com/ponylang/ponyc/issues/1007#issuecomment-689826407.

It should also be noted that in the process of developing this patch, we discovered a use after free race condition in the runtime mpmcq implementation. That race condition can and will result in segfaults when running the above example program. There's no issue opened yet for that problem, but one will be coming.
