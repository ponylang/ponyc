## Report an error for infinitely recursive generic types

Some generic code instantiates itself with an ever-growing type argument, requiring an unbounded number of concrete types. For example, a function that calls itself with a deeper type on each step:

```pony
primitive Bar
  fun apply[A: IFoo](n: USize): IFoo =>
    if n == 0 then
      A
    else
      Bar.apply[Pair[A]](n - 1)
    end
```

Previously the compiler tried to generate every one of those types and kept going until it exhausted all available memory and crashed, with no indication of what in your code caused it. The compiler now stops once a generic instantiation grows past a fixed limit and reports an error pointing at the generic function or type responsible, so you get a clear diagnostic instead of an out-of-memory crash. Genuinely recursive generic types like this remain unsupported — Pony has to know every concrete type ahead of time — but the failure is now explained rather than silent.
