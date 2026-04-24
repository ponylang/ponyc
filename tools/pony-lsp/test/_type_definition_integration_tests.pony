use ".."
use "pony_test"
use "files"
use "json"
use "collections"

primitive _TypeDefinitionIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let server = _LspTestServer(workspace_dir)
    test(_TypeDefinitionLocalVarIntegrationTest.create(server))
    test(_TypeDefinitionParamIntegrationTest.create(server))
    test(_TypeDefinitionNoTypeIntegrationTest.create(server))
    test(_TypeDefinitionInferredIntegrationTest.create(server))

class \nodoc\ iso _TypeDefinitionLocalVarIntegrationTest is UnitTest
  """
  Cursor on `obj` in `demo()` (line 28, col 4) — explicitly annotated let.
  type_definition.pony layout (0-indexed lines/cols):
    line 21:  class _TypeDefTarget          target declaration (21,0)-(23,10)
    line 25:  class _TypeDefUser
    line 26:    fun demo() =>
    line 27:      let obj: _TypeDefTarget = ...
    line 28:      obj                            obj at (28,4)

  Expects type definition at class _TypeDefTarget (21,0)-(23,10).
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "type_definition/integration/local_var"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "type_definition/type_definition.pony",
      [ (28, 4, _TypeDefinitionChecker(
        [("type_definition.pony", (21, 0), (23, 10))]))])

class \nodoc\ iso _TypeDefinitionParamIntegrationTest is UnitTest
  """
  Cursor on `target` in `with_param()` (line 31, col 4) — typed parameter.
  type_definition.pony layout (0-indexed lines/cols):
    line 21:  class _TypeDefTarget          target declaration (21,0)-(23,10)
    line 30:    fun with_param(target: _TypeDefTarget) =>
    line 31:      target                             target at (31,4)

  Expects type definition at class _TypeDefTarget (21,0)-(23,10).
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "type_definition/integration/param"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "type_definition/type_definition.pony",
      [ (31, 4, _TypeDefinitionChecker(
        [("type_definition.pony", (21, 0), (23, 10))]))])

class \nodoc\ iso _TypeDefinitionNoTypeIntegrationTest is UnitTest
  """
  Cursor on the `class` keyword of _TypeDefUser (line 25, col 0) — keywords
  have no type. Expects an empty result.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "type_definition/integration/no_type"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "type_definition/type_definition.pony",
      [(25, 0, _TypeDefinitionChecker([]))])

class \nodoc\ iso _TypeDefinitionInferredIntegrationTest is UnitTest
  """
  Cursor on `x` in `inferred_demo()` (line 35, col 4) — type inferred from
  `_TypeDefTarget.create()`, no explicit annotation.
  type_definition.pony layout (0-indexed lines/cols):
    line 21:  class _TypeDefTarget          target declaration (21,0)-(23,10)
    line 33:    fun inferred_demo() =>
    line 34:      let x = _TypeDefTarget.create()
    line 35:      x                              x at (35,4)

  Expects type definition at class _TypeDefTarget (21,0)-(23,10).
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String => "type_definition/integration/inferred"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "type_definition/type_definition.pony",
      [ (35, 4, _TypeDefinitionChecker(
        [("type_definition.pony", (21, 0), (23, 10))]))])

class val _TypeDefinitionChecker
  let _expected: Array[DefinitionExpectation] val

  new val create(expected: Array[DefinitionExpectation] val) =>
    _expected = expected

  fun lsp_method(): String =>
    Methods.text_document().type_definition()

  fun lsp_range(): (None | (I64, I64, I64, I64)) =>
    None

  fun lsp_context(): (None | JsonObject) =>
    None

  fun lsp_extra_params(): (None | JsonObject) =>
    None

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    var ok =
      h.assert_eq[USize](
        _expected.size(),
        try JsonNav(res.result).size()? else 0 end,
        "Wrong number of type definitions")
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
          "TypeDefinition [" + i.string() +
          "] returned null or invalid, expected (" +
          file_suffix + ":" + exp_start_line.string() +
          ":" + exp_start_char.string() + ")")
      end
    end
    ok
