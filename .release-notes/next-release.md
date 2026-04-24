## Strengthen memory ordering on runtime queue pushes

Pony's runtime uses several concurrent queues to hand off messages and actors between threads. On x86, atomic read-modify-writes are always full memory barriers regardless of the memory ordering requested by the program, so the previous code behaved correctly there. On ARM, aarch64, and other weakly-ordered architectures, the previous code relied on a subtle C11 memory-model rule (release-sequence extension across cross-thread read-modify-writes) to establish the required happens-before between one scheduler thread releasing an actor or a message and another scheduler thread picking it up.

This rule is correct under C11 but was narrowed in C++20, and depending on it made the runtime fragile under evolving compiler interpretations. It is a candidate contributor to the rare, recurring `aarch64` stress-test crashes tracked in https://github.com/ponylang/ponyc/issues/5243 (originally reported as https://github.com/ponylang/ponyc/issues/4069).

The push operations on the actor message queue and on the scheduler's multi-producer multi-consumer queue have been changed to establish happens-before directly with acquire-release read-modify-writes instead of through the release-sequence rule. The multi-producer push paths generate identical machine code on x86 because x86's atomic read-modify-writes are always full barriers regardless of the requested ordering. The single-producer push paths replace two plain atomic stores with one `xchg` on x86. On ARM and other weakly-ordered platforms, the generated code changes in all paths but shouldn't have an impact on performance. Stronger memory ordering should not introduce any incorrect behavior; the change is strictly defensive.

## Add LSP `textDocument/selectionRange` support

The Pony language server now handles `textDocument/selectionRange` requests, enabling editors to expand the selection to progressively larger syntactic units (e.g. Alt+Shift+→ in VS Code).

For a given cursor position the server returns a chain of nested ranges — innermost first — walking up the AST from the token under the cursor through its enclosing expressions, function, class body, and finally the whole file. Ancestor nodes whose span is identical to their child are collapsed so that each step in the chain produces a visible selection change. Descendant nodes from other source files (such as trait methods merged into a class by the compiler) are excluded so that the ranges always stay within the current file.

## Add LSP `workspace/symbol` support

The Pony language server now handles `workspace/symbol` requests, enabling workspace-wide symbol search (e.g. "Go to Symbol in Workspace" / Cmd+T in VS Code).

Given a query string, the server performs a case-insensitive substring search over all compiled symbols — top-level types (class, actor, struct, primitive, trait, interface) and their members (constructors, functions, behaviours, fields) — across every package in the workspace. An empty query returns all symbols. Results are returned as a flat `SymbolInformation[]` with file URI and source range; member symbols include a `containerName` identifying the enclosing type.

The server advertises `workspaceSymbolProvider: true` in its capabilities.

## Fix compiler crash when passing an array literal to `OutStream.writev`

Previously, the compiler would crash with an assertion failure when an array literal was passed to `env.out.writev` (or any other call expecting a `ByteSeqIter`):

```pony
actor Main
  new create(env: Env) =>
    env.out.writev([])
    env.out.writev(["foo"; "bar"])
```

The compiler's element-type inference for array literals, when the antecedent was an interface whose `values` method returned a type alias (such as `ByteSeqIter`, where `ByteSeq` is `(String | Array[U8] val)`), failed to fully strip viewpoint arrows from the inferred type. The leftover arrows eventually reached code generation and triggered an internal assertion.

This has been fixed. Code of the shape above now compiles and runs correctly.

## Fix runtime crash with iso in mixed-capability union type

Matching on a destructively read `iso` field would crash at runtime with a segfault in the GC when the field's union type also contained a `val` member. For example, this program would crash:

```pony
class A
  var v: Any val
  new create(v': Any val) =>
    v = consume v'

class B

actor Foo
  var _x: (A iso | B val | None)

  new create(x': (A iso | B val | None)) =>
    _x = consume x'

  be f() =>
    match (_x = None)
    | let a: A iso => None
    end

actor Main
  new create(env: Env) =>
    let f = Foo(recover A("hello".string()) end)
    f.f()
```

The crash occurred because the GC traced the `iso` object incorrectly, leading to a reference count imbalance and use-after-free. This has been fixed.

## Fix LSP symbol ranges

LSP clients that use `textDocument/documentSymbol` (the outline/breadcrumb view in most editors) could produce a "selectionRange must be contained in range" error, causing the entire symbol list to be rejected. This is now fixed.

In addition, symbol ranges across both `textDocument/documentSymbol` and `workspace/symbol` now correctly cover the full declaration — from the opening keyword to the end of the body. Previously, `textDocument/documentSymbol` ranges covered only the declaration keyword (`class`, `fun`, etc.), and `workspace/symbol` ranges covered only the identifier. Highlighting a symbol or jumping to it now selects the whole declaration.

## Fix LSP definition and type-definition ranges

`textDocument/definition` and `textDocument/typeDefinition` responses now return a range that covers the full declaration — from the opening keyword to the end of the body. Previously the range covered only the declaration keyword (`class`, `fun`, etc.).

Range computation now also correctly handles the last type declaration in a file. Previously the compiler's synthesized default constructors could cause the final entity's range to extend to an incorrect position; this no longer occurs.

