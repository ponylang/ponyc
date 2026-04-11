use ".."
use "pony_test"
use "files"
use "json"
use "collections"

primitive _ReferencesIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let fixture = "references/referenced_class.pony"
    let server = _LspTestServer(workspace_dir)
    test(_RefsCountDeclIncludedTest.create(server, fixture))
    test(_RefsCountDeclExcludedTest.create(server, fixture))
    test(_RefsCountRefIncludedTest.create(server, fixture))
    test(_RefsCountRefExcludedTest.create(server, fixture))
    test(_RefsIncrementCrossFileTest.create(server, fixture))
    test(_RefsIncrementDeclExcludedTest.create(server, fixture))
    test(_RefsTypeNameTest.create(server, fixture))
    test(_RefsTypeNameDeclExcludedTest.create(server, fixture))
    test(_RefsLiteralTest.create(server, fixture))
    test(_RefsSyntheticNewrefTest.create(server, fixture))
    test(_RefsGenericTypeparamDeclIncludedTest.create(server))
    test(_RefsGenericTypeparamDeclExcludedTest.create(server))

class \nodoc\ iso _RefsCountDeclIncludedTest is UnitTest
  """
  Find references to `_count` field from its declaration site,
  with includeDeclaration = true. Expects 5 locations:
    referenced_class.pony (16, 6)-(16,12)   declaration
    referenced_class.pony (19, 4)-(19,10)   in create
    referenced_class.pony (22, 4)-(22,10)   first in increment
    referenced_class.pony (22,13)-(22,19)   second in increment
    referenced_class.pony (23, 4)-(23,10)   return

  referenced_class.pony layout (0-indexed lines/cols):
    line  0:    class ReferencedClass
    line  1-15: docstring
    line 16:      var _count: U32 = 0     _count at (16,6)-(16,12)
    line 18:      new create() =>
    line 19:        _count = 0            _count at (19,4)-(19,10)
    line 21:      fun ref increment(): U32 =>   increment at (21,10)-(21,19)
    line 22:        _count = _count + 1
                            _count at (22,4)-(22,10) and (22,13)-(22,19)
    line 23:        _count                _count at (23,4)-(23,10)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "references/integration/count_decl_included"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (16, 6, _RefsChecker(
        [ ("referenced_class.pony", 16, 6, 16, 12)
          ("referenced_class.pony", 19, 4, 19, 10)
          ("referenced_class.pony", 22, 4, 22, 10)
          ("referenced_class.pony", 22, 13, 22, 19)
          ("referenced_class.pony", 23, 4, 23, 10)], true))])

class \nodoc\ iso _RefsCountDeclExcludedTest is UnitTest
  """
  Find references to `_count` field from its declaration site,
  with includeDeclaration = false. Expects 4 locations (no declaration).
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "references/integration/count_decl_excluded"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (16, 6, _RefsChecker(
        [ ("referenced_class.pony", 19, 4, 19, 10)
          ("referenced_class.pony", 22, 4, 22, 10)
          ("referenced_class.pony", 22, 13, 22, 19)
          ("referenced_class.pony", 23, 4, 23, 10)], false))])

class \nodoc\ iso _RefsCountRefIncludedTest is UnitTest
  """
  Find references to `_count` field from a reference site (line 23, col 4),
  with includeDeclaration = true. Expects the same 5 locations as from
  the declaration.
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "references/integration/count_ref_included"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (23, 4, _RefsChecker(
        [ ("referenced_class.pony", 16, 6, 16, 12)
          ("referenced_class.pony", 19, 4, 19, 10)
          ("referenced_class.pony", 22, 4, 22, 10)
          ("referenced_class.pony", 22, 13, 22, 19)
          ("referenced_class.pony", 23, 4, 23, 10)], true))])

class \nodoc\ iso _RefsCountRefExcludedTest is UnitTest
  """
  Find references to `_count` field from a reference site (line 23, col 4),
  with includeDeclaration = false. Expects 4 locations (no declaration),
  same as _RefsCountDeclExcludedTest but starting from a reference site.
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "references/integration/count_ref_excluded"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (23, 4, _RefsChecker(
        [ ("referenced_class.pony", 19, 4, 19, 10)
          ("referenced_class.pony", 22, 4, 22, 10)
          ("referenced_class.pony", 22, 13, 22, 19)
          ("referenced_class.pony", 23, 4, 23, 10)], false))])

