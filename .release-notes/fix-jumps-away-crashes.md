## Fix compiler crashes involving control expressions that jump away

A control expression that "jumps away" with no value — `error`, `return`,
`break`, or `continue` — has no type. Using one in several positions crashed the
compiler instead of compiling or reporting a clear error.

A `repeat` loop whose `else` clause jumps away now compiles correctly:

```pony
actor Main
  new create(env: Env) =>
    try let x: U8 = repeat 1 until false else error end end
```

Using a jump-away expression where a value is required now produces a clear
compile error instead of crashing the compiler. This covers many positions,
including conditions, `match` operands and guards, `recover` operands, call and
FFI arguments, method receivers, `as` and identity (`is`/`isnt`) operands,
default arguments, lambda captures, and tuple elements:

```pony
// each of these now reports an error rather than crashing the compiler
if error then U8(1) else U8(2) end
match error | let y: U8 => y else U8(0) end
recover error end
let n: U8 = some_function(error)
(error).string()
let t: (U8, U8) = (1, (error))
```
