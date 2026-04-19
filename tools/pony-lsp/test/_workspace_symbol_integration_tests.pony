use ".."
use "pony_test"
use "files"
use "json"

primitive _WorkspaceSymbolIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let server = _LspTestServer(workspace_dir)
    let fixture = "references/referenced_class.pony"
    test(_WsSymExactMatchTest.create(server, fixture))
    test(_WsSymSubstringMatchTest.create(server, fixture))
    test(_WsSymSubstringInMiddleTest.create(server, fixture))
    test(_WsSymCaseInsensitiveTest.create(server, fixture))
    test(_WsSymMemberTest.create(server, fixture))
    test(_WsSymNoMatchTest.create(server, fixture))
    test(_WsSymEmptyQueryTest.create(server, fixture))
    test(_WsSymRangeTest.create(server, fixture))

class \nodoc\ iso _WsSymExactMatchTest is UnitTest
  """
  Query "ReferencedClass" → exactly 1 top-level result named "ReferencedClass"
  with symbol kind 5 (class).
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "workspace_symbol/integration/exact_match"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ ( 0, 0,
          _WsSymChecker(
            "ReferencedClass",
            [("ReferencedClass", 5, "referenced_class.pony", None)]))])

class \nodoc\ iso _WsSymSubstringMatchTest is UnitTest
  """
  Query "Referenced" → matches "ReferencedClass" only (prefix substring).
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "workspace_symbol/integration/substring_match"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ ( 0, 0,
          _WsSymChecker(
            "Referenced",
            [("ReferencedClass", 5, "referenced_class.pony", None)]))])

class \nodoc\ iso _WsSymSubstringInMiddleTest is UnitTest
  """
  Query "Class" → matches "ReferencedClass" via substring in the middle.
  Distinguishes substring matching from prefix-only matching.
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "workspace_symbol/integration/substring_in_middle"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ ( 0, 0,
          _WsSymChecker(
            "Class",
            [("ReferencedClass", 5, "referenced_class.pony", None)]))])

class \nodoc\ iso _WsSymCaseInsensitiveTest is UnitTest
  """
  Query "referencedclass" (all lower-case) → still matches "ReferencedClass".
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "workspace_symbol/integration/case_insensitive"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ ( 0, 0,
          _WsSymChecker(
            "referencedclass",
            [("ReferencedClass", 5, "referenced_class.pony", None)]))])

class \nodoc\ iso _WsSymMemberTest is UnitTest
  """
  Query "increment" → matches the method "increment" inside "ReferencedClass",
  which should appear with containerName "ReferencedClass".
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "workspace_symbol/integration/member_match"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ ( 0, 0,
          _WsSymChecker(
            "increment",
            [ ("increment", 6, "referenced_class.pony",
                "ReferencedClass")]))])

class \nodoc\ iso _WsSymNoMatchTest is UnitTest
  """
  Query "zzznomatch" → empty result array.
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "workspace_symbol/integration/no_match"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [(0, 0, _WsSymChecker("zzznomatch", []))])

