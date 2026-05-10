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

