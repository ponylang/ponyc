## Add LSP call hierarchy support

The Pony language server now supports the LSP call hierarchy protocol:

- `textDocument/prepareCallHierarchy` — when the cursor is on a method or constructor, returns a `CallHierarchyItem` describing it.
- `callHierarchy/incomingCalls` — returns items for all methods in the workspace that call the given method, with the specific call sites within each caller.
- `callHierarchy/outgoingCalls` — returns items for all methods called by the given method, with the specific call sites within it.

Editors that support this protocol (VS Code, Neovim, etc.) expose it as "Show Call Hierarchy", "Show Incoming Calls", and "Show Outgoing Calls" commands.

## Fix LSP range end positions overshooting past source line ends

Go-to-definition, document symbols, workspace symbols, type hierarchy, call hierarchy, and selection ranges could highlight text past the end of the declaration line. Editors that rely on this range for highlighting or cursor placement would overshoot into whitespace or the next token. This is now fixed.

## Fix LSP hover showing on declaration keywords

Hovering over a declaration keyword (`class`, `actor`, `trait`, `interface`, `primitive`, `type`, `struct`, `fun`, `be`, `new`, `let`, `var`, or `embed`) incorrectly showed a hover popup for the entity, method, or field being declared. Hover information now only appears when hovering over the declaration name itself.

## Fix LSP hover showing on capability keywords

Hovering over a capability keyword (`iso`, `trn`, `ref`, `val`, `box`, `tag`) no longer shows hover information. Previously, hovering on the receiver cap in `fun ref foo()` or the type cap in `String val` would pop up method or type hover info, which was incorrect.

## Fix compiler hang and crash on recursive generic types

Previously, the compiler would hang or crash on two shapes of recursive generic type. Both now compile or produce a normal type error.

A recursive generic interface whose method return type references the same interface with strictly larger type arguments would hang the compiler indefinitely:

```pony
// Drifting via tuple typeargs (ponylang/ponyc#1216).
interface Iter[A]
  fun enum[B](): Iter[(B, A)] => this
```

A type parameter whose constraint references the parameter itself would crash the compiler with a stack overflow:

```pony
// Recursive type parameter constraint (ponylang/ponyc#3930).
fun flatten[A: Array[Array[A]] #read](arrayin: Array[Array[A]]): Array[A] =>
  ...
```

The root cause was that `exact_nominal` in the structural subtype checker compared typeargs via `is_eq_typeargs`, which calls back into the subtype machinery and re-enters `check_assume` on the same recursive shapes. Replacing that with a structural AST equality check that compares definition pointers directly — without re-entering the subtype check — eliminates the re-entry while preserving semantic identity (two type parameters that share a source name in different scopes are correctly distinguished). Red Davies originally authored a fix along these lines for ponylang/ponyc#3930. Combined with the new `SAME_DEF_LIMIT` divergence guard in `is_nominal_sub_nominal`, which bounds the depth of any single drifting recursion chain, the compiler now terminates on both shapes above.

## LSP: drop behaviour return type from hover and signature help

Hover popups and signature help for `be` behaviours no longer display a return type. Behaviour return types are always `None val` inserted by the compiler and cannot be written explicitly in source, so showing them was unnecessary and did not add information.

## Fix LSP parameter hover to show valid Pony syntax

Hovering over a method parameter in the LSP previously showed `param name: String` — a `param` keyword that does not exist in Pony. It now shows `name: String`, which is the correct representation of a parameter.

## LSP now shows docstrings for class fields on hover

Docstrings on class fields are now shown when you hover over a field in your editor, consistent with how docstrings on classes, actors, primitives, and methods are already displayed.

## Fix Windows TCP connection silently leaking state on IOCP errors

When a Windows TCP connection's underlying socket failed during an IOCP write, or when the IOCP completion bookkeeping became inconsistent, the connection would silently leak its pending write state instead of closing. Subsequent writes would accumulate on a dead socket.

