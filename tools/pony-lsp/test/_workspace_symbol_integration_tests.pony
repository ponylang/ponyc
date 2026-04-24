use ".."
use "pony_test"
use "files"
use "json"

primitive _WorkspaceSymbolIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let server = _LspTestServer(workspace_dir)
    // All fixtures live under workspace/workspace_symbol/ so the suite
    // compiles a single package once.
    test(_WsSymExactMatchTest.create(server))
    test(_WsSymSubstringMatchTest.create(server))
    test(_WsSymSubstringInMiddleTest.create(server))
    test(_WsSymCaseInsensitiveTest.create(server))
    test(_WsSymMemberTest.create(server))
    test(_WsSymNoMatchTest.create(server))
    test(_WsSymEmptyQueryTest.create(server))
    test(_WsSymRangeTest.create(server))

class \nodoc\ iso _WsSymExactMatchTest is UnitTest
  """
  Query "_WsSymHost" → exactly 1 top-level result named "_WsSymHost"
  with symbol kind 5 (class; actors are reported as sk_class).
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "workspace_symbol/integration/exact_match"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "workspace_symbol/_ws_sym_host.pony",
      [ ( 0, 0,
          _WsSymChecker(
            "_WsSymHost",
            [("_WsSymHost", 5, "_ws_sym_host.pony", None)]))])

class \nodoc\ iso _WsSymSubstringMatchTest is UnitTest
  """
  Query "_WsSym" → prefix substring matches both top-level entities
  `_WsSymHost` (actor) and `_WsSymInner` (class).
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "workspace_symbol/integration/substring_match"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "workspace_symbol/_ws_sym_host.pony",
      [ ( 0, 0,
          _WsSymChecker(
            "_WsSym",
            [ ("_WsSymHost", 5, "_ws_sym_host.pony", None)
              ("_WsSymInner", 5, "_ws_sym_host.pony", None)]))])

class \nodoc\ iso _WsSymSubstringInMiddleTest is UnitTest
  """
  Query "Host" → matches `_WsSymHost` via substring in the middle.
  Distinguishes substring matching from prefix-only matching.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "workspace_symbol/integration/substring_in_middle"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "workspace_symbol/_ws_sym_host.pony",
      [ ( 0, 0,
          _WsSymChecker(
            "Host",
            [("_WsSymHost", 5, "_ws_sym_host.pony", None)]))])

class \nodoc\ iso _WsSymCaseInsensitiveTest is UnitTest
  """
  Query "_wssymhost" (all lower-case) → still matches `_WsSymHost`.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "workspace_symbol/integration/case_insensitive"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "workspace_symbol/_ws_sym_host.pony",
      [ ( 0, 0,
          _WsSymChecker(
            "_wssymhost",
            [("_WsSymHost", 5, "_ws_sym_host.pony", None)]))])

class \nodoc\ iso _WsSymMemberTest is UnitTest
  """
  Query "increment" → matches the method `increment` inside `_WsSymHost`,
  which should appear with containerName "_WsSymHost".
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "workspace_symbol/integration/member_match"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "workspace_symbol/_ws_sym_host.pony",
      [ ( 0, 0,
          _WsSymChecker(
            "increment",
            [ ("increment", 6, "_ws_sym_host.pony",
                "_WsSymHost")]))])

class \nodoc\ iso _WsSymNoMatchTest is UnitTest
  """
  Query "zzznomatch" → empty result array.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "workspace_symbol/integration/no_match"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "workspace_symbol/_ws_sym_host.pony",
      [(0, 0, _WsSymChecker("zzznomatch", []))])

