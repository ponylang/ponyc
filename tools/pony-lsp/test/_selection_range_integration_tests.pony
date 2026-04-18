use ".."
use "pony_test"
use "files"
use "json"

primitive _SelectionRangeIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let server = _LspTestServer(workspace_dir)
    test(_SelectionRangeTokenTest.create(server))
    test(_SelectionRangeMethodTest.create(server))
    test(_SelectionRangeWhitespaceTest.create(server))
    test(_SelectionRangeKeywordTest.create(server))
    test(_SelectionRangePositionsArrayTest.create(server))
    test(_SelectionRangeEmptyPositionsTest.create(server))

class \nodoc\ iso _SelectionRangeTokenTest is UnitTest
  """
  Cursor on `x` (body expression) at line 27, character 4 (0-indexed) in
  `fun value(x: U32): U32 => x`. Expects a chain of at least 3 SelectionRange
  entries with monotonically expanding ranges, innermost exactly (27,4)-(27,5).

  selection_range.pony layout (0-indexed):
    line 0-20:  package docstring
    line 21:    (blank)
    line 22:    class SelectionRange
    line 23-25: class docstring
    line 26:      fun value(x: U32): U32 =>
    line 27:        x
    line 28:    (blank)
    line 29:      fun compute(a: U32, b: U32): U32 =>
    line 30:        a + b
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "selection_range/integration/token"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "selection_range/selection_range.pony",
      [ (27, 4,
        _SelectionRangeChecker(recover val
          [as _SrCheck: (27, 4, 3, (27, 4, 27, 5))]
        end))])

class \nodoc\ iso _SelectionRangeMethodTest is UnitTest
  """
  Cursor on `a` at line 30, character 4 (0-indexed) in `fun compute(...)`.
  Expects a chain of at least 3 SelectionRange entries, innermost exactly
  (30,4)-(30,5).

  selection_range.pony layout (0-indexed):
    line 30:     a + b
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "selection_range/integration/method"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "selection_range/selection_range.pony",
      [ (30, 4,
        _SelectionRangeChecker(recover val
          [as _SrCheck: (30, 4, 3, (30, 4, 30, 5))]
        end))])

class \nodoc\ iso _SelectionRangeWhitespaceTest is UnitTest
  """
  Cursor on blank line 28, character 0 (0-indexed). No AST node at that
  position — expects a JSON null entry in the response array.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "selection_range/integration/whitespace"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "selection_range/selection_range.pony",
      [ (28, 0,
        _SelectionRangeChecker(recover val
          [as _SrCheck: (28, 0, 0, None)]
        end))])

class \nodoc\ iso _SelectionRangeKeywordTest is UnitTest
  """
  Cursor on the `fun` keyword at line 26, character 2 (0-indexed) in
  `  fun value(x: U32): U32 =>`. The cursor is on a keyword token rather
  than an identifier or expression — exercises the keyword-node code path.
  Expects a chain of at least 3 SelectionRange entries (fun → class → module).
  The innermost range is not pinned: declaration keyword nodes use end_pos()
  for their extent and the exact LSP span depends on ponyc AST internals.

  selection_range.pony layout (0-indexed):
    line 26:      fun value(x: U32): U32 =>
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "selection_range/integration/keyword"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "selection_range/selection_range.pony",
      [ (26, 2,
        _SelectionRangeChecker(recover val
          [as _SrCheck: (26, 2, 3, None)]
        end))])

class \nodoc\ iso _SelectionRangePositionsArrayTest is UnitTest
  """
  Sends two positions in a single textDocument/selectionRange request.

  Position 0: cursor on `x` at line 27, character 4 → expects a non-null
  SelectionRange chain (the production code path).
  Position 1: cursor on blank line 28, character 0 → expects JSON null.

  This exercises:
  - The `"positions"` array parsing branch in
    workspace_manager.be selection_range
  - The parallel response array (one entry per input position, in order)
  - That a no-node position produces null without dropping the entry
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "selection_range/integration/positions_array"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "selection_range/selection_range.pony",
      [ (27, 4,
        _SelectionRangeChecker(recover val
          [as _SrCheck: (27, 4, 3, (27, 4, 27, 5)); (28, 0, 0, None)]
        end))])

class \nodoc\ iso _SelectionRangeEmptyPositionsTest is UnitTest
  """
  Sends an empty `"positions"` array. Expects an empty response array `[]`.

  Exercises the zero-position path in workspace_manager.be selection_range:
  the positions loop runs zero iterations and the response is an empty
  JsonArray rather than null.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "selection_range/integration/empty_positions"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "selection_range/selection_range.pony",
      [ (0, 0,
        _SelectionRangeChecker(recover val
          Array[_SrCheck]
        end))])

// (line, character, min_depth, expected_innermost)
// min_depth = 0: expect a null response entry (no AST node at this position)
// min_depth > 0: expect a chain of >= min_depth entries with optional
// exact innermost (startLine, startChar, endLine, endChar)
type _SrCheck is (I64, I64, USize, (None | (I64, I64, I64, I64)))