The connection now closes non-gracefully when these errors occur, matching the POSIX behavior.

## Fix LSP hover for lambda types

Hovering over a field, parameter, or variable with a lambda type annotation now shows the human-readable lambda type instead of a compiler-internal hygienic ID.

Before:

```pony
let _callback: $0 val
```

After:

```pony
let _callback: {(String val): None val} val
```

## Trim whitespace from INI section names

Previously, the INI parser left whitespace inside `[ name ]` as part of the section name, so an input like:

```ini
[ section ]
key = value
```

produced a section named `" section "`. Looking it up as `"section"` missed.

The parser already trims whitespace from lines, keys, and values. Not trimming section names was inconsistent rather than a deliberate dialect choice.

Section names are now trimmed of leading and trailing whitespace. `[ name ]` parses as `name`. Internal whitespace is preserved — `[a b]` is still `"a b"`. `[]` and `[   ]` are both the empty-string section.

This is a behavior change: any existing INI input that relied on the old quirk to distinguish sections by surrounding whitespace (e.g., treating `[section]` and `[ section ]` as different sections) will now see those sections collapse into one. Because `IniParse` overwrites duplicate keys with the last value seen, keys from the earlier section can be silently overwritten.

## Fix spurious LSP inlay hints inside lambda type annotations

Nominal types appearing inside lambda type annotations (e.g. `{(String): None} val` or `{(): String} val`) were causing spurious capability inlay hints at incorrect source positions — for example, a ` val` hint would appear in the middle of a type name. These hints no longer appear.

## Fix LSP document symbol outline dropping class members after lambda field initialisers

If a class contained a `let` field whose initialiser was a lambda literal (e.g. `let _f: {(U32): U32} val = {(x: U32): U32 => x}`), all class members declared after that field were silently missing from the document symbol outline (the structure view shown by editors).

The outline now correctly includes all named class members regardless of whether the class contains lambda field initialisers, lambda-typed fields, methods returning lambdas, or methods with lambda-typed parameters.

## Fix compiler crash when combining `iftype` and `as`

