"""
Test fixtures for exercising LSP document highlight functionality.

Open this file in an LSP-aware editor while the Pony language server
is active. Place the cursor on any symbol described in the class
docstrings below. The editor should highlight all occurrences of that
symbol in the file simultaneously. Placing the cursor on a declaration
or any reference produces the same set of highlights.
"""

class Highlights
  """
  Demonstrates fvar field and let local highlights.

  Place the cursor on `count` to see all 5 occurrences highlighted:
  the field declaration, both sides of the assignment in tick, the
  return value in value, and the right-hand side in use_local.

  Place the cursor on `result` to see 3 occurrences highlighted:
  the let declaration and both uses in the expression below it.
  """
  var count: U32 = 0

  fun ref tick() =>
    count = count + 1

  fun box value(): U32 =>
    count

  fun box use_local(): U32 =>
    let result: U32 = count
    result + result

class _Inner
  """
  Demonstrates class-type, fvar field, and constructor highlights.
  This class is embedded inside _HighlightMore.

  Place the cursor on the `_Inner` class name to see all 4
  occurrences highlighted: the declaration here, the embed type
  annotation in _HighlightMore, and both _Inner.create() receiver
  expressions in _HighlightMore's constructors.

  Place the cursor on `x` to see both occurrences: the field
  declaration and the assignment in create.

  Place the cursor on `create` to see all 3 occurrences: the
  constructor declaration here and both call sites in _HighlightMore.
  """
  var x: U32 = 0

  new create() =>
    x = 0

class _HighlightMore
  """
  Demonstrates flet, embed, fun/funref/funchain, param, and var
  local highlights. Also demonstrates class-type highlights for
  _HighlightMore itself.

  Place the cursor on `_HighlightMore` (the class name) to see
  both occurrences: the declaration and the return type of get_self.

  Place the cursor on `_flag` to see all 3 occurrences: the flet
  declaration and its assignment in each constructor.

  Place the cursor on `_inner` to see all 3 occurrences: the embed
  declaration and its assignment in each constructor.

  Place the cursor on `add` to see all 3 occurrences, exercising
  three call forms: the fun declaration, a chained call via
  get_self().add(1) (tk_funchain), and a direct call in do_work
  (tk_funref).

  Place the cursor on the `x` parameter of add to see all 3
  occurrences: the parameter declaration and both uses in the body.

  Place the cursor on `w` in do_work to see all 4 occurrences: the var
  declaration, both sides of the assignment below it, and the return expression.
  """
  let _flag: Bool
  embed _inner: _Inner
  var _val: U32

  new create() =>
    _flag = true
    _inner = _Inner.create()
    _val = 0

  new other() =>
    _flag = false
    _inner = _Inner.create()
    _val = 1

  fun ref add(x: U32): U32 =>
    _val = _val + x
    x

  fun ref get_self(): _HighlightMore ref =>
    this

  fun ref chain_add(): U32 =>
    get_self().add(1)

  fun ref do_work(n: U32): U32 =>
    let v: U32 = n
    var w: U32 = v
    w = w + add(n)
    w

actor _HighlightRunner
  """
  Demonstrates behaviour and beref highlights.

  Place the cursor on `run` to see both occurrences: the behaviour
  declaration and the call site in trigger.

  Place the cursor on `None` in the body of run to see no highlights:
  None is a type-literal expression that the compiler desugars to an
  implicit None.create() call and has no referenceable identity.
  """
  be run(n: U32) =>
    None

  fun ref trigger() =>
    run(1)

class _LiteralExamples
  """
  Provides float and string literal expressions for testing that
  cursor-on-literal produces no highlights.
  """
  let _f: F64 = 3.14
  let _s: String val = "hello"
