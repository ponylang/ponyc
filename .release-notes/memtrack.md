## Enhance runtime memory allocation tracking

Runtime allocation tracking now also tracks the number
of heap allocations, the number of freed heap
allocations and the number of GC iterations via counters.

Additionally, there is now a way to check if runtime
memory allocation tracking is enabled or not via
`ifdef` statements in Pony code. This allows for some
useful validations for those folks concerned about
heap allocations in the critical path (i.e. if they
rely on the compiler's `HeapToStack` optimization pass
to convert heap allocations to stack allocations and
want to validate it is working correctly).

Example of possible use to validate number of heap allocations:

```pony
use @ponyint_actor_heap[Pointer[Heap]](a: Any tag) if memtrack
use @ponyint_heap_alloc_counter[U32](h: Pointer[Heap]) if memtrack
use "collections"


primitive Heap

actor Main
  new create(env: Env) =>
    let num_allocs_before =
      ifdef memtrack then @ponyint_heap_alloc_counter(@ponyint_actor_heap(this))
      else 0
      end

    let ret = critical()

    let num_allocs_after =
      ifdef memtrack then @ponyint_heap_alloc_counter(@ponyint_actor_heap(this))
      else 0
      end

    env.out.print("Allocations before: " + num_allocs_before.string())
    env.out.print("Allocations after: " + num_allocs_after.string())

    env.out.print("Critical section allocated " + (num_allocs_after - num_allocs_before).string() + " heap objects")

    env.out.print("Allocations at end: " + (ifdef memtrack then @ponyint_heap_alloc_counter(@ponyint_actor_heap(this)) else 0 end).string())

  fun critical(): U32 =>
    var x: U32 = 1
    let y: U32 = 1000

    for i in Range[U32](1, y) do
      x = x * y
    end

    x
```
