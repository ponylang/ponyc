"""
Inlining depth before HeapToStack.

Allocates a Wrapper object through a 3-level call chain of primitive methods.
The inliner must collapse Main -> Level1 -> Level2 -> Level3 -> Wrapper.create
before HeapToStack can see that the Wrapper doesn't escape.

Level2 calls Level3 twice with different arguments, producing 2 allocations per
iteration (200M total). If inlining is deep enough and HeapToStack fires, the
loop runs fast. If either pass falls short, heap allocation overhead dominates.
"""

class Wrapper
  let value: U64
  new create(v: U64) => value = v
  fun get(): U64 => value

primitive Level3
  fun apply(x: U64): U64 =>
    Wrapper(x).get()

primitive Level2
  fun apply(x: U64): U64 =>
    Level3(x * 2) + Level3(x + 1)

primitive Level1
  fun apply(x: U64): U64 =>
    Level2(x)

actor Main
  new create(env: Env) =>
    var sum: U64 = 0
    var i: U64 = 0
    while i < 100_000_000 do
      sum = sum + Level1(i)
      i = i + 1
    end
    env.out.print(sum.string())
