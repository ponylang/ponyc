use ".."
use "pony_test"
use "files"
use "json"
use "collections"

primitive _RenameIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let server = _LspTestServer(workspace_dir)
    test(_RenameFieldTest.create(server))
    test(_RenameFieldFromRefTest.create(server))
    test(_RenameMethodTest.create(server))
    test(_RenameMethodFromCallTest.create(server))
    test(_RenameClassTest.create(server))
    test(_RenameLiteralTest.create(server))
    test(_RenameStdlibTest.create(server))
    test(_RenameMissingNameTest.create(server))
    test(_RenameClassFromRefTest.create(server))
    test(_RenameParamTest.create(server))
    test(_RenameLocalVarTest.create(server))
    test(_RenameClassTypeParamTest.create(server))
    test(_RenameClassTypeParamFromRefTest.create(server))
    test(_RenameFunTypeParamTest.create(server))
    test(_RenameFunTypeParamFromRefTest.create(server))
    test(_RenameInvalidNameTest.create(server))
    test(_RenameActorBeTest.create(server))
    test(_RenameActorBeFromCallTest.create(server))
    test(_RenameSingleOccurrenceTest.create(server))
    test(_PrepareRenameFieldTest.create(server))
    test(_PrepareRenameLiteralTest.create(server))
    test(_PrepareRenameStdlibTest.create(server))

class \nodoc\ iso _RenameFieldTest is UnitTest
  """
  Rename `_value` from its declaration (line 16, col 6).
  renameable.pony layout (0-indexed lines/cols):
    line  0:  class Renameable
    line  1:    \"\"\"  (docstring start)
    ...
    line 15:    \"\"\"  (docstring end)
    line 16:    var _value: U32 = 0          _value at (16,6)-(16,12)
    line 19:      _value = v               _value at (19,4)-(19,10)
    line 22:      _value                   _value at (22,4)-(22,10)
    line 25:    let result: U32 = x + _value  _value at (25,26)-(25,32)

  Expects 4 edits in renameable.pony: declaration + 3 uses.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/field"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/renameable.pony",
      [ (16, 6, _RenameChecker(
        "new_value",
        [ ("renameable.pony", 16, 6, 16, 12)
          ("renameable.pony", 19, 4, 19, 10)
          ("renameable.pony", 22, 4, 22, 10)
          ("renameable.pony", 25, 26, 25, 32)]))])

class \nodoc\ iso _RenameFieldFromRefTest is UnitTest
  """
  Rename `_value` from a use site (line 19, col 4) rather than the declaration.
  Should produce the same 4 edits as _RenameFieldTest.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/field_from_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/renameable.pony",
      [ (19, 4, _RenameChecker(
        "new_value",
        [ ("renameable.pony", 16, 6, 16, 12)
          ("renameable.pony", 19, 4, 19, 10)
          ("renameable.pony", 22, 4, 22, 10)
          ("renameable.pony", 25, 26, 25, 32)]))])

class \nodoc\ iso _RenameMethodTest is UnitTest
  """
  Rename `get_value` from its declaration (line 21, col 6).
  renameable.pony:
    line 21:  fun get_value(): U32 =>   get_value at (21,6)-(21,15)
  rename_user.pony:
    line 13:    r.get_value()            get_value at (13,6)-(13,15)

  Expects 1 edit in renameable.pony and 1 in rename_user.pony.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/method"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/renameable.pony",
      [ (21, 6, _RenameChecker(
        "fetch_value",
        [ ("renameable.pony", 21, 6, 21, 15)
          ("rename_user.pony", 13, 6, 13, 15)]))])

class \nodoc\ iso _RenameMethodFromCallTest is UnitTest
  """
  Rename `get_value` from its call site in rename_user.pony (line 13, col 6)
  rather than the declaration. Should produce the same 2 edits as
  _RenameMethodTest.
  rename_user.pony:
    line 13:    r.get_value()   get_value at (13,6)-(13,15)
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/method_from_call"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/rename_user.pony",
      [ (13, 6, _RenameChecker(
        "fetch_value",
        [ ("renameable.pony", 21, 6, 21, 15)
          ("rename_user.pony", 13, 6, 13, 15)]))])