class \nodoc\ iso _RefsIncrementCrossFileTest is UnitTest
  """
  Find references to `increment` method from its declaration site,
  with includeDeclaration = true. Expects 2 locations:
    referenced_class.pony (21,10)-(21,19)  declaration
    references_user.pony  (12, 8)-(12,17)  reference in ReferencesUser.use_it
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "references/integration/increment_cross_file"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (21, 10, _RefsChecker(
        [ ("referenced_class.pony", 21, 10, 21, 19)
          ("references_user.pony", 12, 8, 12, 17)], true))])

class \nodoc\ iso _RefsIncrementDeclExcludedTest is UnitTest
  """
  Find references to `increment` method from its declaration site,
  with includeDeclaration = false. Expects 1 location (no declaration):
    references_user.pony  (12, 8)-(12,17)  reference in ReferencesUser.use_it
  Exercises the tk_id promotion path: the cursor lands on the tk_id child
  of the tk_fun node, which is promoted to tk_fun before the exclusion key
  is computed.
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "references/integration/increment_decl_excluded"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (21, 10, _RefsChecker(
        [("references_user.pony", 12, 8, 12, 17)], false))])

class \nodoc\ iso _RefsTypeNameTest is UnitTest
  """
  Find references to `ReferencedClass` from its declaration name (line 0,
  col 6), with includeDeclaration = true. Expects 2 locations:
    referenced_class.pony (0, 6)-(0,21)   class declaration
    references_user.pony  (11,18)-(11,33)  type annotation in use_it param
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "references/integration/type_name"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (0, 6, _RefsChecker(
        [ ("referenced_class.pony", 0, 6, 0, 21)
          ("references_user.pony", 11, 18, 11, 33)], true))])

class \nodoc\ iso _RefsTypeNameDeclExcludedTest is UnitTest
  """
  Find references to `ReferencedClass` from its declaration name (line 0,
  col 6), with includeDeclaration = false. Expects 1 location (no declaration):
    references_user.pony  (11,18)-(11,33)  type annotation in use_it param
  Exercises the tk_id promotion path for entity declarations: the cursor
  lands on the tk_id child of tk_class, which is promoted to tk_class before
  the exclusion key is computed.
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "references/integration/type_name_decl_excluded"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (0, 6, _RefsChecker(
        [("references_user.pony", 11, 18, 11, 33)], false))])

class \nodoc\ iso _RefsLiteralTest is UnitTest
  """
  Requesting references for a literal value (the `0` on line 16)
  returns no results.
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "references/integration/literal"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [(16, 20, _RefsChecker([], true))])

class \nodoc\ iso _RefsSyntheticNewrefTest is UnitTest
  """
  Requesting references for a `None` type-literal expression (line 26, col 4)
  returns no results. `None` is desugared by the compiler into a synthetic
  tk_newref inside a tk_call at the same position; the guard in
  References.collect detects this pattern and returns early with [].
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "references/integration/synthetic_newref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(h, _server, _fixture, [(26, 4, _RefsChecker([], true))])

