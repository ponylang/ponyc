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

  fun box enabled(): Bool =>
    _flag

  fun box inner_x(): U32 =>
    _inner.x

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

class _UninitLocal
  """
  Demonstrates a local variable declared without an inline initializer.
  `acc` is declared as `var acc: U32` (no `= expr`). All declarations are
  Write kind; the `acc = 0` assignment below is also Write; the return is Read.
  """
  fun ref example(): U32 =>
    var acc: U32
    acc = 0
    acc

class _TupleAssign
  """
  Demonstrates tuple destructuring assignment.
  LHS elements of a tuple assignment are Write kind.

  Place the cursor on `ta` to see both occurrences:
    the var declaration (Write — has initializer)
    the LHS of the tuple assign (Write)
    the use in ta + tb (Read)
  """
  fun ref work(): U32 =>
    var ta: U32 = 0
    var tb: U32 = 0
    (ta, tb) = (1, 2)
    ta + tb

class _TupleElemAccess
  """
  Demonstrates a tuple element reference (tk_tupleelemref).
  Placing the cursor on `_1` in `pair._1` should produce a Read highlight
  spanning the full `pair._1` expression.
  """
  fun box work(): U32 =>
    let pair: (U32, U32) = (42, 7)
    pair._1

actor _BeChainActor
  """
  Demonstrates tk_beref (expression receiver) and tk_newberef highlights.

  Place the cursor on `go` to see both occurrences: the behaviour
  declaration and the expression-receiver call site in chain_go (tk_beref).

  Place the cursor on `create` to see both occurrences: the constructor
  declaration and the call site in _NewBeRefExample (tk_newberef).
  """
  new create() =>
    None

  be go(n: U32) =>
    None

  fun box get_self(): _BeChainActor tag =>
    this

  fun box chain_go(): None =>
    get_self().go(1)

class _NewBeRefExample
  """
  Demonstrates tk_newberef highlights.
  Place the cursor on `create` in _BeChainActor.create() to see both
  occurrences: the constructor declaration and this call site.
  """
  fun ref work(): _BeChainActor tag =>
    _BeChainActor.create()
