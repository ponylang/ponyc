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
    let workspace_file = "definition/_class.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // field usages → field declaration (line 4, "let" keyword span)
        ((7, 4), [("_class.pony", (4, 2), (4, 5))])
        ((10, 4), [("_class.pony", (4, 2), (4, 5))])
        // parameter usage → parameter declaration (line 6, "v: U32" span)
        ((7, 13), [("_class.pony", (6, 13), (6, 19))])
        // method call → method declaration (line 9, "fun" keyword span)
        ((13, 9), [("_class.pony", (9, 2), (9, 5))])
        // no definition on docstring content
        ((1, 4), [])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _DefinitionChecker(expected))
    end

class \nodoc\ iso _DefinitionThisIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/this"

  fun apply(h: TestHelper) =>
    let workspace_file = "definition/_class.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // `this` in method body → enclosing class declaration (line 0)
        ((13, 4), [("_class.pony", (0, 0), (0, 5))])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _DefinitionChecker(expected))
    end

class \nodoc\ iso _DefinitionKeywordsIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/keywords"

  fun apply(h: TestHelper) =>
    let workspace_file = "definition/_class.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // `class` keyword in a declaration → no definition
        ((0, 0), [])
        // `new` keyword in a constructor declaration → no definition
        ((6, 2), [])
        // `fun` keyword in a method declaration → no definition
        ((9, 2), [])
        // `:` type annotation separator → no definition
        ((9, 11), [])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _DefinitionChecker(expected))
    end

class \nodoc\ iso _DefinitionTraitIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/trait"

  fun apply(h: TestHelper) =>
    let workspace_file = "definition/_trait.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // call via trait-typed receiver → trait method declaration (line 7)
        ((50, 6), [("_trait.pony", (7, 2), (7, 5))])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _DefinitionChecker(expected))
    end

class \nodoc\ iso _DefinitionUnionIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/union"

  fun apply(h: TestHelper) =>
    let workspace_file = "definition/_trait.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // call via union-typed receiver → one definition per union member
        // _DefLeft.shared (line 29) and _DefRight.shared (line 35)
        ((53, 6),
          [
            ("_trait.pony", (29, 2), (29, 5))
            ("_trait.pony", (35, 2), (35, 5))
          ])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _DefinitionChecker(expected))
    end

class \nodoc\ iso _DefinitionCrossFileIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/cross_file"

  fun apply(h: TestHelper) =>
    let workspace_file = "definition/_cross_usage.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // type reference in parameter → class declaration in other file
        ((13, 16), [("_cross_target.pony", (0, 0), (0, 5))])
        // method call → method declaration in other file (line 13)
        ((14, 8), [("_cross_target.pony", (13, 2), (13, 5))])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _DefinitionChecker(expected))
    end

class \nodoc\ iso _DefinitionGenericsIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/generics"

  fun apply(h: TestHelper) =>
    let workspace_file = "definition/_generics.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // `T` in return type → type parameter declaration in method header
        ((19, 22), [("_generics.pony", (19, 12), (19, 16))])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _DefinitionChecker(expected))
    end

class \nodoc\ iso _DefinitionTupleIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/tuple"

  fun apply(h: TestHelper) =>
    let workspace_file = "definition/_tuple.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // `_1` tuple element access → `let pair` declaration
        ((16, 9), [("_tuple.pony", (15, 4), (15, 7))])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _DefinitionChecker(expected))
    end

class \nodoc\ iso _DefinitionTypeAliasIntegrationTest is UnitTest
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "definition/integration/type_alias"

  fun apply(h: TestHelper) =>
    let workspace_file = "definition/_type_alias.pony"
    let checks: Array[DefinitionCheck] val =
      [
        // `String` type arg in `Map[String, U32]` → String class declaration
        ((17, 17), [("string.pony", (8, 0), (8, 5))])
        // `U32` type arg in `Map[String, U32]` → U32 primitive declaration
        ((17, 25), [("unsigned.pony", (185, 0), (185, 9))])
        // `_Alias` usage — after reification the alias is replaced with its
        // expansion (U8), so the original alias identity is lost and goto
        // definition returns no result. Fixing this requires first-class type
        // aliases (ponylang/ponyc#5007).
        ((19, 20), [])
      ]
    h.long_test(10_000_000_000)
    for ((line, character), expected) in checks.values() do
      h.expect_action(
        workspace_file + ":" + line.string() + ":" + character.string())
      _server.request(
        h, workspace_file, line, character, _DefinitionChecker(expected))
    end

type DefinitionExpectation is (String val, (I64, I64), (I64, I64))
type DefinitionCheck is ((I64, I64), Array[DefinitionExpectation] val)

class val _DefinitionChecker
  let _expected: Array[DefinitionExpectation] val

  new val create(expected: Array[DefinitionExpectation] val) =>
    _expected = expected

  fun lsp_method(): String =>
    Methods.text_document().definition()

  fun check(res: ResponseMessage val, h: TestHelper, action: String): Bool =>
    var ok = true
    let got_count =
      try JsonNav(res.result).size()? else 0 end
    if not h.assert_eq[USize](
      _expected.size(),
      got_count,
      "Wrong number of definitions")
    then
      ok = false
    end
    for (i, loc) in _expected.pairs() do
      (let file_suffix, let start_pos, let end_pos) = loc
      (let exp_start_line, let exp_start_char) = start_pos
      (let exp_end_line, let exp_end_char) = end_pos
      try
        let nav = JsonNav(res.result)(i)
        let uri = nav("uri").as_string()?
        let got_start_line =
          nav("range")("start")("line").as_i64()?
        let got_start_char =
          nav("range")("start")("character").as_i64()?
        let got_end_line =
          nav("range")("end")("line").as_i64()?
        let got_end_char =
          nav("range")("end")("character").as_i64()?
        if not h.assert_true(
          uri.contains(file_suffix),
          "Expected URI containing '" + file_suffix + "', got: " + uri)
        then
          ok = false
        end
        if not h.assert_eq[I64](exp_start_line, got_start_line) then
          ok = false
        end
        if not h.assert_eq[I64](exp_start_char, got_start_char) then
          ok = false
        end
        if not h.assert_eq[I64](exp_end_line, got_end_line) then
          ok = false
        end
        if not h.assert_eq[I64](exp_end_char, got_end_char) then
          ok = false
        end
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
