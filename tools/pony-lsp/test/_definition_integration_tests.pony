use ".."
use "pony_test"
use "files"
use "json"
use "collections"

primitive _DefinitionIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let server = _LspTestServer(workspace_dir)
    test(_DefinitionClassIntegrationTest.create(server))
    test(_DefinitionThisIntegrationTest.create(server))
    test(_DefinitionKeywordsIntegrationTest.create(server))
    test(_DefinitionTraitIntegrationTest.create(server))
    test(_DefinitionUnionIntegrationTest.create(server))
    test(_DefinitionCrossFileIntegrationTest.create(server))
    test(_DefinitionGenericsIntegrationTest.create(server))
    test(_DefinitionTupleIntegrationTest.create(server))
    test(_DefinitionTypeAliasIntegrationTest.create(server))

class \nodoc\ iso _DefinitionClassIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/class"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "definition/_class.pony",
      [ // field usages → field declaration (line 4, "let" keyword span)
        (7, 4, _DefinitionChecker([("_class.pony", (4, 2), (4, 5))]))
        (10, 4, _DefinitionChecker([("_class.pony", (4, 2), (4, 5))]))
        // parameter usage → parameter declaration (line 6, "v: U32" span)
        (7, 13, _DefinitionChecker([("_class.pony", (6, 13), (6, 19))]))
        // method call → method declaration (line 9, "fun" keyword span)
        (13, 9, _DefinitionChecker([("_class.pony", (9, 2), (9, 5))]))
        // no definition on docstring content
        (1, 4, _DefinitionChecker([]))])

class \nodoc\ iso _DefinitionThisIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/this"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "definition/_class.pony",
      [ // `this` in method body → enclosing class declaration (line 0)
        (13, 4, _DefinitionChecker([("_class.pony", (0, 0), (0, 5))]))])

class \nodoc\ iso _DefinitionKeywordsIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/keywords"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "definition/_class.pony",
      [ // `class` keyword in a declaration → no definition
        (0, 0, _DefinitionChecker([]))
        // `new` keyword in a constructor declaration → no definition
        (6, 2, _DefinitionChecker([]))
        // `fun` keyword in a method declaration → no definition
        (9, 2, _DefinitionChecker([]))
        // `:` type annotation separator → no definition
        (9, 11, _DefinitionChecker([]))])

class \nodoc\ iso _DefinitionTraitIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/trait"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "definition/_trait.pony",
      [ // call via trait-typed receiver → trait method declaration (line 7)
        (50, 6, _DefinitionChecker([("_trait.pony", (7, 2), (7, 5))]))])

class \nodoc\ iso _DefinitionUnionIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/union"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "definition/_trait.pony",
      [ // call via union-typed receiver → one definition per union member
        // _DefLeft.shared (line 29) and _DefRight.shared (line 35)
        (53, 6, _DefinitionChecker(
          [ ("_trait.pony", (29, 2), (29, 5))
            ("_trait.pony", (35, 2), (35, 5))]))])

class \nodoc\ iso _DefinitionCrossFileIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/cross_file"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "definition/_cross_usage.pony",
      [ // type reference in parameter → class declaration in other file
        (13, 16, _DefinitionChecker([("_cross_target.pony", (0, 0), (0, 5))]))
        // method call → method declaration in other file (line 13)
        (14, 8, _DefinitionChecker(
          [("_cross_target.pony", (13, 2), (13, 5))]))])

class \nodoc\ iso _DefinitionGenericsIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/generics"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "definition/_generics.pony",
      [ // `T` in return type → type parameter declaration in method header
        (19, 22, _DefinitionChecker(
          [("_generics.pony", (19, 12), (19, 16))]))])

class \nodoc\ iso _DefinitionTupleIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/tuple"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "definition/_tuple.pony",
      [ // `_1` tuple element access → `let pair` declaration
        (16, 9, _DefinitionChecker([("_tuple.pony", (15, 4), (15, 7))]))])

class \nodoc\ iso _DefinitionTypeAliasIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/type_alias"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "definition/_type_alias.pony",
      [ // `Map` type alias in `Map[String, U32]` → Map type alias declaration
        (17, 13, _DefinitionChecker([("map.pony", (3, 0), (3, 4))]))
        // `String` type arg in `Map[String, U32]` → String class declaration
        (17, 17, _DefinitionChecker([("string.pony", (8, 0), (8, 5))]))
        // `U32` type arg in `Map[String, U32]` → U32 primitive declaration
        (17, 25, _DefinitionChecker(
          [("unsigned.pony", (185, 0), (185, 9))]))
        // `_Alias` usage → type alias declaration (line 2)
        (19, 20, _DefinitionChecker(
          [("_type_alias.pony", (2, 0), (2, 4))]))])

type DefinitionExpectation is (String val, (I64, I64), (I64, I64))

class val _DefinitionChecker
  let _expected: Array[DefinitionExpectation] val

  new val create(expected: Array[DefinitionExpectation] val) =>
    _expected = expected

  fun lsp_method(): String =>
    Methods.text_document().definition()

  fun lsp_range(): (None | (I64, I64, I64, I64)) =>
    None

  fun lsp_context(): (None | JsonObject) =>
    None

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    var ok =
      h.assert_eq[USize](
        _expected.size(),
        try JsonNav(res.result).size()? else 0 end,
        "Wrong number of definitions")
    for (i, loc) in _expected.pairs() do
      (let file_suffix, let start_pos, let end_pos) = loc
      (let exp_start_line, let exp_start_char) = start_pos
      (let exp_end_line, let exp_end_char) = end_pos
      try
        let nav = JsonNav(res.result)(i)
        let uri = nav("uri").as_string()?
        let got_start_line = nav("range")("start")("line").as_i64()?
        let got_start_char = nav("range")("start")("character").as_i64()?
        let got_end_line = nav("range")("end")("line").as_i64()?
        let got_end_char = nav("range")("end")("character").as_i64()?
        ok = h.assert_true(
          uri.contains(file_suffix),
          "Expected URI containing '" + file_suffix + "', got: " + uri) and ok
        ok = h.assert_eq[I64](exp_start_line, got_start_line) and ok
        ok = h.assert_eq[I64](exp_start_char, got_start_char) and ok
        ok = h.assert_eq[I64](exp_end_line, got_end_line) and ok
        ok = h.assert_eq[I64](exp_end_char, got_end_char) and ok
      else
        ok = false
        h.log(
          "Definition [" + i.string() +
          "] returned null or invalid, expected (" +
          file_suffix + ":" + exp_start_line.string() +
          ":" + exp_start_char.string() + ")")
      end
    end
    ok