class \nodoc\ iso _WsSymEmptyQueryTest is UnitTest
  """
  Empty query → all symbols from the fixture: the top-level actor and
  class, plus every explicitly written member (var/let/embed fields,
  constructor, fun, be). Synthesized constructors on bare classes are
  excluded — `_WsSymInner` has no explicit `new` so its ponyc-generated
  `create` does not appear.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "workspace_symbol/integration/empty_query"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "workspace_symbol/_ws_sym_host.pony",
      [ ( 0, 0,
          _WsSymChecker(
            "",
            [ ("_WsSymHost", 5, "_ws_sym_host.pony", None)
              ("_count", 8, "_ws_sym_host.pony", "_WsSymHost")
              ("_name", 8, "_ws_sym_host.pony", "_WsSymHost")
              ("_inner", 8, "_ws_sym_host.pony", "_WsSymHost")
              ("create", 9, "_ws_sym_host.pony", "_WsSymHost")
              ("increment", 6, "_ws_sym_host.pony", "_WsSymHost")
              ("ping", 6, "_ws_sym_host.pony", "_WsSymHost")
              ("_WsSymInner", 5, "_ws_sym_host.pony", None)]))])

class \nodoc\ iso _WsSymRangeTest is UnitTest
  """
  Verifies that workspace/symbol returns the full declaration range for
  every member and top-level token kind. Each check asserts the exact
  LSP range (start_line, start_char, end_line, end_char), not just the
  start column — a degenerate `{0,0}-{0,0}` response would pass a one-
  coordinate check.

  Exact position tuples are derived from the fixture layout in
  `workspace_symbol/_ws_sym_host.pony`; see the comment at the top of
  that file for the coupling note.

  Coverage map (symbol → token kind asserted):
    _WsSymHost   → tk_actor
    _count       → tk_fvar
    _name        → tk_flet
    _inner       → tk_embed
    create       → tk_new
    increment    → tk_fun
    ping         → tk_be
    _WsSymInner  → tk_class
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "workspace_symbol/integration/range"

  fun apply(h: TestHelper) =>
    // Distinct dummy (line, character) per check so the pony_test
    // action strings are unique — failure diagnostics will point at the
    // specific assertion rather than all sharing one action id.
    _RunLspChecks(
      h,
      _server,
      "workspace_symbol/_ws_sym_host.pony",
      [ ( 0, 0,
          _WsSymRangeChecker(
            "_WsSymHost", "_WsSymHost", None, (5, 0, 18, 10)))
        ( 0, 1,
          _WsSymRangeChecker(
            "_count", "_count", "_WsSymHost", (6, 2, 6, 17)))
        ( 0, 2,
          _WsSymRangeChecker(
            "_name", "_name", "_WsSymHost", (7, 2, 7, 19)))
        ( 0, 3,
          _WsSymRangeChecker(
            "_inner", "_inner", "_WsSymHost", (8, 2, 8, 27)))
        ( 0, 4,
          _WsSymRangeChecker(
            "create", "create", "_WsSymHost", (10, 2, 11, 14)))
        ( 0, 5,
          _WsSymRangeChecker(
            "increment", "increment", "_WsSymHost", (13, 2, 15, 10)))
        ( 0, 6,
          _WsSymRangeChecker(
            "ping", "ping", "_WsSymHost", (17, 2, 18, 10)))
        ( 0, 7,
          _WsSymRangeChecker(
            "_WsSymInner", "_WsSymInner", None, (20, 0, 20, 17)))])

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

class val _WsSymRangeChecker
  """
  Validates that a named symbol in a workspace/symbol response has a
  location.range equal to the expected (start_line, start_char, end_line,
  end_char) tuple. Asserting all four coordinates catches regressions
  that collapse the range to `{0,0}-{0,0}` or any other degenerate span
  — a single-coordinate check would be silently satisfied by several
  wrong values.

  The optional `container` parameter disambiguates symbols whose names
  are not unique within a fixture (e.g. two `create` constructors).
  When `None`, any symbol with the matching name is accepted.
  """
  let _query: String
  let _symbol_name: String
  let _container: (String | None)
  let _expected: (I64, I64, I64, I64)

  new val create(
    query: String,
    symbol_name: String,
    container: (String | None),
    expected: (I64, I64, I64, I64))
  =>
    _query = query
    _symbol_name = symbol_name
    _container = container
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
    (let exp_sl, let exp_sc, let exp_el, let exp_ec) = _expected
    match res.result
    | let arr: JsonArray =>
      for item in arr.values() do
        try
          let container: (String | None) =
            try JsonNav(item)("containerName").as_string()? else None end
          let container_ok =
            match (_container, container)
            | (None, None) => true
            | (let a: String, let b: String) => a == b
            else false
            end
          let name_ok =
            JsonNav(item)("name").as_string()? == _symbol_name
          if name_ok and container_ok then
            let r = JsonNav(item)("location")("range")
            let sl = r("start")("line").as_i64()?
            let sc = r("start")("character").as_i64()?
            let el = r("end")("line").as_i64()?
            let ec = r("end")("character").as_i64()?
            var ok =
              h.assert_eq[I64](
                exp_sl, sl, _symbol_name + ": range.start.line")
            ok = h.assert_eq[I64](
              exp_sc, sc, _symbol_name + ": range.start.character") and ok
            ok = h.assert_eq[I64](
              exp_el, el, _symbol_name + ": range.end.line") and ok
            ok = h.assert_eq[I64](
              exp_ec, ec, _symbol_name + ": range.end.character") and ok
            return ok
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
