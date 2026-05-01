"""
Test fixtures for exercising LSP inlay hint functionality.

This module provides test cases for manual and automated testing of the inlay
hints provided by the Pony language server. Three kinds of hints are covered:

  1. Inferred type hints — shown after `let`/`var` declarations with no type
     annotation. Example: `let x = "hello"` shows `: String val`.

  2. Capability hints on type annotations — shown after a type name when a
     capability is present in the AST but absent from source. Example:
     `let x: String = "hello"` shows ` val` after `String`.
     Also applies to generic type arguments: `Array[U32]` shows ` val` after
     `U32` and ` ref` after `Array`.
     Also applies to union and tuple members: `(U32 | None)` shows ` val`
     after each.

  3. Function hints — shown on `fun` declarations:
     - Receiver capability hint after `fun` when no cap keyword is present
       (e.g. `fun foo()` shows ` box` between `fun` and `foo`).
     - Return type hint after `)` when no return type annotation is present
       (e.g. `fun foo()` shows `: None val` after the closing paren).
     - Return type capability hint after the return type name when an explicit
       return type annotation omits the capability (e.g. `fun f(): String`
       shows ` val` after `String`).

To manually test inlay hint functionality:
1. Open the lsp/test/workspace directory as a project in your editor.
2. Open the files in the inlay_hint directory while the Pony language server
   is active.
3. Enable inlay hints in your editor settings if they are not on by default.

Expected inlay hint behaviour per file:

  _inlay_hint.pony — basic hint types
    - `let inferred_string = "hello"` → `: String val` after the name
    - `var inferred_bool = true` → `: Bool val` after the name
    - `let explicit_string: String = "world"` → ` val` after `String`
    - `fun demo(): String` → ` box` before `demo`, ` val` after `String`
    - `fun inferred_fun()` → ` box` before `inferred_fun`,
      `: None val` after `)`
    - `fun explicit_multiline()` with `: String` on the next line →
      ` box` before the name, ` val` after `String`

  _generics.pony — generic and field cap hints
    - `let arr_u32: Array[U32]` → ` val` after `U32`, ` ref` after `Array`
    - `let nested: Array[Array[U32]]` → ` val`, ` ref` (inner), ` ref` (outer)
    - `let partial: Array[Array[U32] ref]` → ` val` (U32), ` ref` (outer Array)
    - `let full: Array[Array[U32] ref] ref` → ` val` (U32 only)
    - `let inferred = Array[U32]` → `: Array[U32 val] ref` (full inferred type)
    - Class fields follow the same pattern as local declarations
    - Return type annotations on methods follow the same pattern as fields

  _union_types.pony — union and tuple cap hints
    - `let u: (U32 | None)` → ` val` after `U32`, ` val` after `None`
    - `let t: (U32, Bool)` → ` val` after `U32`, ` val` after `Bool`
"""
