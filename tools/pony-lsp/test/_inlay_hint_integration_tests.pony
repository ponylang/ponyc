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
    test(_InlayHintExplicitReturnTypeTest.create(server))
    test(_InlayHintMultilineReturnTypeTest.create(server))
    test(_InlayHintGenericsTest.create(server))
    test(_InlayHintGenericFieldsTest.create(server))
    test(_InlayHintUnionTupleTest.create(server))
    test(_InlayHintBeNewExclusionTest.create(server))
    test(_InlayHintExplicitReceiverCapTest.create(server))
    test(_InlayHintSyntheticNameExclusionTest.create(server))

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
            [ (6, 5, " box")                     // demo receiver cap
              (6, 20, " val")                    // demo return type String cap
              (8, 23, ": String val")            // inferred_string
              (10, 21, ": Bool val")             // inferred_bool
              (12, 31, " val")                   // explicit_string cap
              (18, 5, " box")                    // inferred_fun receiver cap
              (18, 20, ": None val")             // inferred_fun return type
              (22, 5, " box")                    // explicit_fun receiver cap
              (22, 28, " val")                   // explicit_fun return type cap
              (26, 5, " box")                    // multiline receiver cap
              (27, 12, " val")                   // multiline return type cap
              (31, 24, ": None val")             // explicit_cap return type
              (35, 5, " box")                    // for_loop receiver cap
              (35, 16, ": None val")             // for_loop return type
              (36, 27, " val")                   // String cap in Array[String]
              (36, 28, " ref")                   // Array cap in Array[String]
              (37, 12, ": String val") ]         // item type in for loop
            )) ])

class \nodoc\ iso _InlayHintRangeFirstLineTest is UnitTest
  """
  Range covering only line 8 (inferred_string). Expects exactly one hint.
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
            [ (8, 23, ": String val") ]
            where range = (8, 0, 9, 0))) ])

class \nodoc\ iso _InlayHintRangeTwoLineTest is UnitTest
  """
  Range covering lines 8-10. Expects exactly two hints.
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
            [ (8, 23, ": String val")            // inferred_string
              (10, 21, ": Bool val") ]           // inferred_bool
            where range = (8, 0, 11, 0))) ])

class \nodoc\ iso _InlayHintRangeEmptyTest is UnitTest
  """
  Range covering only the class header and docstring (lines 0-5).
  No hints expected.
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
            where range = (0, 0, 6, 0))) ])

class \nodoc\ iso _InlayHintExplicitReturnTypeTest is UnitTest
  """
  Range covering explicit_fun(): String. Expects receiver cap
  and return type cap hints.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "inlay_hint/integration/explicit_return_type"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "inlay_hint/_inlay_hint.pony",
      [ (0, 0,
          _InlayHintChecker(
            [ (22, 5, " box")    // receiver cap
              (22, 28, " val") ] // return type String cap
            where range = (22, 0, 24, 0))) ])

class \nodoc\ iso _InlayHintMultilineReturnTypeTest is UnitTest
  """
  Range covering explicit_multiline() where ': String' is on the second line.
  Expects receiver cap hint on the function line and return type cap on the
  type line.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "inlay_hint/integration/multiline_return_type"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "inlay_hint/_inlay_hint.pony",
      [ (0, 0,
          _InlayHintChecker(
            [ (26, 5, " box")    // receiver cap
              (27, 12, " val") ] // return type String cap on second line
            where range = (26, 0, 29, 0))) ])

class \nodoc\ iso _InlayHintGenericsTest is UnitTest
  """
  Tests cap hints for nested generic annotations.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "inlay_hint/integration/generics"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "inlay_hint/_generics.pony",
      [ (0, 0,
          _InlayHintChecker(
            [ (4, 5, " box")             // demo receiver cap
              (4, 19, " val")            // demo return type USize cap
              (6, 26, " val")            // arr_u32: U32
              (6, 27, " ref")            // arr_u32: Array
              (8, 31, " val")            // nested: U32
              (8, 32, " ref")            // nested: inner Array
              (8, 33, " ref")            // nested: outer Array
              (10, 32, " val")           // partial: U32
              (10, 38, " ref")           // partial: outer Array
              (12, 29, " val")           // full: U32 (other caps explicit)
              (14, 16, ": Array[U32 val] ref") // inferred: full inferred type
              (16, 41, " ref")           // primitive_partial: outer Array cap
              (19, 9, " val")            // multiline_arr: U32 (on its own line)
              (20, 5, " ref") ] // multiline_arr: Array (after ] on next line)
            where range = (0, 0, 24, 0))) ])