class \nodoc\ iso _WsSymEmptyQueryTest is UnitTest
  """
  Empty query → all symbols from the fixture file are returned: the
  top-level class, its field, constructor, and two methods.
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "workspace_symbol/integration/empty_query"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ ( 0, 0,
          _WsSymChecker(
            "",
            [ ("ReferencedClass", 5, "referenced_class.pony", None)
              ("_count", 8, "referenced_class.pony", "ReferencedClass")
              ("create", 9, "referenced_class.pony", "ReferencedClass")
              ("increment", 6, "referenced_class.pony", "ReferencedClass")
              ("maybe", 6, "referenced_class.pony", "ReferencedClass")]))])

class val _WsSymChecker
  """
  Validates a workspace/symbol response.

  Each expected entry is (name, kind, filename_basename, containerName|None).
  The checker verifies that each expected symbol appears in the response and
  that the total count matches.
  """
  let _query: String
  let _expected: Array[(String, I64, String, (String | None))] val

  new val create(
    query: String,
    expected: Array[(String, I64, String, (String | None))] val)
  =>
    _query = query
    _expected = expected

  fun lsp_method(): String =>
    Methods.workspace().symbol()

  fun lsp_range(): (None | (I64, I64, I64, I64)) =>
    None

  fun lsp_context(): (None | JsonObject) =>
    None

  fun lsp_extra_params(): (None | JsonObject) =>
    JsonObject.update("query", _query)

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    var ok = true
    match res.result
    | let arr: JsonArray =>
      let got = arr.size()
      let want = _expected.size()
      if not h.assert_eq[USize](
        want,
        got,
        "workspace/symbol '" + _query + "': expected " +
        want.string() + " result(s), got " + got.string())
      then
        ok = false
        for item in arr.values() do
          try
            let n = JsonNav(item)("name").as_string()?
            let k = JsonNav(item)("kind").as_i64()?
            let uri = JsonNav(item)("location")("uri").as_string()?
            let file = Path.base(Uris.to_path(uri))
            h.log("  actual: " + n + " (kind " + k.string() + ") in " + file)
          end
        end
      end
      for (exp_name, exp_kind, exp_file, exp_container) in _expected.values() do
        var found = false
        for item in arr.values() do
          try
            let n = JsonNav(item)("name").as_string()?
            let k = JsonNav(item)("kind").as_i64()?
            let uri = JsonNav(item)("location")("uri").as_string()?
            let file = Path.base(Uris.to_path(uri))
            let container: (String | None) =
              try JsonNav(item)("containerName").as_string()? else None end
            let container_ok =
              match (exp_container, container)
              | (None, None) => true
              | (let a: String, let b: String) => a == b
              else false
              end
            if (n == exp_name) and (k == exp_kind) and
              (file == exp_file) and container_ok
            then
              found = true
              break
            end
          end
        end
        if not h.assert_true(
          found,
          "workspace/symbol '" + _query + "': expected symbol '" +
          exp_name + "' (kind " + exp_kind.string() + ") in " +
          exp_file + " not found")
        then
          ok = false
        end
      end
    else
      if _expected.size() > 0 then
        ok = false
        h.log(
          "workspace/symbol '" + _query +
          "': expected results but got null/non-array")
      end
    end
    ok

class \nodoc\ iso _WsSymRangeTest is UnitTest
  """
  Verifies that workspace/symbol returns the full declaration range for
  SymbolInformation.location.range, not the identifier-only span.

  "class ReferencedClass" — full declaration starts at char 0 (the
  "class" keyword); the identifier span would start at char 6.

  "  fun ref increment" — full declaration starts at char 2 (the "fun"
  keyword); the identifier span would start at char 10.
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "workspace_symbol/integration/range"

  fun apply(h: TestHelper) =>
    // Query "ReferencedClass": top-level class declared as
    // "class ReferencedClass" — full range starts at char 0, not char 6.
    // Query "increment": member declared as "  fun ref increment" —
    // full range starts at char 2, not char 10.
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ ( 0, 0,
          _WsSymRangeChecker(
            "ReferencedClass", "ReferencedClass", 0))
        ( 0, 0,
          _WsSymRangeChecker("increment", "increment", 2))])

class val _WsSymRangeChecker
  """
  Validates that a named symbol in a workspace/symbol response has a
  location.range.start.character equal to the expected value.
  """
  let _query: String
  let _symbol_name: String
  let _expected_start_char: I64

  new val create(
    query: String,
    symbol_name: String,
    expected_start_char: I64)
  =>
    _query = query
    _symbol_name = symbol_name
    _expected_start_char = expected_start_char

  fun lsp_method(): String =>
    Methods.workspace().symbol()

  fun lsp_range(): (None | (I64, I64, I64, I64)) =>
    None

  fun lsp_context(): (None | JsonObject) =>
    None

  fun lsp_extra_params(): (None | JsonObject) =>
    JsonObject.update("query", _query)

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    match res.result
    | let arr: JsonArray =>
      for item in arr.values() do
        try
          if JsonNav(item)("name").as_string()? == _symbol_name then
            let start_char =
              JsonNav(item)("location")("range")("start")("character").as_i64()?
            return h.assert_eq[I64](
              _expected_start_char,
              start_char,
              _symbol_name +
              ": location.range.start.character should be " +
              _expected_start_char.string())
          end
        end
      end
      h.fail(
        "workspace/symbol '" + _query +
        "': expected symbol '" + _symbol_name + "' not found")
      false
    else
      h.fail(
        "workspace/symbol '" + _query + "': expected array result, got null")
      false
    end
