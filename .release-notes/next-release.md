## Allow constructor expressions to be auto-recovered in assignments or arguments

Previously for the following code:

```pony
actor Main
  new create(env: Env) =>
    Bar.take_foo(Foo(88))
    let bar: Foo iso = Foo(88)

class Foo
  new create(v: U8) =>
    None

primitive Bar
  fun take_foo(foo: Foo iso) =>
    None
```

You'd get compilation errors "argument not assignable to parameter" and "right side must be a subtype of left side" for the two lines in `Main.create()`.

We've added checks to see if the constructor expressions can be implicitly auto-recovered, and if they can, no compilation error is generated.

This only applies to cases where the type of the parameter (or the `let` binding) is a simple type, i.e. not a union, intersection or tuple type.

## Support for RISC-V

Pony now supports compiling for RISC-V linux glibc systems. 64 bit `rv64gc`/`lp64d` is tested via CI. In theory 32 bit `rv32gc`/`ilp32d` might work but is untested currently.

## Enhance runtime stats tracking

The current pony runtime stats tracking that was previously
implemented under the `USE_MEMTRACK` and `USED_MEMTRACK_MESSAGES`
defines has been enhanced. The new defines are called
`USE_RUNTIMESTATS` and `USE_RUNTIMESTATS_MESSAGES`.

Runtime stats tracking tracks the following actor info:
* heap memory allocated
* heap memory used
* heap num allocated
* heap realloc counter
* heap alloc counter
* heap free counter
* heap gc counter
* system message processing cpu usage
* app message processing cpu usage
* garbage collection cpu usage
* messages sent counter
* system messages processed counter
* app messages processed counter

Runtime tracking tracks the following scheduler info:
* mutemap memory used
* mutemap memory allocated
* memory used for gc acquire/release actormaps and actors created
* memory allocated for gc acquire/release actormaps and actors created
* created actors counter
* destroyed actors counter
* actor system message processing cpu for all actor runs on the scheduler
* actor app message processing cpu for all actor runs on the scheduler
* actor garbage collection cpu for all actor runs on the scheduler
* scheduler message processing cpu usage
* scheduler misc cpu usage while waiting to do work
* memory used by inflight messages
* memory allocated by inflight messages
* number of inflight messages

There is also a new command line argument available to Pony programs
called `--ponyprintstatsinterval` that will print out scheduler statistics
every `--ponyprintstatsinterval` seconds along with printing out actor
statistics whenever an actor is destroyed. The output looks something like:

```
$ ./helloworld --ponyprintstatsinterval 1
Hello, world.
Actor stats for actor: 140661474674688, heap memory allocated: 0, heap memory used: 0, heap num allocated: 0, heap realloc counter: 0, heap alloc counter: 0, heap free counter: 0, heap gc counter: 0, system cpu: 2628, app cpu: 964, garbage collection marking cpu: 0, garbage collection sweeping cpu: 0, messages sent counter: 3, system messages processed counter: 1, app messages processed counter: 1
Actor stats for actor: 140661474675200, heap memory allocated: 0, heap memory used: 0, heap num allocated: 0, heap realloc counter: 0, heap alloc counter: 0, heap free counter: 0, heap gc counter: 0, system cpu: 1451, app cpu: 170026, garbage collection marking cpu: 0, garbage collection sweeping cpu: 0, messages sent counter: 0, system messages processed counter: 1, app messages processed counter: 2
Actor stats for actor: 140661474848256, heap memory allocated: 9664, heap memory used: 7408, heap num allocated: 63, heap realloc counter: 0, heap alloc counter: 63, heap free counter: 0, heap gc counter: 0, system cpu: 0, app cpu: 5561, garbage collection marking cpu: 0, garbage collection sweeping cpu: 0, messages sent counter: 8, system messages processed counter: 0, app messages processed counter: 1
Actor stats for actor: 140661474672640, heap memory allocated: 0, heap memory used: 0, heap num allocated: 0, heap realloc counter: 0, heap alloc counter: 0, heap free counter: 0, heap gc counter: 0, system cpu: 1820, app cpu: 103, garbage collection marking cpu: 0, garbage collection sweeping cpu: 0, messages sent counter: 3, system messages processed counter: 1, app messages processed counter: 1
Scheduler stats for index: 1, total memory allocated: -232, total memory used: -320, created actors counter: 0, destroyed actors counter: 1, actors app cpu: 0, actors gc marking cpu: 0, actors gc sweeping cpu: 0, actors system cpu: 2668, scheduler msgs cpu: 203532, scheduler misc cpu: 1871418, memory used inflight messages: 0, memory allocated inflight messages: 0, number of inflight messages: 0
Scheduler stats for index: 0, total memory allocated: -464, total memory used: -576, created actors counter: 0, destroyed actors counter: 1, actors app cpu: 176654, actors gc marking cpu: 0, actors gc sweeping cpu: 0, actors system cpu: 10118, scheduler msgs cpu: 172241, scheduler misc cpu: 1839211, memory used inflight messages: 0, memory allocated inflight messages: 0, number of inflight messages: 0
Scheduler stats for index: 3, total memory allocated: -56, total memory used: -64, created actors counter: 0, destroyed actors counter: 0, actors app cpu: 0, actors gc marking cpu: 0, actors gc sweeping cpu: 0, actors system cpu: 1820, scheduler msgs cpu: 150303, scheduler misc cpu: 1774791, memory used inflight messages: 0, memory allocated inflight messages: 0, number of inflight messages: 0
Scheduler stats for index: 2, total memory allocated: -896, total memory used: -1088, created actors counter: 0, destroyed actors counter: 2, actors app cpu: 0, actors gc marking cpu: 0, actors gc sweeping cpu: 0, actors system cpu: 583713, scheduler msgs cpu: 142592, scheduler misc cpu: 1286404, memory used inflight messages: 0, memory allocated inflight messages: 0, number of inflight messages: 0
Actor stats for actor: 140661474841600, heap memory allocated: 0, heap memory used: 0, heap num allocated: 0, heap realloc counter: 0, heap alloc counter: 0, heap free counter: 0, heap gc counter: 0, system cpu: 592420, app cpu: 0, garbage collection marking cpu: 0, garbage collection sweeping cpu: 0, messages sent counter: 0, system messages processed counter: 7, app messages processed counter: 0
```

This runtime stats tracking info has been exposed to pony programs as
part of the `runtime_info` package and an example `runtime_info` program
has been added to the `examples` directory.

The runtime stats tracking in a pony program can be used for some
useful validations for those folks concerned about
heap allocations in the critical path (i.e. if they
rely on the compiler's `HeapToStack` optimization pass
to convert heap allocations to stack allocations and
want to validate it is working correctly).

Example of possible use to validate number of heap allocations:

```pony
use "collections"
use "runtime_info"

actor Main
  new create(env: Env) =>
    let num_allocs_before = ActorStats.heap_alloc_counter(ActorStatsAuth(env.root))

    let ret = critical()

    let num_allocs_after = ActorStats.heap_alloc_counter(ActorStatsAuth(env.root))

    env.out.print("Allocations before: " + num_allocs_before.string())
    env.out.print("Allocations after: " + num_allocs_after.string())

    env.out.print("Critical section allocated " + (num_allocs_after - num_allocs_before).string() + " heap objects")

    env.out.print("Allocations at end: " + ActorStats.heap_alloc_counter(ActorStatsAuth(env.root)).string())

  fun critical(): U32 =>
    var x: U32 = 1
    let y: U32 = 1000

    for i in Range[U32](1, y) do
      x = x * y
    end

    x
```

