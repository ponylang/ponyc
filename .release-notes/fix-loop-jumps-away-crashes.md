## Fix compiler crashes in while and repeat loops that jump away

Several `while` and `repeat` loops whose body and/or `else` clause jump away crashed the compiler instead of compiling or reporting a clear error. For example, all of these crashed:

```pony
actor Main
  new create(env: Env) =>
    // (1) jumps-away loop with an uninferable literal else
    try repeat error until false else 2 end end

    // (2) jumps-away loop with a value else whose result is used
    let x: U8 = try repeat error until false else U8(2) end else U8(0) end

    // (3) a break with a value while the else jumps away (while and repeat)
    try repeat break U8(3) until false else error end end
    try while true do break U8(3) else error end end
```

This is fixed:

- A `break` that carries a value gives its loop both a value and an exit, so the loop no longer crashes when its `else` clause jumps away — it compiles and yields the break value (case 3). This applies to both `while` and `repeat`.
- A jumps-away loop whose `else` clause produces a value now compiles even when the loop's result is used; the loop simply produces no value (case 2).
- A bare, uninferable literal in such a loop (in the `else` clause, or as a `break` value with nothing to anchor it) is now rejected with a "could not infer literal type" error instead of crashing the compiler (case 1).
