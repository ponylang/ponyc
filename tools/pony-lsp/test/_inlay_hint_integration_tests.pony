use ".."
use "pony_test"
use "files"
use "json"

primitive _InlayHintIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let server = _LspTestServer(workspace_dir)
    test(_InlayHintDemoTest.create(server))
    test(_InlayHintRangeFirstLineTest.create(server))
    test(_InlayHintRangeTwoLineTest.create(server))
    test(_InlayHintRangeEmptyTest.create(server))

class \nodoc\ iso _InlayHintDemoTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "inlay_hint/integration/full_file"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "inlay_hint/_inlay_hint.pony",
      [ (0, 0,
          _InlayHintChecker(
            [ (15, 23, ": String val")           // inferred_string
              (16, 21, ": Bool")                 // inferred_bool
              (18, 22, ": Array[U32 val] ref") ] // inferred_array
            )) ])

class \nodoc\ iso _InlayHintRangeFirstLineTest is UnitTest
  """
  Range covering only line 15 (inferred_string). Expects exactly one hint.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "inlay_hint/integration/range_first_line"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "inlay_hint/_inlay_hint.pony",
      [ (0, 0,
          _InlayHintChecker(
            [ (15, 23, ": String val") ]
            where range = (15, 0, 16, 0))) ])

class \nodoc\ iso _InlayHintRangeTwoLineTest is UnitTest
  """
  Range covering lines 15-17. Expects exactly two hints.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "inlay_hint/integration/range_middle"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "inlay_hint/_inlay_hint.pony",
      [ (0, 0,
          _InlayHintChecker(
            [ (15, 23, ": String val")           // inferred_string
              (16, 21, ": Bool") ]               // inferred_bool
            where range = (15, 0, 17, 0))) ])

class \nodoc\ iso _InlayHintRangeEmptyTest is UnitTest
  """
  Range covering only the class docstring (lines 0-14). No hints expected.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "inlay_hint/integration/range_empty"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "inlay_hint/_inlay_hint.pony",
      [ (0, 0,
          _InlayHintChecker(
            []
            where range = (0, 0, 15, 0))) ])

class val _InlayHintChecker
  """
  Checks that an inlayHint response contains exactly the expected hints.
  Each entry is (line, character, label_substring). The total hint count
  is asserted against the number of expected entries.
  """
  let _expected: Array[(I64, I64, String)] val
  let _range: (None | (I64, I64, I64, I64))

  new val create(
    expected: Array[(I64, I64, String)] val,
    range: (None | (I64, I64, I64, I64)) = None)
  =>
    _expected = expected
    _range = range

  fun lsp_method(): String =>
    Methods.text_document().inlay_hint()

  fun lsp_range(): (None | (I64, I64, I64, I64)) =>
    _range

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    var ok = true
    try
      let hints = res.result as JsonArray
      if not h.assert_eq[USize](
        _expected.size(),
        hints.size(),
        "Expected " + _expected.size().string() +
        " hints, got " + hints.size().string())
      then
        ok = false
      end
      for (exp_line, exp_char, exp_label) in _expected.values() do
        var found = false
        for hint_val in hints.values() do
          try
            let hint = hint_val as JsonObject
            let hint_line = JsonNav(hint)("position")("line").as_i64()?
            let hint_char = JsonNav(hint)("position")("character").as_i64()?
            if (hint_line == exp_line) and (hint_char == exp_char) then
              let label = JsonNav(hint)("label").as_string()?
              if not h.assert_true(
                label.contains(exp_label),
                "At " + exp_line.string() + ":" + exp_char.string() +
                " expected label to contain '" + exp_label +
                "', got: '" + label + "'")
              then
                ok = false
              end
              found = true
              break
            end
          end
        end
        if not found then
          h.log(
            "No hint found at " + exp_line.string() + ":" + exp_char.string())
          ok = false
        end
      end
    else
      h.log("Failed to parse inlay hint response: " + res.string())
      ok = false
    end
    ok
