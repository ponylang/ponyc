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

## Reject self-referential type parameter constraints

A generic type parameter whose constraint referred back to itself used to crash the compiler. For example:

```pony
class A[B: (B | C)]
```

The compiler now reports a clear error for these constraints instead of crashing.

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

## Fix compiler crash on partial application of a method with a literal default argument

Partially applying a method whose default argument is built from a numeric literal would crash the compiler. For example, this crashed during compilation:

```pony
use "format"

actor Main
  new create(env: Env) =>
    Format~apply()
```

`Format.apply` has a parameter whose default argument is `-1`, and partially applying the method triggered the crash. Any method with a comparable default (for instance `0 + 1`) was affected. These now compile and behave correctly.

Relatedly, partially applying a method whose default argument is itself invalid — such as `(-1).abs()`, where the literal has no type to look up `abs` on — now reports a normal compile error instead of crashing the compiler.

## Add JsonPrinter for serializing any JsonValue

The `json` package can now serialize any `JsonValue` — objects, arrays, and scalars alike — to a JSON string via the new `JsonPrinter` primitive. It is the dual of `JsonParser`: where `JsonParser.parse` turns a `String` into a `JsonValue`, `JsonPrinter.print` turns a `JsonValue` back into JSON.

Previously only `JsonObject` and `JsonArray` could be serialized; scalar values had no correct serializer (printing `None` produced `None` instead of `null`, and strings were not escaped). `JsonPrinter` handles the whole `JsonValue` union, so this is also the answer to "how do I serialize my data as JSON?": build a `JsonValue`, then hand it to `JsonPrinter`.

```pony
let doc = JsonObject
  .update("name", "Alice")
  .update("age", I64(30))

JsonPrinter.print(doc)         // {"name":"Alice","age":30}
JsonPrinter.pretty(doc)        // pretty-printed, two-space indent by default
JsonPrinter.print(None)        // null
JsonPrinter.print("hi\"there") // "hi\"there"
```

## Rename JsonObject and JsonArray serialization methods

`JsonObject` and `JsonArray` no longer implement `Stringable`. Their `string()` and `pretty_string()` methods have been renamed to `print()` and `pretty_print()`, matching the new `JsonPrinter` and the `parse`/`print` naming in the package.

This is a breaking change. Code that serialized a `JsonObject` or `JsonArray` needs to call the new method names, and code that relied on these types being `Stringable` (for example passing one where a `Stringable` is expected) should use `JsonPrinter.print` instead.

```pony
// Before
let s = my_object.string()
let p = my_array.pretty_string()

// After
let s = my_object.print()
let p = my_array.pretty_print()
// or, for any JsonValue including scalars:
let s' = JsonPrinter.print(my_value)
```
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

## Use embedded LLD for native FreeBSD sanitizer builds

When ponyc is built with sanitizers (such as `address_sanitizer` or `undefined_behavior_sanitizer`) on FreeBSD, it now links the programs it compiles with its built-in LLD linker, the same as every other build, instead of falling back to your system C compiler to perform the link. Sanitizer-enabled native FreeBSD compilation no longer depends on having an external compiler driver present and usable as a linker.

## Reject tuple types hidden in an intersection within a type constraint

Tuple types can't be used as generic type constraints. The compiler already rejected a tuple smuggled into a constraint through a type alias, including when it was hidden inside a union. It did not, however, catch a tuple hidden inside an intersection, so the following incorrectly compiled:

```pony
type R is (U8 & (U8, U32))
class Block[T: R]
```

The compiler now rejects this with the same error it already gives for tuples in unions:

```
constraint contains a tuple; tuple types can't be used as type constraints
```

