"""
Regression test for type parameter reification in nested lambdas where the
mapping changes the type (U32 -> String). This ensures that both the source
type T and the target type U are correctly reified through multiple levels
of type parameter copying.

See also: lambda-in-object-in-generic-method
"""
use @pony_exitcode[None](code: I32)
use "pony_check"

actor Main
  new create(env: Env) =>
    // Map U32 to String - T and U are different types
    let gen = recover val
      Generators.u32().map[String]({(n: U32): String^ => n.string()})
    end
    let rnd = Randomness(42)
    try
      match gen.generate(rnd)?
      | (let v: String, let shrinks: Iterator[String^]) =>
        if v.size() == 0 then error end
        var count: USize = 0
        while shrinks.has_next() and (count < 5) do
          let s = shrinks.next()?
          if s.size() == 0 then error end
          count = count + 1
        end
        if count == 0 then error end
        @pony_exitcode(1)
      | let v: String =>
        @pony_exitcode(1)
      end
    end
