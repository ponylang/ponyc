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