class \nodoc\ iso _InlayHintGenericFieldsTest is UnitTest
  """
  Tests cap hints for class fields and function return type
  annotations with generics.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "inlay_hint/integration/generic_fields"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "inlay_hint/_generics.pony",
      [ (0, 0,
          _InlayHintChecker(
            [ (29, 22, " val")           // items: U32
              (29, 23, " ref")           // items: Array
              (31, 29, " val")           // nested field: U32
              (31, 30, " ref")           // nested field: inner Array
              (31, 31, " ref")           // nested field: outer Array
              (33, 27, " val")           // embed embedded: U32
              (33, 28, " ref")           // embed embedded: Array
              (41, 5, " box")            // make_items receiver cap
              (41, 29, " val")           // make_items return: U32
              (41, 30, " ref")           // make_items return: Array
              (45, 5, " box")            // make_nested receiver cap
              (45, 36, " val")           // make_nested return: U32
              (45, 37, " ref")           // make_nested return: inner Array
              (45, 38, " ref") ]         // make_nested return: outer Array
            where range = (24, 0, 47, 0))) ])

class \nodoc\ iso _InlayHintUnionTupleTest is UnitTest
  """
  Tests cap hints for union, tuple, and intersection type annotations,
  verifying that _add_nominal_hints recurses into tk_uniontype,
  tk_tupletype, and tk_isecttype.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "inlay_hint/integration/union_tuple"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "inlay_hint/_union_types.pony",
      [ (0, 0,
          _InlayHintChecker(
            [ (4, 5, " box")            // demo receiver cap
              (4, 12, ": None val")     // demo inferred return type
              (6, 15, " val")           // u: U32 cap
              (6, 22, " val")           // u: None cap
              (8, 15, " val")           // t: U32 cap
              (8, 21, " val")           // t: Bool cap
              (10, 16, " val")          // i: None cap
              (10, 29, " box") ]        // i: Stringable cap
            where range = (0, 0, 12, 0))) ])

class \nodoc\ iso _InlayHintBeNewExclusionTest is UnitTest
  """
  Verifies that behaviours (be) and constructors (new) produce no hints —
  neither receiver cap hints nor return type hints.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "inlay_hint/integration/be_new_exclusion"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "inlay_hint/_union_types.pony",
      [ (0, 0,
          _InlayHintChecker(
            []
            where range = (13, 0, 20, 0))) ])

class \nodoc\ iso _InlayHintExplicitReceiverCapTest is UnitTest
  """
  Verifies that an explicit receiver capability keyword suppresses the
  receiver cap hint. `fun ref explicit_cap()` should produce only a return
  type hint, not a receiver cap hint.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "inlay_hint/integration/explicit_receiver_cap"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "inlay_hint/_inlay_hint.pony",
      [ (0, 0,
          _InlayHintChecker(
            [ (31, 24, ": None val") ] // return type only; no receiver cap hint
            where range = (31, 0, 33, 0))) ])

class \nodoc\ iso _InlayHintSyntheticNameExclusionTest is UnitTest
  """
  Verifies that synthetic compiler-generated names (prefixed with `$`) do not
  produce inlay hints. For-loop desugaring introduces a `$`-named iterator
  variable; only the user-visible `item` binding should receive a hint.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "inlay_hint/integration/synthetic_name_exclusion"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "inlay_hint/_inlay_hint.pony",
      [ (0, 0,
          _InlayHintChecker(
            [ (37, 12, ": String val") ] // item hint; no hint for $iterator
            where range = (37, 0, 39, 0))) ])

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

  fun lsp_context(): (None | JsonObject) =>
    None

  fun lsp_extra_params(): (None | JsonObject) =>
    None

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
        if not h.assert_true(
          found,
          "No hint found at " + exp_line.string() + ":" + exp_char.string())
        then
          ok = false
        end
      end
    else
      h.fail("Failed to parse inlay hint response: " + res.string())
      ok = false
    end
    ok
