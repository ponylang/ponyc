## Add byte_order standard library package

A new `byte_order` package provides `BigEndian` and `LittleEndian` primitives for converting multi-byte values to and from their wire representation.

Read methods decode from any `ReadSeq[U8]` source (with an optimized path for `Array[U8]`). Write methods append encoded bytes to an `Array[U8]`. Both primitives cover U16–U128, their signed counterparts, and F32/F64.

```pony
use "byte_order"

// Encode a U32 in network (big-endian) order
let buf = Array[U8]
BigEndian.push_u32(buf, 0xDEADBEEF)
// buf is now [0xDE; 0xAD; 0xBE; 0xEF]

// Decode it back
let value = BigEndian.u32[Array[U8] box](buf)?  // 0xDEADBEEF
```

## Change Stringable.string() to return String val instead of String iso^

The `Stringable` interface's `string()` method now returns `String val` (written as bare `String`) instead of `String iso^`. The previous return type forced every implementation to allocate a new string even when returning a literal or an existing `val` field, because `iso^` requires an isolated reference that the caller can freely mutate. No caller in the standard library or tools ever used that mutability — every call site consumed the result as `val` immediately.

With `String val`, implementations can return string literals directly with zero allocation. The common case of a type whose `string()` method returns a fixed label drops from two heap allocations to zero.

All code that implements `Stringable` must update its `string()` method signature. The migration is mechanical:

Before:

```pony
primitive Green is Stringable
  fun string(): String iso^ => "green".clone()

class MyClass is Stringable
  let _name: String

  fun string(): String iso^ =>
    recover iso
      String.>append("MyClass(")
        .>append(_name)
        .>append(")")
    end
```

After:

```pony
primitive Green is Stringable
  fun string(): String => "green"

class MyClass is Stringable
  let _name: String

  fun string(): String =>
    recover val
      String.>append("MyClass(")
        .>append(_name)
        .>append(")")
    end
```

Callers of `string()` require no changes — `String val` satisfies every context where `String iso^` was previously used as `val`.

The one case that needs attention: code that consumed the `string()` result as `iso` for in-place mutation. Add `.clone()` at the call site to get a mutable copy. This pattern did not occur in the standard library.

## Improve diagnostic for a match capture missing `let` or `var`

A match arm written as `| count: U64 => ...` without `let` or `var` used to be reported as an unterminated match. The compiler now points at the identifier and reports that a variable declaration requires `let` or `var`.

The form is still invalid. A capture is a variable declaration, so `let` or `var` is required:

```pony
match value
| let count: U64 => count.string()
else
  "other"
end
```

## Switch pony-lsp workspace model

pony-lsp's internal model of the workspace it was working on was mixing up corral projects, pony modules and workspaces. It tried to create one workspace manager actor per pony module. This was messing up inter-module dependencies and led to all sorts of erroneous artefacts such not detecting the correct dependencies for certain pony modules.

The new model is the following: An instance of pony-lsp is started with one workspace, which might consist of multiple directories. Those directories might contain multiple corral-projects with multiple pony modules. During workspace initialization the workspace directories are walked, pony modules discovered and their possibly transitive dependencies stated within `corral.json` files are resolved, so that each discovered pony-module has all the dependencies listed that are needed for compilation and static analysis within pony-lsp. The internal state is a map of pony-modules from directory to module data, AST information, derived symbol indices, diagnostic information etc.

Effectively, with this change, pony-lsp is now correctly resolving transitive dependencies from `corral.json`, pony modules not listed in `corral.json` like test modules or the pony standard library. Dependencies outside the workspace are now correctly resolved as well.

## Add runtime tracing toggle API

Tracing categories can now be changed while a program is running. The new `runtime_tracing` package provides three category operations — enable (add to active set), disable (remove from active set), and set (replace active set).

```pony
use "runtime_tracing"

actor Main
  new create(env: Env) =>
    let auth = RuntimeTracingAuth(env.root)

    match RuntimeTracing.enable_categories(
      auth, [SchedulerTracing; ActorBehaviorTracing])
    | TracingOk => env.out.print("tracing enabled")
    | TracingNotAvailable => env.out.print("not on a scheduler thread")
    | TracingLogFileError => env.out.print("could not open trace file")
    end
```