Previously, applying `as` to the result of an `iftype` expression that contained a method call on a narrowed type parameter would crash the compiler with an internal assertion failure (ponylang/ponyc#2042):

```pony
class LitString
interface AST
interface HasDocs
  fun val docs(): (LitString | None)

actor Main
  new create(env: Env) => None
  fun foo[A: AST val](node: A) =>
    try
      iftype A <: HasDocs
      then node.docs()
      end as LitString
    end
```

The compiler now compiles this code correctly.

## Fix missing question mark check for partial calls in trait default bodies

Calling a partial method without `?` inside a trait's default method body now correctly produces a compile error, matching the behavior of primitives and interfaces.

Previously, code like this compiled silently:

```pony
trait T
  fun f1() ? => error
  fun f2() ? => f1()    // missing `?`, but compiler did not complain
```

The same code in a primitive or interface correctly errored; only traits were affected.

If your code was relying on this missing check, add the `?` to the call site:

```pony
trait T
  fun f1() ? => error
  fun f2() ? => f1()?
```

## Fix typecheck assertion failure on loops whose branches all jump away

Previously, defining a loop whose body and else clause both jump away (for example, a `while` with `break` in the body and `return` in the else) inside a function other than `create` triggered an internal compiler assertion:

```pony
actor Main
  fun a() =>
    while true do
      break
    else
      return
    end

  new create(env: Env) =>
    a()
```

```
src/libponyc/pass/expr.c:698: pass_expr: Assertion `errors_get_count(options->check.errors) > 0` failed.
```

This has been fixed. Loops whose branches all jump away now compile correctly.

## Fix compiler crash when a behavior satisfies a non-tag interface method

Previously, the compiler would crash with an assertion failure when an actor's behavior was used to satisfy an interface method declared with a `box` or `ref` receiver capability:

```pony
interface IFunBox
  fun box apply(s: String)

actor Main
  let _env: Env
  new create(env: Env) =>
    _env = env
    let x: IFunBox = this
    x("hello")

  be apply(s: String) => _env.out.print(s)
```

This is a valid subtype relationship — a behavior runs with a `tag` receiver, and `box`/`ref` are both subcaps of `tag`, so the contravariant receiver check holds. The crash has been fixed. Code of this shape now compiles and runs correctly.

## Allow finite recursive type aliases

Type aliases can now reference themselves, as long as the resulting type has a finite layout. The compiler used to reject every self-referential alias, including ones that would have been perfectly fine to construct — JSON-like data, parse trees, and other tree-shaped patterns couldn't be expressed as aliases.

```pony
use "collections"

// Legal: JSON-like recursive structure.
type JsonValue is
  ( String
  | F64
  | Bool
  | None
  | Array[JsonValue]
  | Map[String, JsonValue])

// Legal: tree built from a generic carrier.
type Tree is (None | Array[Tree])
```

The aliases declare type *shape*. As with any Pony type, sending a value of one of these aliases across actors needs a sendable capability — the JSON example used for messaging would typically be `JsonValue val`, with the inner Array and Map constructed inside `recover val ... end` blocks.

Recursion is allowed when the cycle threads through some generic class's, actor's, or other nominal type's type-argument position (like `Array[T]` or `Map[K, V]`) and there's a union somewhere reachable from the cycle with at least one member that doesn't loop back. Without both, the recursion is infinite. The most common illegal shapes:

```pony
// Illegal: bare self-reference. The recursive arm has no
// constructor wrapping it, so every value is just None.
type A is (A | None)

// Illegal: recursion through a tuple element. Pony tuples are
// inline value types, so each IntList value would need to inline
// another IntList transitively. No finite layout exists.
type IntList is (None | (U32, IntList))

// Illegal: the recursion threads through Array's type argument,
// but no union with a non-recursive alternative is reachable, so
// there's no base case to stop at.
type A is Array[A]
```

When you write something illegal, the compiler reports `type alias 'X' can't be infinitely recursive` along with a note suggesting the fix. Either thread the recursion through the type argument of a generic class (or other nominal type) like `Array[X]`, or add a non-recursive alternative in a union (`(None | <body>)`).

## Replace stack-unwinding error handling with error-flag returns

Pony's `error` keyword no longer unwinds the stack to propagate errors. Partial functions now return an error flag alongside their result, which `try` and `?` check directly. For pure Pony code the change is invisible: `error`, `try`, and `?` behave exactly as they did before.

The change is visible at the boundary with C. The `pony_error()` runtime function has been removed. C code that signalled a Pony error by calling `pony_error()` must now report failure through its return value, and the Pony side must check that value and raise `error` itself.

Because `pony_error()` was the only way a partial FFI function could signal an error, partial FFI is no longer supported. A `?` on an FFI declaration (`use @foo(...) ?`) or on an FFI call (`@foo()?`) is now a compile error; remove it.

Bare lambdas (`@{...}`) that raise `error` outside a `try` now abort the program rather than unwinding, so a bare partial lambda can no longer propagate an error across a C stack frame.

If you have C code that calls `pony_error()`:

```c
// Before: signal failure by raising a Pony error from C.
PONY_API size_t my_read(...)
{
  if(failed)
    pony_error();
  return bytes_read;
}
```

```c
// After: report failure through the return value. The mechanism is up
// to you; here, a sentinel the Pony caller knows to treat as failure.
PONY_API size_t my_read(...)
{
  if(failed)
    return SIZE_MAX;
  return bytes_read;
}
```

```pony
// Before: the FFI declaration is partial and the call site uses `?`.
use @my_read[USize](...) ?

fun read(...): USize ? =>
  @my_read(...)?
```

```pony
// After: the declaration is not partial; check the sentinel yourself.
use @my_read[USize](...)

fun read(...): USize ? =>
  let r = @my_read(...)
  if r == USize.max_value() then error end
  r
```

## Remove the serialise package from the standard library

The `serialise` package has been removed from the standard library. Code that does `use "serialise"` will no longer compile.

The package was a security footgun: it was only safe when used with fully trusted data, and deserializing untrusted data could crash the program or hand hostile code access to the machine. The capability tokens gating the API did nothing to make deserialization of untrusted input safe. Rather than rework the package, the maintainers chose to remove it. This was ratified as RFC #83.

If you relied on `serialise`, you will need to implement serialization suited to your own use case and security requirements.

## Change socket runtime functions to use three-state result type

The five `PONY_API` socket runtime functions — `pony_os_writev`, `pony_os_send`, `pony_os_recv`, `pony_os_sendto`, and `pony_os_recvfrom` — have a new signature. Previously they were partial Pony functions returning `USize` that called `pony_error()` on failure, with `0` doubling as a "would-block" signal. They now return a three-state result (`PONY_SOCKET_OK = 0`, `PONY_SOCKET_RETRY = 1`, `PONY_SOCKET_ERROR = 2`) and write the operation's byte count through a new trailing `size_t* count_out` parameter.

Anyone calling these functions from Pony via FFI must update both their `use @...` declarations and their call sites.

The recommended call-site pattern uses a Pony-side dual of the result type with `match \exhaustive\`, so a future state addition on the C side surfaces as a compile error rather than a silent fall-through. The Pony stdlib's dual lives at `packages/net/_socket_result.pony` and is package-private — downstream FFI consumers should define their own dual following the same shape.

Before:
```pony
use @pony_os_recv[USize](event: AsioEventID, buffer: Pointer[U8] tag,
  size: USize) ?

try
  let len = @pony_os_recv(event, buffer, size)?
  if len == 0 then
    // would-block path
  else
    // len bytes were received
  end
else
  // pony_error() fired in the C runtime
end
```

After:
```pony
use @pony_os_recv[U8](event: AsioEventID, buffer: Pointer[U8] tag,
  size: USize, count_out: Pointer[USize])

// Define the dual once, in your package:
primitive MyOk    fun apply(): U8 => 0
primitive MyRetry fun apply(): U8 => 1
primitive MyError fun apply(): U8 => 2

type MyResult is (MyOk | MyRetry | MyError)

primitive MyResultDecoder
  fun apply(v: U8): MyResult =>
    match v
    | MyOk()    => MyOk
    | MyRetry() => MyRetry
    else MyError
    end

// At each call site:
var count: USize = 0
match \exhaustive\ MyResultDecoder(
  @pony_os_recv(event, buffer, size, addressof count))
| MyOk    => // count holds the bytes received
| MyRetry => // would-block, try again later
| MyError => // unrecoverable error
end
```

## Fix duplicate error for partial method call with type arguments

Previously, when a partial method with type parameters was called and the required `?` was missing, the compiler emitted the same error twice:

```pony
class C
  fun f1[A: Any val](x: A): A ? => error
  fun f2() ? => f1[U32](42)              // missing `?`
```

```
main.pony:3:15: call is not partial but the method is - a question mark is required after this call
  fun f2() ? => f1[U32](42)
              ^
    Info:
    main.pony:2:34: method is here

main.pony:3:15: call is not partial but the method is - a question mark is required after this call
  fun f2() ? => f1[U32](42)
              ^
    Info:
    main.pony:2:34: method is here
```

The same duplication occurred for the reverse mistake — a `?` on a call to a non-partial method with type parameters. It also occurred for chained `.>` calls, for partial constructor calls, and when type arguments were filled in from defaults rather than written explicitly. The compiler now emits each diagnostic exactly once.

Additionally, taking the address of a partial method with type arguments (e.g. `addressof obj.method[U32]`) previously triggered a compiler assertion failure. The same construct now compiles correctly.

## Fix incorrect `#any` capability for type parameters with union constraints

A type parameter constrained by a union type — for example,
`[O: (Foo tag | Bar ref)]` — was being given the `#any` capability even when
every member's capability was in `#alias` (one of `ref`, `val`, `box`, or
`tag`). That made common patterns fail to type-check:

