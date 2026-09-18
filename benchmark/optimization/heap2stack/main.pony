"""
Stress test for HeapToStack promotion of composite objects.

Creates a nested pair of objects (Triple containing Pair) in a tight loop.
When HeapToStack works, both allocations are promoted to the stack and the
loop runs in ~0.2s. When promotion fails, 400M heap allocations produce
several seconds of allocation and GC overhead.

The Pair-stored-into-Triple pattern exercises the store-to-alloca promotion
path: HeapToStack must first promote Triple, then recognize that Pair's only
escape is into the (now stack-allocated) Triple.
"""

class Pair
  let a: U64
  let b: U64
  new create(a': U64, b': U64) => a = a'; b = b'
  fun sum(): U64 => a + b

class Triple
  let p: Pair
  let c: U64

  new create(a: U64, b: U64, c': U64) =>
    p = Pair(a, b)
    c = c'

  fun total(): U64 => p.sum() + c

actor Main
  new create(env: Env) =>
    var sum: U64 = 0
    var i: U64 = 0
    while i < 200_000_000 do
      sum = sum + Triple(i, i + 1, i + 2).total()
      i = i + 1
    end
    env.out.print(sum.string())