class \nodoc\ iso _RenameClassTest is UnitTest
  """
  Rename `Renameable` from its declaration (line 0, col 6).
  renameable.pony:
    line 0:  class Renameable   Renameable at (0,6)-(0,16)
  rename_user.pony:
    line 12:  fun use_it(r: Renameable): U32 =>
                              Renameable at (12,16)-(12,26)

  Expects 1 edit in renameable.pony and 1 in rename_user.pony.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/class"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/renameable.pony",
      [ (0, 6, _RenameChecker(
        "RenameTarget",
        [ ("renameable.pony", 0, 6, 0, 16)
          ("rename_user.pony", 12, 16, 12, 26)]))])

class \nodoc\ iso _RenameLiteralTest is UnitTest
  """
  Rename a literal (`0` at line 16, col 20 of renameable.pony). The server
  should return an error response — literals have no referenceable identity.
  renameable.pony:
    line 16:  var _value: U32 = 0   literal 0 at (16,20)
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/literal_error"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/renameable.pony",
      [(16, 20, _RenameErrorChecker("new_name", ErrorCodes.request_failed()))])

class \nodoc\ iso _RenameStdlibTest is UnitTest
  """
  Rename `U32` (a stdlib type) at line 16, col 14 of renameable.pony. The
  server should return an error response — stdlib symbols are defined outside
  the workspace.
  renameable.pony:
    line 16:  var _value: U32 = 0   U32 at (16,14)
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/stdlib_error"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/renameable.pony",
      [(16, 14, _RenameErrorChecker("MyU32", ErrorCodes.request_failed()))])

class \nodoc\ iso _RenameMissingNameTest is UnitTest
  """
  Send a rename request with no `newName` parameter. The server should return
  an invalid_params error response.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/missing_name_error"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/renameable.pony",
      [(16, 6, _RenameErrorChecker(None, ErrorCodes.invalid_params()))])

class \nodoc\ iso _RenameInvalidNameTest is UnitTest
  """
  Send a rename request with a newName that is not a valid Pony identifier
  (`"hello world"` contains a space). The server should return an
  invalid_params error response.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/invalid_name_error"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/renameable.pony",
      [ ( 16, 6,
          _RenameErrorChecker("hello world", ErrorCodes.invalid_params()))])

class \nodoc\ iso _RenameClassFromRefTest is UnitTest
  """
  Rename `Renameable` from a use site in rename_user.pony (line 12, col 16)
  rather than the declaration. Should produce the same 2 edits as
  _RenameClassTest.
  rename_user.pony:
    line 12:  fun use_it(r: Renameable): U32 =>
                              Renameable at (12,16)-(12,26)
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/class_from_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/rename_user.pony",
      [ (12, 16, _RenameChecker(
        "RenameTarget",
        [ ("renameable.pony", 0, 6, 0, 16)
          ("rename_user.pony", 12, 16, 12, 26)]))])

class \nodoc\ iso _RenameParamTest is UnitTest
  """
  Rename the constructor parameter `v` from its declaration (line 18, col 13).
  renameable.pony layout (0-indexed lines/cols):
    line 18:  new create(v: U32) =>   v at (18,13)-(18,14)
    line 19:    _value = v            v at (19,13)-(19,14)

  Expects 2 edits in renameable.pony.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/param"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/renameable.pony",
      [ (18, 13, _RenameChecker(
        "val",
        [ ("renameable.pony", 18, 13, 18, 14)
          ("renameable.pony", 19, 13, 19, 14)]))])

class \nodoc\ iso _RenameLocalVarTest is UnitTest
  """
  Rename the local variable `result` in `compute` from its declaration
  (line 25, col 8). renameable.pony layout (0-indexed lines/cols):
    line 24:  fun compute(x: U32): U32 =>
    line 25:    let result: U32 = x + _value   result at (25,8)-(25,14)
    line 26:    result                         result at (26,4)-(26,10)

  Expects 2 edits in renameable.pony.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/local_var"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/renameable.pony",
      [ (25, 8, _RenameChecker(
        "value",
        [ ("renameable.pony", 25, 8, 25, 14)
          ("renameable.pony", 26, 4, 26, 10)]))])

