## Add prebuilt ponyc binaries for CentOS 8

Prebuilt nightly versions of ponyc are now available from our [Cloudsmith nightlies repo](https://cloudsmith.io/~ponylang/repos/nightlies/packages/). Release versions will be available in the [release repo](https://cloudsmith.io/~ponylang/repos/releases/packages/) starting with this release.

CentOS 8 releases were added at the request of Damon Kwok. If you are a Pony user and are looking for prebuilt releases for your distribution, open an issue and let us know. We'll do our best to add support for any Linux distribution version that is still supported by their creators.

## Fix building libs with Visual Studio 16.7

Visual Studio 16.7 introduced a change in C++20 compatibility that breaks building LLVM on Windows (#3634). We've added a patch for LLVM that fixes the issue.

## Add prebuilt ponyc binaries for Ubuntu 20.04

We've added prebuilt ponyc binaries specifically made to work on Ubuntu 20.04. At the time of this change, our generic glibc binaries are built on Ubuntu 20.04 and guaranteed to work on it. However, at some point in the future, the generic glibc version will change to another distro and break Ubuntu 20.04. With explicit Ubuntu 20.04 builds, they are guaranteed to work until we drop support for 20.04 when Canonical does the same.

You can opt into using the Ubuntu binaries when using ponyup by running:

```bash
ponyup default ubuntu20.04
```

## Fix missing Makefile lines to re-enable multiple `use=` options

When implementing the CMake build system, I missed copying a couple of lines from the old Makefile that meant that you could only specify one use= option when doing make configure on Posix. This change restores those lines, so you can specify multiple options for use, e.g. make configure use=memtrack_messages,address_sanitizer.

## Make Range.next partial

Range.next used to return the final index over and over when you reached the end. It was decided that this was a confusing and error prone API.

Range.next is now partial and will return error when you try to iterate past the end of the range.

Where you previously had code like:

```pony
let r = Range(0,5)
let a = r.next()
```

you now need (or similar based on your usage of Range.next

```pony
try
  let r = Range(0,5)
  let a = r.next()?
end
```

## Fix function calls in consume expressions

Function calls are not allowed in `consume` expressions. However, if the expression being `consume`d is a field access expression, whose LHS happens to be a function call,
the compiler fails after hitting an assertion.

For instance the following results in a compilation failure

```pony
    let a = "I am a string"
    let x:C iso! = consume a.chop(1)
```

but the following results in a compiler crash with an assertion failure

```pony
    let a = "I am a string"
    let x:C iso! = consume a.chop(1)._1
```

The fix ensures that both the scenarios are handled gracefully.

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

## Handling compiler crashes in some `consume` expressions

Previously  an expression of the form `consume (consume variable).field` resulted in the compiler crashing down with an assertion failure. With this fix, we get a friendly error message pointing to the erroneous line.

Likewise, a valid assignment of the form `(consume f).b =  recover Bar end` resulted in a compiler crash. With the fix, the compiler successfuly type checks the assignment and ensures that `f` is _consumed_.

## Fix soundness problem with Array.chop

Previously, the `chop` method on arrays could be used for any element type,
and return two isolated halves. While this is fine for sendable data,
`ref` elements could have shared references between elements which could
violate this isolation and make the method unsound. This change now
requires that the argument is sendable to call the method.

This is a breaking change. Because the array itself is isolated,
a usage that is genuinely sound can likely be fixed by ensuring
that the element type is `iso` instead of `ref` or `trn`, or `val`
instead of `box`, and recovering when elements are constructed.

## Allow for building on DragonFlyBSD again

When the ponyc build system was switched from make to cmake, the ability to build on DragonFlyBSD was inadvertenyly removed. The build system has been updated so that ponyc can be built again on DragonFly.

## Improvement to garbage collection for short-lived actors

We have a few example programs that create a large number of short-lived actors that can exhibit runaway memory growth. This update greatly reduces the memory growth or potentially reduce it to be stable.

The primary change here is to use some static analysis at compilation time to determine if an actor is "an orphan". That is, upon creation, has no actors (including it's creator) with references to it.

In order to support the concept of an "orphan actor", additional changes had to be made to the cycle detector protocol and how other actors interact with it. There are two primary things we can do with an orphan actor to improve the rate of garbage collection when the cycle detector is running.

When the cycle detector is running actors are collected by the cycle detector when they have no other actors referring to them and can't receive any new messages or are in a cycle that can't receive additional messages.

The examples listed in this PR all had problematic memory usage because, actors were created at a rate that overwhelmed the cycle detector and caused large amount of messages to it to back up and sit in its queue waiting to be processed.

This update addresses the "too many messages" problem by making the following actor GC rule changes for orphan actors. Note, all these rules are when the cycle detector is running.

* when an actor finishes being run by a scheduler thread "locally delete" it if:

  * no other actors hold a reference to the actor
  * the cycle detector isn't holding a reference to the actor
  * its queue is empty

  "locally deleting" follows the same message free protocol used when the cycle detector isn't running.

* when an actor finishes being run by a scheduler thread, preemptively inform the
  cycle detector that we are blocked if:

  * no other actors hold a reference to the actor
  * the cycle detector is holding a reference to the actor
  * its queue is empty

  by preemptively informing the cycle detector that we are blocked, the cycle detector can delete the actor more quickly. This "preemptively" notify change was introduced in [#3649](https://github.com/ponylang/ponyc/pull/3649).

In order to support these changes, additional cycle detector protocol changes were made. Previously, every actor upon creation informed the cycle detector of its existence. If we want to allow for local deleting, then this won't work. Every actor was known by the cycle detector. All concept of "actor created" messages have been removed by this change. Instead, actors have a flag FLAG_CD_CONTACTED that is set if the actor has ever sent a message to the cycle detector. Once the flag is set, we know that locally deleting the actor is unsafe and we can fall back on the slower preemptively inform strategy.

The cycle detector works by periodically sending messages to all actors it knows about and asking them if they are currently blocked as the first step in its "path to actor garbage collection" protocol. As actors no longer inform the cycle detector of their existence on creation, the cycle detector needs a new way to discover actors.

The first time an actor becomes logically blocked, it sets itself as blocked and notifies the cycle detector of its existence and that it is blocked. This is done by:

* setting the actor as blocked
* sending an "i'm blocked" message to the cycle detector
* setting FLAG_CD_CONTACTED on the actor

The overall effect of these changes is:

- The rate of garbage collection for short-lived actors is improved.
- Fewer cycle detector related messages are generated.

It should be noted that if an actor is incorrectly identified by the compiler as being an orphan when it is in fact not, the end result would be a segfault. During development of this feature, it was discovered that the following code from the Promise select test would segfault:

```pony
let pb = Promise[String] .> next[None]({(s) => h.complete_action(s) })
```

The issue was that .> wasn't being correctly handled by the `is_result_needed` logic in `expr.c`. It is possibly that other logic within the function is faulty and if segfaults are see after this change that didn't exist prior to it, then incorrectly identifying an actor as an orphan might be the culprit.

Prior to these change, examples such as the one from issue [#007](https://github.com/ponylang/ponyc/issues/1007) (listed later) would be unable to be collected due to an edge-case in the cycle detector and the runtime garbage collection algorithms.

Issue [#007](https://github.com/ponylang/ponyc/issues/1007) was opened with the following code having explosive memory growth:

```pony
primitive N fun apply(): U64 => 2_000_000_000

actor Test
  new create(n: U64) =>
    if n == 0 then return end
    Test(n - 1)

actor Main
  new create(env: Env) =>
    Test(N())
```

A previous PR [#3649](https://github.com/ponylang/ponyc/pull/3649) partially addressed some memory growth issues by adding the "preemptively informing" that detailed earlier. The example above from [#007](https://github.com/ponylang/ponyc/issues/1007) wasn't addressed by [#3649](https://github.com/ponylang/ponyc/pull/3649) because the key to [#3649](https://github.com/ponylang/ponyc/pull/3649) was that when done running its behaviors, an actor can see if it has no additional messages AND no references to itself and can then tell the cycle detector to skip parts of the CD protocol and garbage collect sooner. [#007](https://github.com/ponylang/ponyc/issues/1007) requires the "local delete" functionality from this change whereas [#3649](https://github.com/ponylang/ponyc/pull/3649) only provided "pre-emptive informing".

The addition of "local delete" allows for the cycle detector to keep up with many cases of generating large amounts of "orphaned" actors. The from [#007](https://github.com/ponylang/ponyc/issues/1007) above wasn't addressed by [#3649](https://github.com/ponylang/ponyc/pull/3649) because of an implementation detail in the ORCA garbage collection protocol; at the time that an instance of Test is done running its create behavior, it doesn't have a reference count of 0. It has a reference count of 256. Not because there are 256 references but because, when an actor is created puts a "fake value" in the rc value such that an actor isn't garbage collected prematurely. The value will be set to the correct value once the actor that created it is first traced and will be subsequently updated correctly per ORCA going forward.

However, at the time that an instance of Test is finished running its create, that information isn't available. It would be incorrect to say "if rc is 256, I'm blocked and you can gc me". 256 is a perfectly reasonable value for an rc to be in normal usage.

This isn't a problem with the changes in this PR as the compiler detects that each instance of Test will be an orphan and immediately sets its rc to 0. This allows it to be garbage collected as the instance's message queue is empty so long as it's rc remains 0.

Any changes in the future to address lingering issues with creating large numbers of orphaned actors should also be tested with the following examples. Each exercises a different type of pattern that could lead to memory growth or incorrect execution.

Example 2 features reasonably stable memory usage that I have seen from time-to-time, increase rapidly. It should be noted that such an increase is rather infrequent but suggests there are additional problems in the cycle-detector. I suspect said problem is a periodic burst of additional messages to the cycle-detector from actors that can be garbage collected, but I haven't investigated further.

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

Example 3 has stable memory growth and given that it won't result in any messages being sent to the cycle detector as we have determined at compile-time that the Foo actor instances are orphaned.

```pony
actor Main
  var n: U64 = 2_000_000_000

  new create(e: Env) =>
    run()

  be run() =>
    while(n > 0 ) do
      Foo(n)
      n = n - 1
      if ((n % 1_000) == 0) then
        run()
        break
      end
     end

actor Foo
  new create(n: U64) =>
    if ((n % 1_000_000) == 0) then
      @printf[I32]("%ld\n".cstring(), n)
    end

    None
```

Example 4 has the same characteristics as example 3 with the code as of this change. However, it did exhibit different behavior prior to this change being fully completed and appears to be a good test candidate for any future changes.

```pony
actor Main
  var n: U64 = 2_000_000_000

  new create(e: Env) =>
    run()

  be run() =>
    while(n > 0 ) do
      Foo(n)
      n = n - 1
      if ((n % 1_000_000) == 0) then
        @printf[I32]("%ld\n".cstring(), n)
      end
      if ((n % 1_000) == 0) then
        run()
        break
      end
     end

actor Foo
  new create(n: U64) =>
    None
```

Finally, for anyone who works on improving this in the future, here's an additional test program beyond ones that already exist elsewhere for testing pony programs. This program will create a large number of actors that are orphaned but then send themselves to another actor. This should increase their rc count and keep them from being garbage collected. If the program segfaults, then something has gone wrong and the logic related to orphan actors has been broken. The example currently passes as of this change.

```pony
use "collections"

actor Holder
  let _holding: SetIs[ManyOfMe] = _holding.create()

  new create() =>
    None

  be hold(a: ManyOfMe) =>
    _holding.set(a)

  be ping() =>
    var c: U64 = 1_000
    for h in _holding.values() do
      if c > 0 then
        h.ping()
        c = c - 1
      end
    end

actor ManyOfMe
  new create(h: Holder) =>
    h.hold(this)

  be ping() =>
    None

actor Main
  var n: U64 = 500_000
  let holder: Holder

  new create(env: Env) =>
    holder = Holder
    run()

  be run() =>
    while(n > 0 ) do
      ManyOfMe(holder)
      n = n - 1
      if ((n % 1_000) == 0) then
        @printf[I32]("%ld\n".cstring(), n)
        run()
        holder.ping()
        break
      end
     end
```