class \nodoc\ iso _RefsGenericTypeparamDeclIncludedTest is UnitTest
  """
  Find references to type parameter `T` of `GenericRefs[T]` from its
  declaration site (line 0, col 18), with includeDeclaration = true.
  Expects 3 locations — the declaration and both type annotations in id:
    generic_refs.pony (0, 18)-(0, 19)   T declaration in class [T]
    generic_refs.pony (4, 12)-(4, 13)   T in parameter type x: T
    generic_refs.pony (4, 16)-(4, 17)   T in return type ): T

  generic_refs.pony layout (0-indexed):
    line 0:  class GenericRefs[T]    T at (0,18)
    line 4:    fun id(x: T): T =>    T at (4,12) and (4,16)
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "references/integration/generic_typeparam_decl_included"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "references/generic_refs.pony",
      [ (0, 18, _RefsChecker(
        [ ("generic_refs.pony", 0, 18, 0, 19)
          ("generic_refs.pony", 4, 12, 4, 13)
          ("generic_refs.pony", 4, 16, 4, 17)], true))])

class \nodoc\ iso _RefsGenericTypeparamDeclExcludedTest is UnitTest
  """
  Find references to type parameter `T` of `GenericRefs[T]` from its
  declaration site (line 0, col 18), with includeDeclaration = false.
  Expects 2 locations (no declaration):
    generic_refs.pony (4, 12)-(4, 13)   T in parameter type x: T
    generic_refs.pony (4, 16)-(4, 17)   T in return type ): T
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "references/integration/generic_typeparam_decl_excluded"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "references/generic_refs.pony",
      [ (0, 18, _RefsChecker(
        [ ("generic_refs.pony", 4, 12, 4, 13)
          ("generic_refs.pony", 4, 16, 4, 17)], false))])

class val _RefsChecker
  """
  Validates a textDocument/references response against expected locations.
  Each expected location is (filename_basename, start_line, start_char,
  end_line, end_char).
  """
  let _expected: Array[(String, I64, I64, I64, I64)] val
  let _include_declaration: Bool

  new val create(
    expected: Array[(String, I64, I64, I64, I64)] val,
    include_declaration: Bool = true)
  =>
    _expected = expected
    _include_declaration = include_declaration

  fun lsp_method(): String =>
    Methods.text_document().references()

  fun lsp_range(): (None | (I64, I64, I64, I64)) =>
    None

  fun lsp_context(): (None | JsonObject) =>
    JsonObject.update("includeDeclaration", _include_declaration)

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    var ok = true
    match res.result
    | let arr: JsonArray =>
      let got = arr.size()
      let want = _expected.size()
      if not h.assert_eq[USize](
        want,
        got,
        "Expected " + want.string() + " references, got " + got.string())
      then
        ok = false
        for item in arr.values() do
          try
            let uri = JsonNav(item)("uri").as_string()?
            let file = Path.base(Uris.to_path(uri))
            let range = JsonNav(item)("range")
            let sl = range("start")("line").as_i64()?
            let sc = range("start")("character").as_i64()?
            let el = range("end")("line").as_i64()?
            let ec = range("end")("character").as_i64()?
            h.log(
              "  actual ref " + file +
              " (" + sl.string() + ", " + sc.string() +
              ")–(" + el.string() + ", " + ec.string() + ")")
          end
        end
      end
      for (exp_file, exp_sl, exp_sc, exp_el, exp_ec) in _expected.values() do
        var found = false
        for item in arr.values() do
          try
            let uri = JsonNav(item)("uri").as_string()?
            let file = Path.base(Uris.to_path(uri))
            let range = JsonNav(item)("range")
            let sl = range("start")("line").as_i64()?
            let sc = range("start")("character").as_i64()?
            let el = range("end")("line").as_i64()?
            let ec = range("end")("character").as_i64()?
            if (file == exp_file) and (sl == exp_sl) and (sc == exp_sc)
              and (el == exp_el) and (ec == exp_ec)
            then
              found = true
              break
            end
          end
        end
        if not h.assert_true(
          found,
          "Expected ref " + exp_file +
          " (" + exp_sl.string() + ", " + exp_sc.string() +
          ")–(" + exp_el.string() + ", " + exp_ec.string() + ") not found")
        then
          ok = false
        end
      end
    else
      if _expected.size() > 0 then
        ok = false
        h.log("Expected references but got null/non-array")
      end
    end
    ok