class val _SelectionRangeChecker
  """
  Validates a textDocument/selectionRange response.

  Each entry in `_checks` corresponds to one requested position. Positions are
  sent via `lsp_extra_params()` and responses are validated in order:
  - min_depth = 0: expect a null entry (no AST node at that position).
  - min_depth > 0: expect a SelectionRange chain of at least that depth, with
    monotonically expanding ranges and an optional exact innermost check.
  """
  let _checks: Array[_SrCheck] val

  new val create(checks': Array[_SrCheck] val) =>
    _checks = checks'

  fun lsp_method(): String =>
    Methods.text_document().selection_range()

  fun lsp_range(): (None | (I64, I64, I64, I64)) =>
    None

  fun lsp_context(): (None | JsonObject) =>
    None

  fun lsp_extra_params(): (None | JsonObject) =>
    var positions = JsonArray
    for c in _checks.values() do
      positions =
        positions.push(
          JsonObject.update("line", c._1).update("character", c._2))
    end
    JsonObject.update("positions", positions)

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    try
      let result_arr = res.result as JsonArray
      if not h.assert_true(
        result_arr.size() == _checks.size(),
        "expected " + _checks.size().string() + " response entries, got " +
        result_arr.size().string())
      then
        return false
      end
      var ok = true
      for (i, entry_check) in _checks.pairs() do
        let entry = result_arr(i)?
        let min_depth = entry_check._3
        if min_depth == 0 then
          // Expect null — no AST node at this position.
          if not h.assert_true(
            entry is None,
            "positions[" + i.string() + "]: expected null, got: " +
            entry.string())
          then
            ok = false
          end
        else
          // Expect a SelectionRange chain.
          let chain = _flatten_chain(entry)
          if not h.assert_true(
            chain.size() >= min_depth,
            "positions[" + i.string() + "]: expected chain depth >= " +
            min_depth.string() + ", got " + chain.size().string() +
            "\n" + _describe_chain(chain))
          then
            ok = false
          end
          // Innermost range: verify the exact token extent when expected.
          match entry_check._4
          | (let esl: I64, let esc: I64, let eel: I64, let eec: I64) =>
            try
              let inner = chain(0)?
              if not h.assert_true(
                (inner._1 == esl) and (inner._2 == esc) and
                (inner._3 == eel) and (inner._4 == eec),
                "positions[" + i.string() + "]: expected innermost " +
                _range_str(esl, esc, eel, eec) +
                ", got " + _range_str(inner._1, inner._2, inner._3, inner._4))
              then
                ok = false
              end
            end
          end
          // Monotonic containment: each inner range must fit inside its parent.
          var j: USize = 0
          while (j + 1) < chain.size() do
            try
              let inner = chain(j)?
              let outer = chain(j + 1)?
              let start_ok =
                (outer._1 < inner._1) or
                ((outer._1 == inner._1) and (outer._2 <= inner._2))
              let end_ok =
                (outer._3 > inner._3) or
                ((outer._3 == inner._3) and (outer._4 >= inner._4))
              if not h.assert_true(
                start_ok and end_ok,
                "positions[" + i.string() + "] range " + j.string() +
                " not contained in parent " + (j + 1).string() + ": " +
                _range_str(inner._1, inner._2, inner._3, inner._4) +
                " vs " + _range_str(outer._1, outer._2, outer._3, outer._4))
              then
                ok = false
              end
              // Deduplication: each entry must differ from its parent.
              if not h.assert_true(
                (inner._1 != outer._1) or (inner._2 != outer._2) or
                (inner._3 != outer._3) or (inner._4 != outer._4),
                "positions[" + i.string() + "] range " + j.string() +
                " has identical span to parent " + (j + 1).string() + ": " +
                _range_str(inner._1, inner._2, inner._3, inner._4))
              then
                ok = false
              end
            end
            j = j + 1
          end
        end
      end
      ok
    else
      h.fail("Expected selectionRange JsonArray, got: " + res.string())
      false
    end

  fun _flatten_chain(val': JsonValue): Array[(I64, I64, I64, I64)] =>
    """
    Walk the linked-list { range, parent? } structure and return a flat array
    of (startLine, startChar, endLine, endChar) tuples, innermost first.
    """
    let result: Array[(I64, I64, I64, I64)] = Array[(I64, I64, I64, I64)]
    var current: JsonValue = val'
    while true do
      match current
      | let obj: JsonObject =>
        try
          let sl = JsonNav(obj)("range")("start")("line").as_i64()?
          let sc = JsonNav(obj)("range")("start")("character").as_i64()?
          let el = JsonNav(obj)("range")("end")("line").as_i64()?
          let ec = JsonNav(obj)("range")("end")("character").as_i64()?
          result.push((sl, sc, el, ec))
          current = obj("parent")?
        else
          break
        end
      else
        break
      end
    end
    result

  fun _describe_chain(chain: Array[(I64, I64, I64, I64)]): String val =>
    var s: String val = ""
    for (i, entry) in chain.pairs() do
      s = s + "  [" + i.string() + "] " +
        _range_str(entry._1, entry._2, entry._3, entry._4) + "\n"
    end
    s

  fun _range_str(sl: I64, sc: I64, el: I64, ec: I64): String val =>
    recover val
      "(" + sl.string() + ":" + sc.string() +
      ")-(" + el.string() + ":" + ec.string() + ")"
    end
