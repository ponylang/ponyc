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

## Use embedded LLD for FreeBSD use=dtrace builds

When ponyc is built with `use=dtrace` on FreeBSD, it now links the programs it compiles with its built-in LLD linker, the same as every other build, instead of falling back to your system C compiler to perform the link. A `use=dtrace` compiler no longer depends on having an external compiler driver present and usable as a linker, and the programs it builds register and fire their `pony` provider DTrace probes exactly as before.

## Update supported OpenBSD version to 7.9

The supported version of OpenBSD is now 7.9. Our continuous integration builds and tests ponyc against OpenBSD 7.9.

We won't intentionally break OpenBSD 7.8, but we are no longer actively maintaining support for it. If you need ponyc on OpenBSD, we recommend running 7.9.

## Use embedded LLD for native macOS sanitizer builds

Native macOS sanitizer builds (`use=address_sanitizer`, `use=undefined_behavior_sanitizer`) now link through the embedded ld64.lld linker instead of requiring an external linker.

## Fix use=dtrace builds on macOS

Building ponyc with `use=dtrace` failed on macOS because the build tried to run `dtrace -G`, a flag that macOS dtrace has never supported. macOS's linker resolves DTrace probe symbols natively, so the `-G` step was never needed — only the `dtrace -h` header generation step is required.

`use=dtrace` now builds and links correctly on macOS. Programs compiled by a dtrace-enabled ponyc expose their `pony` provider probes to DTrace. Actually tracing a running program requires System Integrity Protection (SIP) to permit DTrace; see the examples/dtrace README for details.

## Fix intermittent crashes when compiling on multiple threads at once

The compiler could crash intermittently when more than one compilation ran at the same time within a single process, as the Pony language server does while you edit. Because the failure depended on thread timing, it appeared as occasional, hard-to-reproduce crashes rather than a consistent error.

Concurrent compilations no longer share state that simultaneous access could corrupt, so these crashes no longer happen.

## Reject self-referential iftype constraints inside lambdas and object literals

A self-referential `iftype` subtype condition — one whose subtype check refers back to the tested type parameter through a union, intersection, or tuple — is meaningless and is rejected when written in a method. The same condition inside a lambda or object literal was silently accepted. It is now rejected everywhere with the same error.

The following program previously compiled but now reports a clear error:

```pony
actor Main
  new create(env: Env) =>
    let f = {[A](x: A) =>
      iftype A <: (A | None) then None end}
    f[U8](0)
```

## Reject use=dtrace at configure time on DragonFly and OpenBSD

DTrace isn't supported on DragonFly BSD or OpenBSD. `make configure use=dtrace` now rejects the option on those platforms with a clear, platform-specific message rather than a confusing build failure or a generic error.

## Remove the `--linker` and `--link-ldcmd` command line options

ponyc now links every supported configuration with its embedded LLD linker, so the options that selected an external linker have been removed. `--link-ldcmd` already had no effect on the embedded linker, and `--linker` was an escape hatch back to the external linker, which no longer exists.

If you used `--linker` to link an additional library, declare it in your Pony source instead.

Before:

```
ponyc --linker="ld -lFoo"
```

After:

```pony
use "lib:Foo"
```

