"""
Test fixtures for exercising LSP Go to Type Definition functionality.

Open this file in an LSP-aware editor while the Pony language server is active.
Place the cursor on any of the symbols below and invoke Go to Type Definition
(typically bound to a right-click menu option or a keyboard shortcut such as
Ctrl+F12 / Cmd+F12 in VS Code).

The editor should navigate to the declaration of the *type* of the symbol,
not the symbol itself:

  - `obj` in `demo()` — explicitly annotated `let`; navigates to
    `_TypeDefTarget`
  - `target` in `with_param()` — parameter; navigates to `_TypeDefTarget`
  - `x` in `inferred_demo()` — inferred type, no annotation; navigates to
    `_TypeDefTarget`

Placing the cursor on a keyword such as `class` or `fun` returns no results,
because keywords have no type.
"""

class _TypeDefTarget
  new create() =>
    None

class _TypeDefUser
  fun demo() =>
    let obj: _TypeDefTarget = _TypeDefTarget.create()
    obj

  fun with_param(target: _TypeDefTarget) =>
    target

  fun inferred_demo() =>
    let x = _TypeDefTarget.create()
    x