class \nodoc\ iso _RenameClassTypeParamTest is UnitTest
  """
  Rename the class-level type parameter `A` from its declaration (line 0,
  col 15). generics.pony layout (0-indexed lines/cols):
    line  0:  class Generics[A: Any val]   A at (0,15)-(0,16)
    line 13:    var _item: A               A at (13,13)-(13,14)
    line 15:    new create(item: A) =>     A at (15,19)-(15,20)
    line 18:    fun get(): A =>            A at (18,13)-(18,14)

  Expects 4 edits in generics.pony.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/class_type_param"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/generics.pony",
      [ (0, 15, _RenameChecker(
        "Item",
        [ ("generics.pony", 0, 15, 0, 16)
          ("generics.pony", 13, 13, 13, 14)
          ("generics.pony", 15, 19, 15, 20)
          ("generics.pony", 18, 13, 18, 14)]))])

class \nodoc\ iso _RenameClassTypeParamFromRefTest is UnitTest
  """
  Rename the class-level type parameter `A` from a use site (line 13, col 13)
  rather than the declaration. Should produce the same 4 edits as
  _RenameClassTypeParamTest.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/class_type_param_from_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/generics.pony",
      [ (13, 13, _RenameChecker(
        "Item",
        [ ("generics.pony", 0, 15, 0, 16)
          ("generics.pony", 13, 13, 13, 14)
          ("generics.pony", 15, 19, 15, 20)
          ("generics.pony", 18, 13, 18, 14)]))])

class \nodoc\ iso _RenameFunTypeParamTest is UnitTest
  """
  Rename the function-level type parameter `B` in `transform` from its
  declaration (line 21, col 16). generics.pony layout (0-indexed):
    line 21:  fun transform[B: Any val](item: B): B =>
      B declaration at (21,16)-(21,17)
      B param type  at (21,34)-(21,35)
      B return type at (21,38)-(21,39)

  Expects 3 edits in generics.pony.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/fun_type_param"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/generics.pony",
      [ (21, 16, _RenameChecker(
        "Elem",
        [ ("generics.pony", 21, 16, 21, 17)
          ("generics.pony", 21, 34, 21, 35)
          ("generics.pony", 21, 38, 21, 39)]))])

class \nodoc\ iso _RenameFunTypeParamFromRefTest is UnitTest
  """
  Rename the function-level type parameter `B` from its return-type position
  (line 21, col 38) rather than the declaration. Should produce the same 3
  edits as _RenameFunTypeParamTest.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/fun_type_param_from_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/generics.pony",
      [ (21, 38, _RenameChecker(
        "Elem",
        [ ("generics.pony", 21, 16, 21, 17)
          ("generics.pony", 21, 34, 21, 35)
          ("generics.pony", 21, 38, 21, 39)]))])

class \nodoc\ iso _RenameActorBeTest is UnitTest
  """
  Rename `work` from its declaration (line 12, col 5).
  actor.pony layout (0-indexed lines/cols):
    line 12:  be work(value: U32) =>   work at (12,5)-(12,9)
    line 20:    a.work(0)              work at (20,6)-(20,10)

  Expects 2 edits in actor.pony: declaration + call.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/actor_be"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/actor.pony",
      [ (12, 5, _RenameChecker(
        "process",
        [ ("actor.pony", 12, 5, 12, 9)
          ("actor.pony", 20, 6, 20, 10)]))])

class \nodoc\ iso _RenameActorBeFromCallTest is UnitTest
  """
  Rename `work` from its call site (line 20, col 6) rather than the
  declaration. Should produce the same 2 edits as _RenameActorBeTest.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/actor_be_from_call"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/actor.pony",
      [ (20, 6, _RenameChecker(
        "process",
        [ ("actor.pony", 12, 5, 12, 9)
          ("actor.pony", 20, 6, 20, 10)]))])

