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

