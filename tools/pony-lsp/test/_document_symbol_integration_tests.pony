use ".."
use "pony_test"
use "files"
use "json"

primitive _DocumentSymbolIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let server = _LspTestServer(workspace_dir)
    let fixture = "references/referenced_class.pony"
    test(_DocSymContainmentTest.create(server, fixture))
    test(_DocSymRangeTest.create(server, fixture))

class \nodoc\ iso _DocSymContainmentTest is UnitTest
  """
  Regression guard for #5240 follow-up: every symbol's selectionRange must
  be contained within its range. Checks the top-level symbols and all
  nested children recursively.
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_symbol/integration/containment"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [(0, 0, _DocSymContainmentChecker)])

class \nodoc\ iso _DocSymRangeTest is UnitTest
  """
  "class ReferencedClass" — full `range` should span from the `class`
  keyword on line 0 past the class body (at least through line 21 where
  `fun ref increment` is declared), distinguishing it from the
  keyword-only span that would end at column 5.

  "fun ref increment" — full `range` should span from the `fun` keyword
  on line 21 through the method body (at least line 22).
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_symbol/integration/range"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ ( 0, 0,
          _DocSymRangeChecker(
            "ReferencedClass",
            None,
            // range.start (line, char); selectionRange.start (line, char)
            (0, 0, 0, 6),
            21))
        ( 0, 0,
          _DocSymRangeChecker(
            "increment",
            "ReferencedClass",
            (21, 2, 21, 10),
            22))])

class val _DocSymContainmentChecker
  """
  Validates that every DocumentSymbol (and every nested child) in a
  textDocument/documentSymbol response has selectionRange contained
  within range.
  """
  new val create() => None

  fun lsp_method(): String =>
    Methods.text_document().document_symbol()

  fun lsp_range(): (None | (I64, I64, I64, I64)) =>
    None

  fun lsp_context(): (None | JsonObject) =>
    None

  fun lsp_extra_params(): (None | JsonObject) =>
    None

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    match res.result
    | let arr: JsonArray =>
      var ok = true
      for item in arr.values() do
        ok = _check_symbol(item, h) and ok
      end
      ok
    else
      h.fail("documentSymbol: expected array result, got null")
      false
    end

  fun _check_symbol(item: JsonValue, h: TestHelper): Bool =>
    var ok = true
    try
      let name = JsonNav(item)("name").as_string()?
      let r_sl = JsonNav(item)("range")("start")("line").as_i64()?
      let r_sc = JsonNav(item)("range")("start")("character").as_i64()?
      let r_el = JsonNav(item)("range")("end")("line").as_i64()?
      let r_ec = JsonNav(item)("range")("end")("character").as_i64()?
      let s_sl = JsonNav(item)("selectionRange")("start")("line").as_i64()?
      let s_sc = JsonNav(item)("selectionRange")("start")("character").as_i64()?
      let s_el = JsonNav(item)("selectionRange")("end")("line").as_i64()?
      let s_ec = JsonNav(item)("selectionRange")("end")("character").as_i64()?

      // selectionRange.start must be >= range.start
      let start_ok =
        (s_sl > r_sl) or ((s_sl == r_sl) and (s_sc >= r_sc))
      if not h.assert_true(
        start_ok,
        "documentSymbol '" + name +
        "': selectionRange.start (" + s_sl.string() + ":" + s_sc.string() +
        ") must be >= range.start (" + r_sl.string() + ":" + r_sc.string() +
        ")")
      then
        ok = false
      end

      // selectionRange.end must be <= range.end
      let end_ok =
        (s_el < r_el) or ((s_el == r_el) and (s_ec <= r_ec))
      if not h.assert_true(
        end_ok,
        "documentSymbol '" + name +
        "': selectionRange.end (" + s_el.string() + ":" + s_ec.string() +
        ") must be <= range.end (" + r_el.string() + ":" + r_ec.string() +
        ")")
      then
        ok = false
      end

      // Recurse into children if present.
      try
        let children = JsonNav(item)("children").as_array()?
        for child in children.values() do
          ok = _check_symbol(child, h) and ok
        end
      end
    else
      h.fail("documentSymbol: malformed symbol entry")
      ok = false
    end
    ok