class \nodoc\ iso _RenameSingleOccurrenceTest is UnitTest
  """
  Rename `_tag_only` from its declaration (line 10, col 6). The field is
  never referenced anywhere, so the server should return exactly 1 edit.
  actor.pony layout (0-indexed lines/cols):
    line 10:  var _tag_only: U32 = 0   _tag_only at (10,6)-(10,15)

  Expects 1 edit in actor.pony: only the declaration.
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/single_occurrence"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/actor.pony",
      [ (10, 6, _RenameChecker(
        "_only",
        [ ("actor.pony", 10, 6, 10, 15)]))])

class val _RenameErrorChecker
  """
  Validates that a textDocument/rename response is an error (not a
  WorkspaceEdit) with the expected error code. Pass a String new_name to
  include it in the request params, or None to omit `newName` entirely (to
  exercise the missing-param path).
  """
  let _new_name: (String val | None)
  let _expected_code: I64

  new val create(new_name: (String val | None), expected_code: I64) =>
    _new_name = new_name
    _expected_code = expected_code

  fun lsp_method(): String =>
    Methods.text_document().rename()

  fun lsp_range(): (None | (I64, I64, I64, I64)) =>
    None

  fun lsp_context(): (None | JsonObject) =>
    None

  fun lsp_extra_params(): (None | JsonObject) =>
    match \exhaustive\ _new_name
    | let n: String val => JsonObject.update("newName", n)
    | None => None
    end

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    match \exhaustive\ res.err
    | let err: ResponseError val =>
      h.assert_eq[I64](_expected_code, err.code, "Wrong error code")
    | None =>
      h.log("Expected error response but got result: " + res.string())
      false
    end

class val _RenameChecker
  """
  Validates a textDocument/rename response (WorkspaceEdit) against expected
  edits. Each expected edit is (filename_basename, start_line, start_char,
  end_line, end_char). All edits are checked against the `changes` map in the
  response regardless of which URI they appear in.
  """
  let _new_name: String val
  let _expected: Array[(String, I64, I64, I64, I64)] val

  new val create(
    new_name: String val,
    expected: Array[(String, I64, I64, I64, I64)] val)
  =>
    _new_name = new_name
    _expected = expected

  fun lsp_method(): String =>
    Methods.text_document().rename()

  fun lsp_range(): (None | (I64, I64, I64, I64)) =>
    None

  fun lsp_context(): (None | JsonObject) =>
    None

  fun lsp_extra_params(): (None | JsonObject) =>
    JsonObject.update("newName", _new_name)

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    var ok = true
    // Collect all (file_basename, sl, sc, el, ec, newText) from changes.
    let got: Array[(String, I64, I64, I64, I64, String)] =
      Array[(String, I64, I64, I64, I64, String)].create()
    try
      let changes = JsonNav(res.result)("changes").as_object()?
      for (uri, edits_val) in changes.pairs() do
        let file = Path.base(Uris.to_path(uri))
        let edits = edits_val as JsonArray
        for edit in edits.values() do
          let range = JsonNav(edit)("range")
          let sl = range("start")("line").as_i64()?
          let sc = range("start")("character").as_i64()?
          let el = range("end")("line").as_i64()?
          let ec = range("end")("character").as_i64()?
          let new_text = JsonNav(edit)("newText").as_string()?
          got.push((file, sl, sc, el, ec, new_text))
        end
      end
    else
      h.log("Expected WorkspaceEdit with changes but got: " + res.string())
      return false
    end

    let want = _expected.size()
    let got_count = got.size()
    if not h.assert_eq[USize](
      want,
      got_count,
      "Expected " + want.string() + " edits, got " + got_count.string())
    then
      ok = false
      for (file, sl, sc, el, ec, new_text) in got.values() do
        h.log(
          "  actual edit " + file +
          " (" + sl.string() + ", " + sc.string() +
          ")–(" + el.string() + ", " + ec.string() +
          ") -> '" + new_text + "'")
      end
    end

    for (exp_file, exp_sl, exp_sc, exp_el, exp_ec) in _expected.values() do
      var found = false
      for (file, sl, sc, el, ec, new_text) in got.values() do
        if (file == exp_file) and (sl == exp_sl) and (sc == exp_sc)
          and (el == exp_el) and (ec == exp_ec) and (new_text == _new_name)
        then
          found = true
          break
        end
      end
      if not h.assert_true(
        found,
        "Expected edit " + exp_file +
        " (" + exp_sl.string() + ", " + exp_sc.string() +
        ")–(" + exp_el.string() + ", " + exp_ec.string() + ") not found")
      then
        ok = false
      end
    end
    ok

class val _PrepareRenameChecker
  """
  Validates a textDocument/prepareRename response against an expected
  identifier range.
  """
  let _expected_sl: I64
  let _expected_sc: I64
  let _expected_el: I64
  let _expected_ec: I64

  new val create(sl: I64, sc: I64, el: I64, ec: I64) =>
    _expected_sl = sl
    _expected_sc = sc
    _expected_el = el
    _expected_ec = ec

  fun lsp_method(): String =>
    Methods.text_document().prepare_rename()

  fun lsp_range(): (None | (I64, I64, I64, I64)) =>
    None

  fun lsp_context(): (None | JsonObject) =>
    None

  fun lsp_extra_params(): (None | JsonObject) =>
    None

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    try
      let range = JsonNav(res.result)
      let sl = range("start")("line").as_i64()?
      let sc = range("start")("character").as_i64()?
      let el = range("end")("line").as_i64()?
      let ec = range("end")("character").as_i64()?
      var ok = true
      ok = h.assert_eq[I64](_expected_sl, sl, "Wrong start line") and ok
      ok = h.assert_eq[I64](_expected_sc, sc, "Wrong start character") and ok
      ok = h.assert_eq[I64](_expected_el, el, "Wrong end line") and ok
      ok = h.assert_eq[I64](_expected_ec, ec, "Wrong end character") and ok
      ok
    else
      h.log("Expected prepareRename range but got: " + res.string())
      false
    end

class val _PrepareRenameErrorChecker
  """
  Validates that a textDocument/prepareRename response is an error with the
  expected error code.
  """
  let _expected_code: I64

  new val create(expected_code: I64) =>
    _expected_code = expected_code

  fun lsp_method(): String =>
    Methods.text_document().prepare_rename()

  fun lsp_range(): (None | (I64, I64, I64, I64)) =>
    None

  fun lsp_context(): (None | JsonObject) =>
    None

  fun lsp_extra_params(): (None | JsonObject) =>
    None

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    match \exhaustive\ res.err
    | let err: ResponseError val =>
      h.assert_eq[I64](_expected_code, err.code, "Wrong error code")
    | None =>
      h.log("Expected error response but got result: " + res.string())
      false
    end

class \nodoc\ iso _PrepareRenameFieldTest is UnitTest
  """
  PrepareRename on `_value` at (16, 6) in renameable.pony. Should return the
  identifier range (16,6)-(16,12) without error.
  renameable.pony layout (0-indexed lines/cols):
    line 16:  var _value: U32 = 0   _value at (16,6)-(16,12)
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/prepare_rename_field"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/renameable.pony",
      [(16, 6, _PrepareRenameChecker(16, 6, 16, 12))])

class \nodoc\ iso _PrepareRenameLiteralTest is UnitTest
  """
  PrepareRename on the literal `0` at (16, 20) in renameable.pony. Literals
  are not renameable — expect request_failed().
  renameable.pony layout (0-indexed lines/cols):
    line 16:  var _value: U32 = 0   literal `0` at (16,20)
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/prepare_rename_literal"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/renameable.pony",
      [(16, 20, _PrepareRenameErrorChecker(ErrorCodes.request_failed()))])

class \nodoc\ iso _PrepareRenameStdlibTest is UnitTest
  """
  PrepareRename on `U32` (stdlib type) at (16, 14) in renameable.pony.
  Stdlib symbols are outside the workspace — expect request_failed().
  renameable.pony layout (0-indexed lines/cols):
    line 16:  var _value: U32 = 0   U32 at (16,14)-(16,17)
  """
  let _server: _LspTestServer

  new iso create(server: _LspTestServer) =>
    _server = server

  fun name(): String =>
    "rename/integration/prepare_rename_stdlib"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      "rename/renameable.pony",
      [(16, 14, _PrepareRenameErrorChecker(ErrorCodes.request_failed()))])