```pony
primitive Writer[O: (Async tag | Sync ref)]
  fun _write(out: O, data: String) =>
    None

  fun ok(out: O) =>
    _write(out, "+OK\r\n")  // error: O #any ! is not a subtype of O #any
```

The compiler now derives the correct capability for these constraints. In the
example above, the capability is `#alias` (the smallest capability set that
contains both `tag` and `ref`), and the program compiles.

The workaround of intersecting the union with `Any #alias` is no longer
needed.

## Reject adjacent string literals

The compiler used to silently accept code where two string literals were placed back-to-back with no separator between them, treating them as two unrelated expressions. This most commonly came up as a confusing failure mode for typos involving `"""`, where a missing or misplaced quote produced a program that compiled but behaved nothing like what was written. Adjacent string literals are now reported as a syntax error.

## Reject identity comparison between distinct concrete types

The compiler now rejects `is` and `isnt` comparisons whose operand types are two distinct concrete entity types (any pair drawn from `class`, `actor`, `primitive`, and `struct`). Such comparisons can never be true, so they are almost always a bug; flagging them at compile time surfaces the mistake immediately instead of producing a constant `false` at runtime.

```pony
class C
class D

actor Main
  new create(env: Env) =>
    let c: C = C
    let d: D = D
    if c is d then None end  // now a compile-time error
```