class val _DocSymRangeChecker
  """
  Validates range/selectionRange for a named symbol (optionally nested
  under a parent) in a documentSymbol response.

  - positions: (range.start.line, range.start.char,
      selectionRange.start.line, selectionRange.start.char)
  - min_end_line: asserts range.end.line >= this value, proving the full
    declaration is covered rather than just the keyword.
  """
  let _symbol_name: String
  let _parent_name: (String | None)
  let _positions: (I64, I64, I64, I64)
  let _min_end_line: I64

  new val create(
    symbol_name: String,
    parent_name: (String | None),
    positions: (I64, I64, I64, I64),
    min_end_line: I64)
  =>
    _symbol_name = symbol_name
    _parent_name = parent_name
    _positions = positions
    _min_end_line = min_end_line

  fun lsp_method(): String =>
    Methods.text_document().document_symbol()

  fun lsp_range(): (None | (I64, I64, I64, I64)) =>
    None

  fun lsp_context(): (None | JsonObject) =>
    None

  fun lsp_extra_params(): (None | JsonObject) =>
    None

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    match res.result
    | let arr: JsonArray =>
      let found = _find(arr, _symbol_name, _parent_name)
      match found
      | None =>
        h.fail(
          "documentSymbol: symbol '" + _symbol_name +
          "' not found in response")
        false
      else
        _assert_ranges(found, h)
      end
    else
      h.fail("documentSymbol: expected array result, got null")
      false
    end

  fun _find(
    arr: JsonArray,
    name: String,
    parent: (String | None))
    : JsonValue
  =>
    for item in arr.values() do
      try
        let item_name = JsonNav(item)("name").as_string()?
        match \exhaustive\ parent
        | None =>
          if item_name == name then return item end
        | let p: String =>
          if item_name == p then
            try
              let children = JsonNav(item)("children").as_array()?
              for child in children.values() do
                try
                  if JsonNav(child)("name").as_string()? == name then
                    return child
                  end
                end
              end
            end
          end
        end
      end
    end
    None

  fun _assert_ranges(item: JsonValue, h: TestHelper): Bool =>
    (let exp_r_sl, let exp_r_sc, let exp_s_sl, let exp_s_sc) = _positions
    var ok = true
    try
      let r_sl = JsonNav(item)("range")("start")("line").as_i64()?
      let r_sc = JsonNav(item)("range")("start")("character").as_i64()?
      let r_el = JsonNav(item)("range")("end")("line").as_i64()?
      let s_sl = JsonNav(item)("selectionRange")("start")("line").as_i64()?
      let s_sc = JsonNav(item)("selectionRange")("start")("character").as_i64()?

      ok = h.assert_eq[I64](
        exp_r_sl,
        r_sl,
        _symbol_name + ": range.start.line") and ok
      ok = h.assert_eq[I64](
        exp_r_sc,
        r_sc,
        _symbol_name + ": range.start.character") and ok
      ok = h.assert_eq[I64](
        exp_s_sl,
        s_sl,
        _symbol_name + ": selectionRange.start.line") and ok
      ok = h.assert_eq[I64](
        exp_s_sc,
        s_sc,
        _symbol_name + ": selectionRange.start.character") and ok
      ok = h.assert_true(
        r_el >= _min_end_line,
        _symbol_name +
        ": range.end.line (" + r_el.string() +
        ") should be >= " + _min_end_line.string() +
        " (full declaration, not keyword-only)") and ok
    else
      h.fail(_symbol_name + ": malformed symbol entry")
      ok = false
    end
    ok
