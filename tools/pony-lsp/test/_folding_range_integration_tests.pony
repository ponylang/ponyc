use ".."
use "pony_test"
use "files"
use "json"

primitive _FoldingRangeIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let server = _LspTestServer(workspace_dir)
    test(_FoldingRangeClassTest.create(server))
    test(_FoldingRangeEntityTypesTest.create(server))
    test(_FoldingRangeExpressionsRangesTest.create(server))
    test(_FoldingRangeTypeRangeTest.create(server))
    test(_FoldingRangeMoreExpressionsRangesTest.create(server))
    test(_FoldingRangeIfdefTest.create(server))
    test(_FoldingRangeNestedExpressionsTest.create(server))

class \nodoc\ iso _FoldingRangeClassTest is UnitTest
  """
  FoldingRange for foldable.pony. Verifies that the class body and
  multi-line methods each produce a range, and that a single-line
  method is excluded.

  folding.pony layout (0-indexed lines):
    line  0:  class Foldable
    line  1:    \"\"\"
    line  2:    A class with members to test folding ranges.
    line  3:    \"\"\"
    line  4:    var _x: U32 = 0
    line  5:    (blank)
    line  6:    new create(x: U32) =>
    line  7:      _x = x
    line  8:    (blank)
    line  9:    fun get_value(): U32 =>
    line 10:      _x
    line 11:    (blank)
    line 12:    fun single_line(): U32 => _x

  Expected fold ranges:
    (0, 12) — class Foldable body
    (6,  7) — new create body
    (9, 10) — fun get_value body
    fun single_line is single-line (startLine == endLine) — excluded
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "folding_range/integration/class"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "folding_range/foldable.pony",
      [ ( 0, 0,
          _FoldingRangeChecker(
            [ (0, 12)
              (6, 7)
              (9, 10)]))])

class \nodoc\ iso _FoldingRangeEntityTypesTest is UnitTest
  """
  FoldingRange for entity_types.pony. Verifies that fold ranges are produced
  for all entity types (primitive, struct, actor, trait, interface) and their
  multi-line members, including be behaviors.

  Struct and actor receive explicit single-line constructors so that ponyc
  does not generate synthetic constructors, which would appear as extra ranges.

  entity_types.pony layout (0-indexed lines):
    line  0: primitive P                → (0, 2)
    line  1:   fun value(): U32 =>      → (1, 2)
    line  4: struct S                   → (4, 10)
    line  7:   new create() => None     — single-line, excluded
    line  9:   fun get(): U32 =>        → (9, 10)
    line 12: actor A                    → (12, 21)
    line 15:   new create() => _n = 0   — single-line, excluded
    line 17:   be tick() =>             → (17, 18)
    line 20:   fun count(): U32 =>      → (20, 21)
    line 23: trait T                    → (23, 27)
    line 26:   fun doubled(): U32 =>    → (26, 27)
    line 29: interface EntityTypes       → (29, 33)
    line 32:   fun tripled(): U32 =>    → (32, 33)
    fun required(): U32 (lines 24, 30) — single-line, excluded
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "folding_range/integration/entity_types"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "folding_range/entity_types.pony",
      [ ( 0, 0,
          _FoldingRangeChecker(
            [ (0, 2)
              (1, 2)
              (4, 10)
              (9, 10)
              (12, 21)
              (17, 18)
              (20, 21)
              (23, 27)
              (26, 27)
              (29, 33)
              (32, 33)]))])

class \nodoc\ iso _FoldingRangeExpressionsRangesTest is UnitTest
  """
  FoldingRange for expressions.pony: verifies all 9 exact (startLine, endLine)
  pairs — 1 class, 4 methods, and 4 expression-level ranges.

  `end` keywords have no source position in the typechecked AST, so endLine
  is always the last real statement/expression before `end`.

  expressions.pony layout (0-indexed lines):
    line  0: class Expressions             → (0, 31)
    line  5: fun with_if                   → (5, 9)
    line  6:   if x > 0 then ... end      → (6, 9)   last child: `0` at line 9
    line 12: fun with_match                → (12, 17)
    line 13:   match x ... end            → (13, 17)  last child:  at line 17
    line 20: fun with_while                → (20, 25)
    line 22:   while i < x do ... end     → (22, 23)  last child:  at line 23
    line 27: fun with_try                  → (27, 31)
    line 28:   try ... else ... end       → (28, 31)  last child: `0` at line 31
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "folding_range/integration/expressions_ranges"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "folding_range/expressions.pony",
      [ ( 0, 0,
          _FoldingRangeChecker(
            [ (0, 31)
              (5, 9)
              (6, 9)
              (12, 17)
              (13, 17)
              (20, 25)
              (22, 23)
              (27, 31)
              (28, 31)]))])

class \nodoc\ iso _FoldingRangeTypeRangeTest is UnitTest
  """
  FoldingRange for type_alias.pony: verifies the range spans lines 0–4.

  type_alias.pony layout (0-indexed lines):
    line 0: type TypeAlias is
    line 1:   ( U32
    line 2:   | String
    line 3:   | None
    line 4:   )
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "folding_range/integration/type_range"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "folding_range/type_alias.pony",
      [(0, 0, _FoldingRangeChecker([(0, 4)]))])