`use "path:..."` adds a library search path the same way. There is no longer a way to pass arbitrary linker flags directly; if your build relies on that, let us know in [this discussion](https://github.com/ponylang/ponyc/discussions/5448).

## Fix incorrect rejection and crashes for iftype conditions inside lambdas and object literals

An `iftype` condition behaved differently inside a lambda or object literal than it does at method scope, in two ways that are now fixed.

A condition that narrows a type parameter and then uses that narrowed parameter in the `then` branch compiled at method scope but was wrongly rejected inside a lambda or object literal. This was most visible with a recursively-constrained trait — for example `trait T[X: T[X]]` with the condition `iftype A <: T[A]`, which failed with "type argument is outside its constraint" — but it affected any narrowing condition whose narrowed parameter was used in the branch.

A `let` or `var` binding in the `then` branch of such a condition crashed the compiler with an internal assertion failure instead of compiling.

Both now behave inside a lambda or object literal exactly as they do at method scope. The following program previously failed to compile but now works:

```pony
trait T[X: T[X] #any]
  fun tag m(): X

class val C is T[C]
  fun tag m(): C => C.create()

actor Main
  new create(env: Env) =>
    let f = {[A](x': A) =>
      var x = x'
      iftype A <: T[A] then
        x = x.m()
      end}
    f[C](C)
```

## Fix runtime crash in optimized builds for types with 128-bit fields

Programs containing a class or struct with a `U128` or `I128` field could crash with a segmentation fault at runtime when compiled in release (optimized) mode, even though the same program ran correctly when compiled with `--debug`. The crash happened when such an object was used in a way that let the compiler place it on the stack.

These objects are now correctly aligned in optimized builds, and the crash no longer occurs.

## Add Alpine 3.24 as a supported platform

We've added arm64 and amd64 builds for Alpine Linux 3.24. We'll be building ponyc releases for it until it stops receiving security updates in 2028. At that point, we'll stop building releases for it.

## Fix `UDPSocket.set_multicast_interface` not setting the interface

`UDPSocket.set_multicast_interface` never actually set the interface used for outgoing multicast. It handed the operating system the address of an internal pointer rather than the resolved interface address, so the kernel received meaningless bytes and the socket's multicast interface was left unchanged, on both IPv4 and IPv6.

It now sets the interface correctly. For an IPv4 interface, pass the interface's IPv4 address. For an IPv6 interface, the interface is taken from the scope id of the resolved address, so a scoped address such as `"fe80::1%eth0"` is needed to select an interface.

## Fix `UDPSocket.set_multicast_loopback` and `set_multicast_ttl` having no effect

`UDPSocket.set_multicast_loopback` and `set_multicast_ttl` did not change a socket's IPv4 multicast loopback or TTL behavior. They applied the options at the wrong socket level, so the request either failed or set an unrelated option, leaving the multicast loopback and TTL at their defaults.

Both now take effect for IPv4 multicast.

## Fix Windows process crash when a UDP socket fails to listen

On Windows, a `UDPSocket` that failed to start listening — for example because its host address could not be resolved — would crash the entire process immediately after reporting the failure through `not_listening`. The failure is now reported and your program keeps running, matching the behavior on other platforms.

## Make `UDPSocket.set_broadcast` a no-op on IPv6 sockets

`UDPSocket.set_broadcast` promises to enable or disable broadcasting from a socket. On IPv4 sockets it does: it sets the `SO_BROADCAST` socket option to the value you pass. On IPv6 sockets it instead ignored its argument and joined the FF02::1 all-nodes multicast group — `set_broadcast(false)` joined the group too, and the group was never left.

IPv6 has no broadcast, and sending to a multicast address such as the all-nodes group (`DNS.broadcast_ip6`) requires no permission, so there is nothing for `set_broadcast` to enable on an IPv6 socket. It is now a documented no-op on IPv6 sockets. The removed join had no observable effect in practice — every IPv6 node is automatically a member of the all-nodes group — so existing programs should see no change in behavior. To receive traffic for a multicast group, use `multicast_join`.

## Fix crash when a directory is named like a Pony source file

Previously, if a directory inside a package was named like a Pony source file — that is, with a name ending in `.pony` — ponyc would abort with an out-of-memory error instead of compiling your program. Such directories are now ignored, like any other non-source entry in a package directory, and compilation proceeds normally.

## Fix NetAddress.scope() returning a byte-swapped value on little-endian platforms

`NetAddress.scope()` returned the IPv6 scope zone identifier -- the interface index of a scoped address, such as the `eth0` in `fe80::1%eth0` -- with its bytes reversed on little-endian platforms. An address scoped to interface index 2, for instance, returned 33554432 (0x02000000) instead of 2, making the value useless for identifying the interface.

`scope()` now returns the zone identifier correctly. Big-endian platforms were unaffected and are unchanged.

## Fix crash when tracing actor behaviors without forced actor tracing

A runtime built with `runtime_tracing` enabled would crash when the `actor_behavior` tracing category was active without `--ponytracingforceactortracing` also being set:

```
program --ponytracingcategories actor_behavior
```

The crash occurred whenever a message was scheduled from a thread that was not running an actor, such as the ASIO thread delivering a network event or the cycle detector. Programs that perform any I/O or run long enough to trigger cycle detection would reliably crash. Setting `--ponytracingforceactortracing all` avoided the crash.

This has been fixed. Actor behavior tracing now works without forcing actor tracing on.

## Fix unnecessary reallocations when building small Strings and Arrays

Building a small `String` or `Array` incrementally — appending or pushing through its first several elements — triggered repeated reallocations and copies, even though every one of those sizes fit within the single block the runtime allocator had already handed out. The collections now record the capacity of the block they were given, so a small `String` or `Array` allocates once and does not reallocate again until it genuinely outgrows that block.

A side effect is that `space()` may report a larger capacity than before for a small collection; the extra capacity is memory the allocator had already reserved, so a program's memory use is unchanged.

## Fix linking of Pony programs on 32-bit ARM Linux

Pony programs failed to link on 32-bit ARM Linux systems, such as a Raspberry Pi running a 32-bit OS. They now build and run correctly.

