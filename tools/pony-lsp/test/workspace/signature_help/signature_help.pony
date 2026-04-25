"""
Test fixtures for exercising LSP signature help functionality.

This module provides test cases for manual and automated testing of
textDocument/signatureHelp provided by the Pony language server. Signature
help shows a method's parameter list with the active parameter highlighted
as you type arguments inside a call expression.

To manually test signature help:
1. Open the lsp/test/workspace directory as a project in your editor.
2. Save this file to ensure the language server has compiled it.
3. Open this file while the Pony language server is active.
4. Place your cursor inside any of the call expressions in `_SigUser.apply`
   and trigger signature help (typically via a keyboard shortcut or by typing
   `(` or `,`).

Expected signature help behaviour per call in `_SigUser.apply`:

  `_SigTarget(0)` — constructor call
    - Signature: `new create(v: U32)`
    - Cursor on `0`: activeParameter=0, parameter `v: U32` highlighted

  `_SigTarget(0).simple(42)` — single-parameter method with docstring
    - Signature: `fun box simple(x: U32): U32`
    - Cursor on `42`: activeParameter=0, parameter `x: U32` highlighted
    - Documentation popup shows: "Returns x plus the stored value."

  `_SigTarget(0).multi(1, "hello", true)` — multi-parameter method
    - Signature: `fun box multi(a: U8, b: String val, c: Bool): None`
    - Cursor on `1`: activeParameter=0, parameter `a: U8` highlighted
    - Cursor on `"hello"`: activeParameter=1, `b: String val` highlighted
    - Cursor on `true`: activeParameter=2, parameter `c: Bool` highlighted

  `_SigTarget(0).no_args()` — zero-parameter method
    - Signature: `fun box no_args(): U32`
    - No parameter highlighted (empty parameters list)

Note: because the server compiles on save (not on every keystroke), signature
help reflects the last saved state of the file. The popup will not appear
until the file has been saved at least once after opening.
"""

class _SigTarget
  let _value: U32

  new create(v: U32) =>
    _value = v

  fun box simple(x: U32): U32 =>
    """
    Returns x plus the stored value.
    """
    x + _value

  fun box multi(a: U8, b: String val, c: Bool): None =>
    None

  fun box no_args(): U32 =>
    _value

class _SigUser
  fun apply() =>
    _SigTarget(0).simple(42)
    _SigTarget(0).multi(1, "hello", true)
    _SigTarget(0).no_args()
    _SigTarget(0).multi( // a multi line call
      1,
      "hello",
      true)
    _SigTarget(_SigTarget(0).simple(1)).multi(2, "nested", false)

actor _SigActor
  be greet(name: String, count: U32) =>
    None

class _BeUser
  """
  Exercises signature help on a `be` (behaviour) call.

  To test: place the cursor inside `a.greet("hello", 1)` and trigger
  signature help (keyboard shortcut or type `(` or `,`).

  Expected for `a.greet("hello", 1)`:
    - Signature: `be greet(name: String val, count: U32 val)`
    - Cursor on `"hello"`: activeParameter=0, `name: String val` highlighted
    - Cursor on `1`: activeParameter=1, `count: U32 val` highlighted
  """
  fun apply(a: _SigActor tag) =>
    a.greet("hello", 1)

primitive _GenericSigPrimitive
  fun box typed[T: Any val](x: T, y: T): T =>
    x

class _GenericSigUser
  """
  Exercises signature help on a generic method call with explicit type args.

  To test: place the cursor on either `_n` inside
  `_GenericSigPrimitive.typed[U32](_n, _n)` and trigger signature help.

  Expected for `typed[U32](_n, _n)`:
    - Signature: `fun box typed[T: Any val](x: T, y: T): T`
    - Cursor on first `_n`: activeParameter=0, `x: T` highlighted
    - Cursor on second `_n`: activeParameter=1, `y: T` highlighted
  """
  let _n: U32 = 1

  fun apply() =>
    _GenericSigPrimitive.typed[U32](_n, _n)

class _NamedArgUser
  """
  Exercises signature help with named arguments (`where` syntax).

  To test: place the cursor inside either call in `apply` and trigger
  signature help.

  After typecheck the compiler resolves named args into positional order, so
  named-arg VALUES behave like positional args and get correct highlighting.
  Named-arg NAMES (the identifiers before `=`) return no signature help.

  Expected:
    `_SigTarget(0).multi(where a = U8(1), b = "hello", c = false)`:
      - Cursor on `b` (named-arg name): no signature help.
      - Cursor on `"hello"` (value for `b`): activeParameter=1,
        `b: String val` highlighted.
    `_SigTarget(0).multi(1 where b = "hello", c = false)`:
      - Cursor on `"hello"` (value for `b`): activeParameter=1,
        `b: String val` highlighted.
      - Cursor on `b` (named-arg name): no signature help.
  """
  fun apply() =>
    _SigTarget(0).multi(where a = U8(1), b = "hello", c = false)
    _SigTarget(0).multi(1 where b = "hello", c = false)