class \nodoc\ iso _FoldingRangeMoreExpressionsRangesTest is UnitTest
  """
  FoldingRange for more_expressions.pony: verifies all 13 exact
  (startLine, endLine) pairs — 2 classes, 6 methods, and 5 expression ranges.

  `end` keywords have no source position in the typechecked AST, so endLine
  is always the last real statement/expression before `end`.

  more_expressions.pony layout (0-indexed lines):
    line  0: class MoreExpressions             → (0, 42)
    line  5: fun with_for                      → (5, 10)
    line  7:   for i in [...].values() do end  → (7, 8)    via tk_while after sugar
    line 12: fun with_repeat                   → (12, 17)
    line 14:   repeat...until i >= 3 end       → (14, 16)  last: `i >= 3`
    line 19: fun with_with                     → (19, 21)
    line 24: fun with_object                   → (24, 29)
    line 26:   object...end                    → (26, 27)  via synthetic class
    line 31: fun with_lambda                   → (31, 36)
    line 33:   {(): String =>...}              → (33, 34)  via synthetic class
    line 38: fun with_recover                  → (38, 42)
    line 39:   recover...end                   → (39, 42)  last: `consume s`
    line 45: class _D                          → (45, 47)

  After PassExpr, ponyc replaces `object`/lambda literals in method bodies
  with constructor calls and appends synthetic entity classes to the module.
  The synthetic classes carry the source position of the original expression,
  so we emit fold ranges for them rather than scanning method bodies for
  tk_object/tk_lambda (which are no longer there after desugaring).

  `with` desugars before the typechecked AST; tk_with never appears, so
  no expression-level range is produced for with_with.

  The cap approach (next entity's line) prevents synthetic nodes from the
  desugared `with_with` dispose call (positioned at _D's line) from inflating
  MoreExpressions' end line past line 42.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "folding_range/integration/more_expressions_ranges"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "folding_range/more_expressions.pony",
      [ ( 0, 0,
          _FoldingRangeChecker(
            [ (0, 42)
              (5, 10)
              (7, 8)
              (12, 17)
              (14, 16)
              (19, 21)
              (24, 29)
              (26, 27)
              (31, 36)
              (33, 34)
              (38, 42)
              (39, 42)
              (45, 47)]))])

class val _FoldingRangeChecker
  """
  Validates a textDocument/foldingRange response. Each expected entry is
  (startLine, endLine). Asserts that every expected range appears in the
  response and that the total count matches.
  """
  let _expected: Array[(I64, I64)] val

  new val create(expected: Array[(I64, I64)] val) =>
    _expected = expected

  fun lsp_method(): String =>
    Methods.text_document().folding_range()

  fun lsp_range(): (None | (I64, I64, I64, I64)) =>
    None

  fun lsp_context(): (None | JsonObject) =>
    None

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    var ok = true
    try
      let ranges = res.result as JsonArray
      if not h.assert_eq[USize](
        _expected.size(),
        ranges.size(),
        "Expected " + _expected.size().string() +
        " ranges, got " + ranges.size().string())
      then
        ok = false
      end
      for (exp_sl, exp_el) in _expected.values() do
        var found = false
        for range_val in ranges.values() do
          try
            let sl = JsonNav(range_val)("startLine").as_i64()?
            let el = JsonNav(range_val)("endLine").as_i64()?
            if (sl == exp_sl) and (el == exp_el) then
              found = true
              break
            end
          end
        end
        if not h.assert_true(
          found,
          "Expected (" + exp_sl.string() + ", " +
          exp_el.string() + ") not found")
        then
          ok = false
        end
      end
      if not ok then
        h.log("Actual ranges:")
        for r in ranges.values() do
          try
            let sl = JsonNav(r)("startLine").as_i64()?
            let el = JsonNav(r)("endLine").as_i64()?
            h.log("  (" + sl.string() + ", " + el.string() + ")")
          end
        end
      end
    else
      h.log(
        "Expected foldingRange array but got: " + res.string())
      return false
    end
    ok

class \nodoc\ iso _FoldingRangeIfdefTest is UnitTest
  """
  FoldingRange for ifdef_expressions.pony: verifies that an ifdef block
  produces an expression-level folding range.

  `ifdef` is resolved to `if` by resolve_ifdef() during the expr pass, so the
  fold range is produced by the tk_if arm, not a tk_ifdef arm. The test
  verifies the correct user-facing behavior regardless of the internal token.

  ifdef_expressions.pony layout (0-indexed lines):
    line 0: class IfdefExpressions          → (0, 8)
    line 4:   fun with_ifdef(x: U32): U32   → (4, 8)
    line 5:     ifdef debug then ... end    → (5, 8)  via tk_if after resolution
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "folding_range/integration/ifdef"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "folding_range/ifdef_expressions.pony",
      [ ( 0, 0,
          _FoldingRangeChecker(
            [ (0, 8)
              (4, 8)
              (5, 8)]))])

class \nodoc\ iso _FoldingRangeNestedExpressionsTest is UnitTest
  """
  FoldingRange for nested_expressions.pony: verifies that compound expressions
  nested within other compound expressions each produce their own fold range,
  and that sequential compound expressions in a single method body both produce
  fold ranges.

  nested_expressions.pony layout (0-indexed lines):
    line  0: class NestedExpressions             → (0, 25)
    line  4:   fun with_nested(n: U32): U32 =>   → (4, 13)
    line  6:     while i < n do                  → (6, 11)  last: if's `end`
    line  7:       if (i % 2) == 0 then          → (7, 10)  nested inside while
    line 15:   fun with_sequential(n: U32): U32  → (15, 25)
    line 17:     if n > 0 then                   → (17, 20)  first sequential
    line 22:     while result > 0 do             → (22, 23)  second sequential
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "folding_range/integration/nested_expressions"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "folding_range/nested_expressions.pony",
      [ ( 0, 0,
          _FoldingRangeChecker(
            [ (0, 25)
              (4, 13)
              (6, 11)
              (7, 10)
              (15, 25)
              (17, 20)
              (22, 23)]))])