The rule fires only when both operands resolve to a single concrete nominal type. Comparisons involving traits, interfaces, unions, intersections, tuples, or type parameters continue to compile, even when one side is concrete. Two reifications of the same generic class (for example `Array[U64]` and `Array[U32]`) share a single root definition and are also not flagged. Two `object end` literals at different source locations synthesize distinct anonymous classes and are flagged.

If you hit the new error, the comparison was unreachable. Switch to structural equality (`==`) or remove the comparison.

## Update to LLVM 22.1.6

We've updated the LLVM version used to build Pony from 21.1.8 to 22.1.6.

## Use embedded LLD for native FreeBSD builds

FreeBSD linking now uses the embedded LLD linker (ELF driver) instead of invoking an external compiler driver via `system()`. You no longer need a C compiler installed solely to link Pony programs on FreeBSD. This is part of the ongoing work to eliminate external linker dependencies across all platforms.

The embedded LLD path activates automatically for any FreeBSD target without `--linker` set. To use an external linker instead, pass `--linker=<command>` as an escape hatch to the legacy linking path. The `--link-ldcmd` flag is ignored when using embedded LLD; use `--linker` instead to get legacy behavior.

## Use embedded LLD for native DragonFly builds

DragonFly linking now uses the embedded LLD linker (ELF driver) instead of invoking an external C compiler driver via `system()`. Pkg gcc (e.g. `gcc13`) is still required at link time to supply libgcc, libgcc_s, libatomic, and the GCC CRT objects — the same install BUILD.md already prescribes for building ponyc on DragonFly. This is part of the ongoing work to eliminate external linker dependencies across all platforms.

## Use embedded LLD for native OpenBSD builds

OpenBSD linking now uses the embedded LLD linker (ELF driver) instead of invoking an external compiler driver via `system()`. You no longer need to invoke a C compiler driver solely to link Pony programs on OpenBSD; the base toolchain's `clang` is still required to build ponyc itself, but ponyc no longer shells out to it at link time. This is part of the ongoing work to eliminate external linker dependencies across all platforms.

The embedded LLD path activates automatically for any OpenBSD target without `--linker` set. To use an external linker instead, pass `--linker=<command>` as an escape hatch to the legacy linking path. The `--link-ldcmd` flag is ignored when using embedded LLD; use `--linker` instead to get legacy behavior.

