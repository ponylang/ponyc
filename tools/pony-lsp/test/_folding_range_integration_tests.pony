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
    line  4: struct S                   → (4, 9)
    line  6:   new create() => None     — single-line, excluded
    line  8:   fun get(): U32 =>        → (8, 9)
    line 11: actor A                    → (11, 19)
    line 13:   new create() => _n = 0   — single-line, excluded
    line 15:   be tick() =>             → (15, 16)
    line 18:   fun count(): U32 =>      → (18, 19)
    line 21: trait T                    → (21, 25)
    line 24:   fun doubled(): U32 =>    → (24, 25)
    line 27: interface I                → (27, 31)
    line 30:   fun tripled(): U32 =>    → (30, 31)
    fun required(): U32 (lines 22, 28) — single-line, excluded
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
              (4, 9)
              (8, 9)
              (11, 19)
              (15, 16)
              (18, 19)
              (21, 25)
              (24, 25)
              (27, 31)
              (30, 31)]))])

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
    line 13:   match x ... end            → (13, 17)  last child: `"many"` at line 17
    line 20: fun with_while                → (20, 25)
    line 22:   while i < x do ... end     → (22, 23)  last child: `i = i + 1` at line 23
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
  FoldingRange for more_expressions.pony: verifies all 14 exact
  (startLine, endLine) pairs — 2 classes, 6 methods, and 6 expression ranges.

  `end` keywords have no source position in the typechecked AST, so endLine
  is always the last real statement/expression before `end`.

  more_expressions.pony layout (0-indexed lines):
    line  0: class _D                          → (0, 2)
    line  4: class MoreExpressions             → (4, 46)
    line  9: fun with_for                      → (9, 14)
    line 11:   for i in [...].values() do end  → (11, 12)  last: `sum = sum + i`
    line 16: fun with_repeat                   → (16, 21)
    line 18:   repeat...until i >= 3 end       → (18, 20)  last: condition `i >= 3`
    line 23: fun with_with                     → (23, 25)
    line 28: fun with_object                   → (28, 33)
    line 30:   object...end                    → (30, 31)  last: `fun value` body
    line 35: fun with_lambda                   → (35, 40)
    line 37:   {(): String =>...}              → (37, 38) × 2  last: `"hello world"`
    line 42: fun with_recover                  → (42, 46)
    line 43:   recover...end                   → (43, 46)  last: `consume s`
  (Lambda produces two ranges: one for tk_lambda, one for the desugared
   tk_object. Both span the same lines.
   `with` desugars before the typechecked AST; tk_with never appears, so
   no expression-level range is produced for with_with.)
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
            [ (0, 2)
              (4, 46)
              (9, 14)
              (11, 12)
              (16, 21)
              (18, 20)
              (23, 25)
              (28, 33)
              (30, 31)
              (35, 40)
              (37, 38)
              (37, 38)
              (42, 46)
              (43, 46)]))])

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
          "Expected (" + exp_sl.string() + ", " + exp_el.string() + ") not found")
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