Each category is a typed primitive passed in an array. Actor event categories (`ActorTracing`, `ActorBehaviorTracing`, etc.) trace all actors when enabled. `CycleDetectorTracing` enables actor events for the cycle detector only. No trace file is created until the first call that enables a category.

`RuntimeOptions` now includes all tracing configuration fields, so `@runtime_override_defaults` can set initial tracing configuration from code.

## Remove `--ponytracingforceactortracing`

The `--ponytracingforceactortracing` CLI flag is removed. Actor tracing is now controlled through categories: enable `ActorTracing`, `ActorBehaviorTracing`, or other actor event categories to trace all actors, or enable `CycleDetectorTracing` to trace the cycle detector only.

Scripts and launch configurations that pass `--ponytracingforceactortracing` need to use `--ponytracingcategories` instead:

```
# before: trace all actors
--ponytracingforceactortracing all
# after:
--ponytracingcategories "actor,actor_gc,actor_behavior,actor_state_change"

# before: trace only the cycle detector
--ponytracingforceactortracing cd_only
# after:
--ponytracingcategories "cycle_detector"
```

## Remove orphaned ASIO thread tracing call

The ASIO event loop backends (epoll, kqueue, Windows IOCP) had a `TRACING_THREAD_STOP()` call with no corresponding `TRACING_THREAD_START()`. Since ASIO thread tracing can't be toggled at runtime, the orphaned stop call is removed.

## Add binary_codec package

A new standard library package for bidirectional binary parsing. Define a `Codec[A]` once — the same definition decodes bytes into a value and encodes the value back to bytes.

Sub-byte fields (4-bit nibbles, 1-bit flags, 13-bit offsets) work the same as byte-aligned fields. `decode` takes `Array[U8] val` so byte-slice codecs return zero-copy views via `Array.trim`.

Codecs are `val` and compose via combinators: `Seq2`–`Seq6` sequence fields into tuples, `Mapped` maps between wire tuples and domain types, `FlatZip` threads a decoded value into a dependent codec, `Many` and `Count` handle repetition, and `Discriminated` dispatches on a tag value.

Decode errors are data: `InsufficientData` means the input is incomplete, `MalformedData` means the data is structurally wrong. `FrameDecoder` wraps any codec for TCP stream framing and uses this distinction to separate "not enough data yet" from "corrupt message."

```pony
use "binary_codec"

class val UdpHeader
  let src_port: U16
  let dst_port: U16
  let length: U16
  let checksum: U16

  new val create(s: U16, d: U16, l: U16, c: U16) =>
    src_port = s; dst_port = d; length = l; checksum = c

actor Main
  new create(env: Env) =>
    let codec: Codec[UdpHeader] =
      Mapped[(U16, U16, U16, U16), UdpHeader](
        Seq4[U16, U16, U16, U16](U16BE, U16BE, U16BE, U16BE),
        {(t) => UdpHeader(t._1, t._2, t._3, t._4)},
        {(h) => (h.src_port, h.dst_port, h.length, h.checksum)})

    let data: Array[U8] val =
      [0x00; 0x50; 0x00; 0x51; 0x00; 0x08; 0x12; 0x34]

    match codec.decode(data, 0)
    | (let h: UdpHeader, let bits: USize) =>
      env.out.print("src=" + h.src_port.string())
      env.out.print("dst=" + h.dst_port.string())
    | InsufficientData =>
      env.out.print("need more data")
    | let e: MalformedData =>
      env.out.print("bad data: " + e.string())
    end
```

## Add `\noalloc\` annotation for methods

The `\noalloc\` annotation marks a method whose body must not allocate heap memory, either directly or through calls to other functions. The compiler enforces this in several ways: a `\noalloc\` method cannot make a virtual call to a method that is not also `\noalloc\`, a class implementing a trait with `\noalloc\` methods must mark its implementations `\noalloc\` too, FFI calls and bare lambda calls are not allowed inside a `\noalloc\` method, and the LTO pass verifies at link time that the transitive call graph of a `\noalloc\` function contains no allocation call, no FFI call, and no indirect call through a bare lambda.

```pony
trait Computable
  fun \noalloc\ compute(x: U32): U32

class Calculator is Computable
  fun \noalloc\ compute(x: U32): U32 => x * 2
```

The annotation can appear on `fun`, `be`, and `new` declarations.

