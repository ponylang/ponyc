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

