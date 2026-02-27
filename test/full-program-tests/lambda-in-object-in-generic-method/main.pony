"""
Regression test for a bug where a lambda inside an object literal inside a
generic method had its type parameters not properly reified during subtype
checking. This caused the lambda's apply method to be missing from its vtable,
resulting in a segfault at runtime when iterating shrink values from a mapped
pony_check Generator.

The root cause was that collect_type_params() creates copies of type parameters
at each nesting level, but reify_typeparamref() only followed the ast_data
chain one level instead of to the root.
"""
use @pony_exitcode[None](code: I32)
use "pony_check"

actor Main
  new create(env: Env) =>
    let gen = recover val
      Generators.u32().map[U32]({(n: U32): U32^ => n + 1})
    end
    let rnd = Randomness(42)
    try
      match gen.generate(rnd)?
      | (let v: U32, let shrinks: Iterator[U32^]) =>
        if v == 0 then error end
        // Iterating shrink values is where the segfault occurred,
        // because the lambda's apply method was missing from the vtable.
        var count: USize = 0
        while shrinks.has_next() and (count < 5) do
          shrinks.next()?
          count = count + 1
        end
        if count == 0 then error end
        @pony_exitcode(1)
      | let v: U32 =>
        @pony_exitcode(1)
      end
    end
