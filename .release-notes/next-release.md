## Fix `use=dtrace` builds on FreeBSD

Building ponyc with `use=dtrace` failed on FreeBSD: the runtime build aborted with `dtrace: failed to link script ... No probe sites found for declared provider`, and even past that, programs compiled by a dtrace-enabled ponyc could not be linked.

`use=dtrace` now builds and links correctly on FreeBSD, and dynamically-linked programs expose their `pony` provider probes to DTrace.

`--static` programs build and run but do not expose their probes: FreeBSD registers DTrace USDT probes through the runtime linker, which statically-linked programs don't use.

## Use embedded LLD for native Linux sanitizer builds

When ponyc is built with sanitizers (such as `address_sanitizer` or `undefined_behavior_sanitizer`) on Linux, it now links the programs it compiles with its built-in LLD linker, the same as every other build, instead of falling back to your system C compiler to perform the link. Sanitizer-enabled native Linux compilation no longer depends on having an external compiler driver present and usable as a linker.

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

