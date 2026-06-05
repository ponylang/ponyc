## Fix compiler crashes in while and repeat loops that jump away

Several `while` and `repeat` loops whose body and/or `else` clause jump away crashed the compiler or, with a debug build, produced invalid code, instead of compiling or reporting a clear error. For example, all of these were broken:

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

- A `break` that carries a value gives its loop both a value and an exit, so the loop no longer crashes when its `else` clause jumps away — it compiles and yields the break value (case 3). The same is true of a `continue` that reaches a value-producing `else`. This applies to both `while` and `repeat`.
- A loop that genuinely jumps away (its body always errors or returns) now compiles correctly whether or not its result is used, including inside a `try` (case 2). It simply produces no value.
- A bare, uninferable literal in such a loop (in the `else` clause, or as a `break` value with nothing to anchor it) is now rejected with a "could not infer literal type" error instead of crashing the compiler (case 1).

One related change: a `while` or `repeat` loop that jumps away makes any code after it unreachable, so the compiler now reports `unreachable code` for it — the same error an equivalent `if` already gives. This affects a loop used as the body of a function with no explicit return, such as `while true do break else return end`, which previously compiled.
