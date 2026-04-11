use ".."
use "pony_test"
use "files"
use "json"
use "collections"

primitive _DocumentHighlightIntegrationTests is TestList
  new make() => None

  fun tag tests(test: PonyTest) =>
    let workspace_dir = Path.join(Path.dir(__loc.file()), "workspace")
    let fixture = "highlights/highlights.pony"
    let server = _LspTestServer(workspace_dir)
    test(_DocHighlightFieldTest.create(server, fixture))
    test(_DocHighlightFieldRefTest.create(server, fixture))
    test(_DocHighlightLocalTest.create(server, fixture))
    test(_DocHighlightFletTest.create(server, fixture))
    test(_DocHighlightFletRefTest.create(server, fixture))
    test(_DocHighlightEmbedTest.create(server, fixture))
    test(_DocHighlightEmbedRefTest.create(server, fixture))
    test(_DocHighlightNewRefTest.create(server, fixture))
    test(_DocHighlightFunRefTest.create(server, fixture))
    test(_DocHighlightParamTest.create(server, fixture))
    test(_DocHighlightVarLocalTest.create(server, fixture))
    test(_DocHighlightBeRefTest.create(server, fixture))
    test(_DocHighlightClassDeclTest.create(server, fixture))
    test(_DocHighlightClassTypeTest.create(server, fixture))
    test(_DocHighlightTypeDeclTest.create(server, fixture))
    test(_DocHighlightTypeRefTest.create(server, fixture))
    test(_DocHighlightLiteralTest.create(server, fixture))
    test(_DocHighlightNoneTest.create(server, fixture))
    test(_DocHighlightInnerXFieldTest.create(server, fixture))
    test(_DocHighlightValFieldTest.create(server, fixture))
    test(_DocHighlightDoWorkParamTest.create(server, fixture))
    test(_DocHighlightDoWorkLetTest.create(server, fixture))
    test(_DocHighlightLetRefTest.create(server, fixture))
    test(_DocHighlightVarRefTest.create(server, fixture))
    test(_DocHighlightParamRefTest.create(server, fixture))
    test(_DocHighlightNewRefCallTest.create(server, fixture))
    test(_DocHighlightIntLiteralTest.create(server, fixture))
    test(_DocHighlightFloatLiteralTest.create(server, fixture))
    test(_DocHighlightStringLiteralTest.create(server, fixture))
    test(_DocHighlightWhitespaceTest.create(server, fixture))
    test(_DocHighlightOutOfBoundsTest.create(server, fixture))
    test(_DocHighlightUninitLocalTest.create(server, fixture))
    test(_DocHighlightTupleAssignATest.create(server, fixture))
    test(_DocHighlightTupleAssignBTest.create(server, fixture))
    test(_DocHighlightTupleElemRefTest.create(server, fixture))
    test(_DocHighlightBeRefExprTest.create(server, fixture))
    test(_DocHighlightNewBeRefTest.create(server, fixture))
    test(_DocHighlightGenericMethodTTest.create(server, fixture))
    test(_DocHighlightGenericClassTTest.create(server, fixture))
    test(_DocHighlightGenericClassTRefTest.create(server, fixture))
    test(_DocHighlightGenericPairATest.create(server, fixture))
    test(_DocHighlightGenericPairBTest.create(server, fixture))
    test(_DocHighlightConstrainedGenericTTest.create(server, fixture))

class \nodoc\ iso _DocHighlightFieldTest
  is UnitTest
  """
  Highlights the `count` field from its declaration site.
  Expects all 5 occurrences to be found:
    line 21 col  6  (var count: U32 = 0 declaration — Write, has initializer)
    line 24 col  4  (tick LHS — Write)
    line 24 col 12  (tick RHS — Read)
    line 27 col  4  (value() body — Read)
    line 30 col 22  (use_local() body — Read)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/field"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (21, 6, _DocHighlightChecker(
        [ (21, 6, 21, 11, DocumentHighlightKind.write())
          (24, 4, 24, 9, DocumentHighlightKind.write())
          (24, 12, 24, 17, DocumentHighlightKind.read())
          (27, 4, 27, 9, DocumentHighlightKind.read())
          (30, 22, 30, 27, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightFieldRefTest
  is UnitTest
  """
  Highlights the `count` field from a reference site (line 27 col 4).
  Expects the same 5 occurrences as when starting from the declaration.
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/field_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (27, 4, _DocHighlightChecker(
        [ (21, 6, 21, 11, DocumentHighlightKind.write())
          (24, 4, 24, 9, DocumentHighlightKind.write())
          (24, 12, 24, 17, DocumentHighlightKind.read())
          (27, 4, 27, 9, DocumentHighlightKind.read())
          (30, 22, 30, 27, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightLocalTest
  is UnitTest
  """
  Highlights the `result` local variable from its declaration site.
  Expects 3 occurrences:
    line 30 col  8  (let result: U32 = count — Write, has initializer)
    line 31 col  4  (first use in result + result)
    line 31 col 13  (second use in result + result)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/local"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (30, 8, _DocHighlightChecker(
        [ (30, 8, 30, 14, DocumentHighlightKind.write())
          (31, 4, 31, 10, DocumentHighlightKind.read())
          (31, 13, 31, 19, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightFletTest is UnitTest
  """
  Highlights the `_flag` let field from its declaration.
  Expects 4 occurrences:
    line  80 col  6  (let _flag: Bool — Write, declaration)
    line  85 col  4  (assigned true in create — Write)
    line  90 col  4  (assigned false in other — Write)
    line 111 col  4  (_flag in enabled() body — Read, tk_fletref)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/flet"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (80, 6, _DocHighlightChecker(
        [ (80, 6, 80, 11, DocumentHighlightKind.write())
          (85, 4, 85, 9, DocumentHighlightKind.write())
          (90, 4, 90, 9, DocumentHighlightKind.write())
          (111, 4, 111, 9, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightEmbedTest is UnitTest
  """
  Highlights the `_inner` embed field from its declaration.
  Expects 4 occurrences:
    line  81 col  8  (embed _inner: _Inner — Write, declaration)
    line  86 col  4  (assigned in create — Write)
    line  91 col  4  (assigned in other — Write)
    line 114 col  4  (_inner in inner_x() body — Read, tk_embedref)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/embed"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (81, 8, _DocHighlightChecker(
        [ (81, 8, 81, 14, DocumentHighlightKind.write())
          (86, 4, 86, 10, DocumentHighlightKind.write())
          (91, 4, 91, 10, DocumentHighlightKind.write())
          (114, 4, 114, 10, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightNewRefTest is UnitTest
  """
  Highlights `create` constructor of _Inner from its declaration.
  Expects 3 occurrences:
    line 51 col  6  (new create declaration in _Inner — Text)
    line 86 col 20  (_Inner.create() in create body — Read)
    line 91 col 20  (_Inner.create() in other body — Read)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/new_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (51, 6, _DocHighlightChecker(
        [ (51, 6, 51, 12, DocumentHighlightKind.text())
          (86, 20, 86, 26, DocumentHighlightKind.read())
          (91, 20, 91, 26, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightFunRefTest is UnitTest
  """
  Highlights `add` from its declaration, covering tk_fun,
  tk_funref (direct call), and tk_funchain (chained call).
  Expects 3 occurrences:
    line 94 col 10  (fun ref add declaration — Text)
    line 102 col 15  (get_self().add(1) — Read, tk_funchain)
    line 107 col 12  (w = w + add(n) — Read, tk_funref)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/fun_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (94, 10, _DocHighlightChecker(
        [ (94, 10, 94, 13, DocumentHighlightKind.text())
          (102, 15, 102, 18, DocumentHighlightKind.read())
          (107, 12, 107, 15, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightParamTest is UnitTest
  """
  Highlights the `x` parameter of `add` from its declaration.
  Expects 3 occurrences:
    line 94 col 14  (x param declaration in add — Write)
    line 95 col 18  (_val = _val + x — Read)
    line 96 col  4  (x — return value — Read)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/param"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (94, 14, _DocHighlightChecker(
        [ (94, 14, 94, 15, DocumentHighlightKind.write())
          (95, 18, 95, 19, DocumentHighlightKind.read())
          (96, 4, 96, 5, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightVarLocalTest is UnitTest
  """
  Highlights the `w` var local from its declaration.
  Expects 4 occurrences:
    line 106 col  8  (var w: U32 = v — Write, has initializer)
    line 107 col  4  (w = w + ... LHS)
    line 107 col  8  (w = w + ... RHS)
    line 108 col  4  (w return value)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/var_local"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (106, 8, _DocHighlightChecker(
        [ (106, 8, 106, 9, DocumentHighlightKind.write())
          (107, 4, 107, 5, DocumentHighlightKind.write())
          (107, 8, 107, 9, DocumentHighlightKind.read())
          (108, 4, 108, 5, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightBeRefTest is UnitTest
  """
  Highlights the `run` behaviour from its declaration.
  Expects 2 occurrences:
    line 127 col  5  (be run declaration — Text)
    line 131 col  4  (run(1) call in trigger — Read)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/be_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (127, 5, _DocHighlightChecker(
        [ (127, 5, 127, 8, DocumentHighlightKind.text())
          (131, 4, 131, 7, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightClassTypeTest is UnitTest
  """
  Highlights the `_Inner` class from a reference site,
  covering tk_class and all tk_nominal/tk_typeref references.
  Expects 4 occurrences:
    line  33 col  6  (class _Inner declaration)
    line  81 col 16  (embed _inner: _Inner type annotation)
    line  86 col 13  (_Inner.create() receiver in create)
    line  91 col 13  (_Inner.create() receiver in other)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/class_type"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (81, 16, _DocHighlightChecker(
        [ (33, 6, 33, 12, DocumentHighlightKind.text())
          (81, 16, 81, 22, DocumentHighlightKind.text())
          (86, 13, 86, 19, DocumentHighlightKind.text())
          (91, 13, 91, 19, DocumentHighlightKind.text())]))])

class \nodoc\ iso _DocHighlightClassDeclTest is UnitTest
  """
  Highlights the `_Inner` class from its declaration site.
  Expects the same 4 occurrences as _DocHighlightClassTypeTest:
    line  33 col  6  (class _Inner declaration)
    line  81 col 16  (embed _inner: _Inner type annotation)
    line  86 col 13  (_Inner.create() receiver in create)
    line  91 col 13  (_Inner.create() receiver in other)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/class_decl"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (33, 6, _DocHighlightChecker(
        [ (33, 6, 33, 12, DocumentHighlightKind.text())
          (81, 16, 81, 22, DocumentHighlightKind.text())
          (86, 13, 86, 19, DocumentHighlightKind.text())
          (91, 13, 91, 19, DocumentHighlightKind.text())]))])

class \nodoc\ iso _DocHighlightTypeDeclTest is UnitTest
  """
  Highlights the `_HighlightMore` class from its declaration site.
  Expects the same 2 occurrences as _DocHighlightTypeRefTest:
    line  54 col  6  (class _HighlightMore declaration)
    line  98 col 22  (_HighlightMore in get_self return type)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/type_decl"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (54, 6, _DocHighlightChecker(
        [ (54, 6, 54, 20, DocumentHighlightKind.text())
          (98, 22, 98, 36, DocumentHighlightKind.text())]))])

class \nodoc\ iso _DocHighlightTypeRefTest is UnitTest
  """
  Highlights the `_HighlightMore` class from a reference site,
  covering tk_class and tk_nominal in a return type annotation.
  Expects 2 occurrences:
    line  54 col  6  (class _HighlightMore declaration)
    line  98 col 22  (_HighlightMore in get_self return type)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/type_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (98, 22, _DocHighlightChecker(
        [ (54, 6, 54, 20, DocumentHighlightKind.text())
          (98, 22, 98, 36, DocumentHighlightKind.text())]))])

class \nodoc\ iso _DocHighlightLiteralTest is UnitTest
  """
  Placing the cursor on a literal value should produce no highlights.
  Tests `true` on line 85 col 12 (_flag = true in create).
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/literal"

  fun apply(h: TestHelper) =>
    _RunLspChecks(h, _server, _fixture, [(85, 12, _DocHighlightChecker([]))])

class \nodoc\ iso _DocHighlightNoneTest is UnitTest
  """
  Placing the cursor on `None` (a type-literal expression) should produce no
  highlights. `None` desugars to `None.create()` synthetically; it has no
  referenceable identity.
  Tests `None` on line 128 col 4 (body of be run in _HighlightRunner).
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/none_literal"

  fun apply(h: TestHelper) =>
    _RunLspChecks(h, _server, _fixture, [(128, 4, _DocHighlightChecker([]))])

class \nodoc\ iso _DocHighlightInnerXFieldTest is UnitTest
  """
  Highlights the `x` fvar field of _Inner from its declaration.
  Expects 2 occurrences:
    line 49 col 6  (var x: U32 = 0 — Write, has inline initializer)
    line 52 col 4  (x = 0 assignment in create — Write)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/inner_x_field"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (49, 6, _DocHighlightChecker(
        [ (49, 6, 49, 7, DocumentHighlightKind.write())
          (52, 4, 52, 5, DocumentHighlightKind.write())]))])

class \nodoc\ iso _DocHighlightValFieldTest is UnitTest
  """
  Highlights the `_val` fvar field of _HighlightMore from its declaration.
  Expects 5 occurrences:
    line 82 col  6  (var _val: U32 — Write, declaration)
    line 87 col  4  (_val = 0 in create — Write)
    line 92 col  4  (_val = 1 in other — Write)
    line 95 col  4  (_val = ... LHS in add — Write)
    line 95 col 11  (... + _val RHS in add — Read)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/val_field"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (82, 6, _DocHighlightChecker(
        [ (82, 6, 82, 10, DocumentHighlightKind.write())
          (87, 4, 87, 8, DocumentHighlightKind.write())
          (92, 4, 92, 8, DocumentHighlightKind.write())
          (95, 4, 95, 8, DocumentHighlightKind.write())
          (95, 11, 95, 15, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightDoWorkParamTest is UnitTest
  """
  Highlights the `n` parameter of do_work from its declaration.
  Expects 3 occurrences:
    line 104 col 18  (n param declaration in do_work — Write)
    line 105 col 17  (let v: U32 = n — Read)
    line 107 col 16  (w = w + add(n) — Read)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/do_work_param"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (104, 18, _DocHighlightChecker(
        [ (104, 18, 104, 19, DocumentHighlightKind.write())
          (105, 17, 105, 18, DocumentHighlightKind.read())
          (107, 16, 107, 17, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightDoWorkLetTest is UnitTest
  """
  Highlights the `v` let local of do_work from its declaration.
  Expects 2 occurrences:
    line 105 col  8  (let v: U32 = n — Write, has initializer)
    line 106 col 17  (var w: U32 = v)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/do_work_let"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (105, 8, _DocHighlightChecker(
        [ (105, 8, 105, 9, DocumentHighlightKind.write())
          (106, 17, 106, 18, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightLetRefTest is UnitTest
  """
  Highlights `result` from a letref site (line 31 col 4).
  Verifies that starting from a reference produces the same highlights
  as starting from the declaration (_DocHighlightLocalTest).
  Expects 3 occurrences:
    line 30 col  8  (let result declaration)
    line 31 col  4  (first use in result + result)
    line 31 col 13  (second use in result + result)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/let_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (31, 4, _DocHighlightChecker(
        [ (30, 8, 30, 14, DocumentHighlightKind.write())
          (31, 4, 31, 10, DocumentHighlightKind.read())
          (31, 13, 31, 19, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightVarRefTest is UnitTest
  """
  Highlights `w` from a varref site (line 107 col 4).
  Verifies that starting from a reference produces the same highlights
  as starting from the declaration (_DocHighlightVarLocalTest).
  Expects 4 occurrences:
    line 106 col 8  (var w declaration)
    line 107 col 4  (w = w + ... LHS)
    line 107 col 8  (w = w + ... RHS)
    line 108 col 4  (w return value)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/var_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (107, 4, _DocHighlightChecker(
        [ (106, 8, 106, 9, DocumentHighlightKind.write())
          (107, 4, 107, 5, DocumentHighlightKind.write())
          (107, 8, 107, 9, DocumentHighlightKind.read())
          (108, 4, 108, 5, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightParamRefTest is UnitTest
  """
  Highlights `x` from a paramref site (line 95 col 18).
  Verifies that starting from a reference produces the same highlights
  as starting from the declaration (_DocHighlightParamTest).
  Expects 3 occurrences:
    line 94 col 14  (x param declaration in add — Write)
    line 95 col 18  (_val = _val + x — Read)
    line 96 col  4  (x — return value — Read)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/param_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (95, 18, _DocHighlightChecker(
        [ (94, 14, 94, 15, DocumentHighlightKind.write())
          (95, 18, 95, 19, DocumentHighlightKind.read())
          (96, 4, 96, 5, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightNewRefCallTest is UnitTest
  """
  Highlights `create` from a call site (_Inner.create() on line 86 col 20).
  Verifies that starting from a newref reference produces the same highlights
  as starting from the declaration (_DocHighlightNewRefTest).
  Expects 3 occurrences:
    line 51 col  6  (new create declaration in _Inner — Text)
    line 86 col 20  (_Inner.create() in _HighlightMore.create body — Read)
    line 91 col 20  (_Inner.create() in _HighlightMore.other body — Read)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/new_ref_call"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (86, 20, _DocHighlightChecker(
        [ (51, 6, 51, 12, DocumentHighlightKind.text())
          (86, 20, 86, 26, DocumentHighlightKind.read())
          (91, 20, 91, 26, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightIntLiteralTest is UnitTest
  """
  Placing the cursor on an integer literal should produce no highlights.
  Tests `0` on line 87 col 11 (_val = 0 in _HighlightMore.create).
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/int_literal"

  fun apply(h: TestHelper) =>
    _RunLspChecks(h, _server, _fixture, [(87, 11, _DocHighlightChecker([]))])

class \nodoc\ iso _DocHighlightFloatLiteralTest is UnitTest
  """
  Placing the cursor on a float literal should produce no highlights.
  Tests `3.14` on line 138 col 16 (let _f: F64 = 3.14 in _LiteralExamples).
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/float_literal"

  fun apply(h: TestHelper) =>
    _RunLspChecks(h, _server, _fixture, [(138, 16, _DocHighlightChecker([]))])

class \nodoc\ iso _DocHighlightStringLiteralTest is UnitTest
  """
  Placing the cursor on a string literal should produce no highlights.
  Tests `"hello"` on line 139 col 23 (in _LiteralExamples).
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/string_literal"

  fun apply(h: TestHelper) =>
    _RunLspChecks(h, _server, _fixture, [(139, 23, _DocHighlightChecker([]))])

class \nodoc\ iso _DocHighlightWhitespaceTest is UnitTest
  """
  Placing the cursor on a blank line should produce no highlights.
  Tests line 22 col 0 (blank line between count field and tick method).
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/whitespace"

  fun apply(h: TestHelper) =>
    _RunLspChecks(h, _server, _fixture, [(22, 0, _DocHighlightChecker([]))])

class \nodoc\ iso _DocHighlightOutOfBoundsTest is UnitTest
  """
  Placing the cursor past the end of a line should produce no highlights.
  Tests line 21 col 100 (past end of `  var count: U32 = 0`).
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/out_of_bounds"

  fun apply(h: TestHelper) =>
    _RunLspChecks(h, _server, _fixture, [(21, 100, _DocHighlightChecker([]))])

class \nodoc\ iso _DocHighlightUninitLocalTest is UnitTest
  """
  Highlights `acc` — a local var declared without an initializer.
  Expects 3 occurrences:
    line 148 col  8  (var acc: U32 — Write, declaration)
    line 149 col  4  (acc = 0 LHS — Write)
    line 150 col  4  (return acc — Read)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/uninit_local"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (148, 8, _DocHighlightChecker(
        [ (148, 8, 148, 11, DocumentHighlightKind.write())
          (149, 4, 149, 7, DocumentHighlightKind.write())
          (150, 4, 150, 7, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightTupleAssignATest is UnitTest
  """
  Highlights `ta` in a tuple destructuring assignment.
  Expects 3 occurrences:
    line 163 col  8  (var ta: U32 = 0 — Write, has initializer)
    line 165 col  5  ((ta, tb) = ... LHS — Write, tuple destructuring)
    line 166 col  4  (ta + tb — Read)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/tuple_assign_a"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (163, 8, _DocHighlightChecker(
        [ (163, 8, 163, 10, DocumentHighlightKind.write())
          (165, 5, 165, 7, DocumentHighlightKind.write())
          (166, 4, 166, 6, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightTupleAssignBTest is UnitTest
  """
  Highlights `tb` in a tuple destructuring assignment.
  Expects 3 occurrences:
    line 164 col  8  (var tb: U32 = 0 — Write, has initializer)
    line 165 col  9  ((ta, tb) = ... LHS — Write, tuple destructuring)
    line 166 col  9  (ta + tb — Read)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/tuple_assign_b"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (164, 8, _DocHighlightChecker(
        [ (164, 8, 164, 10, DocumentHighlightKind.write())
          (165, 9, 165, 11, DocumentHighlightKind.write())
          (166, 9, 166, 11, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightTupleElemRefTest is UnitTest
  """
  Highlights from the `_1` position in `pair._1` (tk_tupleelemref).
  The resolver follows the receiver, so this highlights all occurrences of
  `pair`. Crucially, the tk_tupleelemref itself must be kind=Read (not Text).
  Expects 2 occurrences:
    line 175 col  8  (let pair declaration — Write, has initializer)
    line 176 col  4  (pair._1 full expression — Read, tk_tupleelemref)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/tuple_elem_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (176, 9, _DocHighlightChecker(
        [ (175, 8, 175, 12, DocumentHighlightKind.write())
          (176, 4, 176, 11, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightBeRefExprTest is UnitTest
  """
  Highlights `go` called on an expression receiver (get_self().go(1)).
  The call produces tk_beref (not tk_bechain — that requires ~ syntax).
  Expects 2 occurrences:
    line 191 col  5  (be go declaration — Text)
    line 198 col 15  (get_self().go(1) — Read, tk_beref)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/be_ref_expr"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (198, 15, _DocHighlightChecker(
        [ (191, 5, 191, 7, DocumentHighlightKind.text())
          (198, 15, 198, 17, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightNewBeRefTest is UnitTest
  """
  Highlights `create` from a tk_newberef call site (_BeChainActor.create()).
  Expects 2 occurrences:
    line 188 col  6  (new create declaration — Text)
    line 207 col 18  (_BeChainActor.create() — Read, tk_newberef)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/new_be_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (207, 18, _DocHighlightChecker(
        [ (188, 6, 188, 12, DocumentHighlightKind.text())
          (207, 18, 207, 24, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightFletRefTest is UnitTest
  """
  Highlights `_flag` from a tk_fletref site (enabled() body, line 111 col 4).
  Expects the same 4 occurrences as _DocHighlightFletTest:
    line  80 col  6  (let _flag: Bool — Write, declaration)
    line  85 col  4  (assigned true in create — Write)
    line  90 col  4  (assigned false in other — Write)
    line 111 col  4  (_flag in enabled() body — Read, tk_fletref)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/flet_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (111, 4, _DocHighlightChecker(
        [ (80, 6, 80, 11, DocumentHighlightKind.write())
          (85, 4, 85, 9, DocumentHighlightKind.write())
          (90, 4, 90, 9, DocumentHighlightKind.write())
          (111, 4, 111, 9, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightEmbedRefTest is UnitTest
  """
  Highlights `_inner` from a tk_embedref site (inner_x() body, line 114 col 4).
  Expects the same 4 occurrences as _DocHighlightEmbedTest:
    line  81 col  8  (embed _inner: _Inner — Write, declaration)
    line  86 col  4  (assigned in create — Write)
    line  91 col  4  (assigned in other — Write)
    line 114 col  4  (_inner in inner_x() body — Read, tk_embedref)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/embed_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (114, 4, _DocHighlightChecker(
        [ (81, 8, 81, 14, DocumentHighlightKind.write())
          (86, 4, 86, 10, DocumentHighlightKind.write())
          (91, 4, 91, 10, DocumentHighlightKind.write())
          (114, 4, 114, 10, DocumentHighlightKind.read())]))])

class \nodoc\ iso _DocHighlightGenericMethodTTest is UnitTest
  """
  Highlights the type parameter `T` of `apply[T]` in _HighlightGenericMethod.
  Expects 3 occurrences, all Text kind:
    line 217 col 12  (T type param declaration in [T])
    line 217 col 18  (T in parameter type x: T)
    line 217 col 22  (T in return type ): T)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/generic_method_t"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (217, 12, _DocHighlightChecker(
        [ (217, 12, 217, 13, DocumentHighlightKind.text())
          (217, 18, 217, 19, DocumentHighlightKind.text())
          (217, 22, 217, 23, DocumentHighlightKind.text())]))])

class \nodoc\ iso _DocHighlightGenericClassTTest is UnitTest
  """
  Highlights the type parameter `T` of `_HighlightGenericClass[T]`.
  Expects 3 occurrences, all Text kind:
    line 220 col 29  (T type param declaration in class [T])
    line 228 col 12  (T in parameter type x: T in id)
    line 228 col 16  (T in return type ): T in id)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/generic_class_t"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (220, 29, _DocHighlightChecker(
        [ (220, 29, 220, 30, DocumentHighlightKind.text())
          (228, 12, 228, 13, DocumentHighlightKind.text())
          (228, 16, 228, 17, DocumentHighlightKind.text())]))])

class \nodoc\ iso _DocHighlightGenericPairATest is UnitTest
  """
  Highlights the type parameter `A` of `_HighlightGenericPair[A, B]`.
  Expects 4 occurrences, all Text kind:
    line 231 col 28  (A type param declaration in class [A, B])
    line 242 col 15  (A in first parameter a: A)
    line 242 col 25  (A in first return type ): A)
    line 245 col 16  (A in second parameter a: A)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/generic_pair_a"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (231, 28, _DocHighlightChecker(
        [ (231, 28, 231, 29, DocumentHighlightKind.text())
          (242, 15, 242, 16, DocumentHighlightKind.text())
          (242, 25, 242, 26, DocumentHighlightKind.text())
          (245, 16, 245, 17, DocumentHighlightKind.text())]))])

class \nodoc\ iso _DocHighlightGenericPairBTest is UnitTest
  """
  Highlights the type parameter `B` of `_HighlightGenericPair[A, B]`.
  Expects 4 occurrences, all Text kind, independent of the A occurrences:
    line 231 col 31  (B type param declaration in class [A, B])
    line 242 col 21  (B in first parameter b: B)
    line 245 col 22  (B in second parameter b: B)
    line 245 col 26  (B in second return type ): B)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/generic_pair_b"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (231, 31, _DocHighlightChecker(
        [ (231, 31, 231, 32, DocumentHighlightKind.text())
          (242, 21, 242, 22, DocumentHighlightKind.text())
          (245, 22, 245, 23, DocumentHighlightKind.text())
          (245, 26, 245, 27, DocumentHighlightKind.text())]))])

class \nodoc\ iso _DocHighlightGenericClassTRefTest is UnitTest
  """
  Highlights the type parameter `T` of `_HighlightGenericClass[T]`,
  with cursor placed on a USE site (the `T` in `x: T` on line 228 col 12).
  Expects the same 3 occurrences as _DocHighlightGenericClassTTest:
    line 220 col 29  (T type param declaration in class [T])
    line 228 col 12  (T in parameter type x: T in id)
    line 228 col 16  (T in return type ): T in id)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/generic_class_t_ref"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (228, 12, _DocHighlightChecker(
        [ (220, 29, 220, 30, DocumentHighlightKind.text())
          (228, 12, 228, 13, DocumentHighlightKind.text())
          (228, 16, 228, 17, DocumentHighlightKind.text())]))])

class \nodoc\ iso _DocHighlightConstrainedGenericTTest is UnitTest
  """
  Highlights the type parameter `T` of
  `_HighlightConstrainedGeneric[T: Stringable]`.
  The constraint (`Stringable`) should NOT appear as a highlight.
  Expects 3 occurrences, all Text kind:
    line 248 col 35  (T type param declaration in [T: Stringable])
    line 255 col 15  (T in parameter type x: T in apply)
    line 255 col 19  (T in return type ): T in apply)
  """
  let _server: _LspTestServer
  let _fixture: String val

  new iso create(server: _LspTestServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/constrained_generic_t"

  fun apply(h: TestHelper) =>
    _RunLspChecks(
      h,
      _server,
      _fixture,
      [ (248, 35, _DocHighlightChecker(
        [ (248, 35, 248, 36, DocumentHighlightKind.text())
          (255, 15, 255, 16, DocumentHighlightKind.text())
          (255, 19, 255, 20, DocumentHighlightKind.text())]))])

class val _DocHighlightChecker
  let _expected: Array[(I64, I64, I64, I64, I64)] val

  new val create(expected: Array[(I64, I64, I64, I64, I64)] val) =>
    _expected = expected

  fun lsp_method(): String =>
    Methods.text_document().document_highlight()

  fun lsp_range(): (None | (I64, I64, I64, I64)) =>
    None

  fun lsp_context(): (None | JsonObject) =>
    None

  fun check(res: ResponseMessage val, h: TestHelper): Bool =>
    var ok = true
    match res.result
    | let arr: JsonArray =>
      // Log all actual highlights upfront so they appear on any failure,
      // including kind mismatches where the count happens to match.
      for item in arr.values() do
        try
          let range = JsonNav(item)("range")
          let sl = range("start")("line").as_i64()?
          let sc = range("start")("character").as_i64()?
          let el = range("end")("line").as_i64()?
          let ec = range("end")("character").as_i64()?
          let kind =
            try
              JsonNav(item)("kind").as_i64()?
            else
              I64(1)
            end
          h.log(
            "  actual highlight (" + sl.string() + ", " + sc.string() +
            ")–(" + el.string() + ", " + ec.string() +
            ") kind=" + kind.string())
        end
      end
      let got = arr.size()
      let want = _expected.size()
      if not h.assert_eq[USize](
        want,
        got,
        "Expected " + want.string() + " highlights, got " + got.string())
      then
        ok = false
      end
      for (exp_sl, exp_sc, exp_el, exp_ec, exp_kind) in _expected.values() do
        var found = false
        for item in arr.values() do
          try
            let range = JsonNav(item)("range")
            let sl = range("start")("line").as_i64()?
            let sc = range("start")("character").as_i64()?
            let el = range("end")("line").as_i64()?
            let ec = range("end")("character").as_i64()?
            let kind = JsonNav(item)("kind").as_i64()?
            if (sl == exp_sl) and (sc == exp_sc) and
              (el == exp_el) and (ec == exp_ec) and (kind == exp_kind)
            then
              found = true
              break
            end
          end
        end
        if not h.assert_true(
          found,
          "Expected highlight (" + exp_sl.string() + ", " + exp_sc.string() +
          ")–(" + exp_el.string() + ", " + exp_ec.string() +
          ") kind=" + exp_kind.string() + " not found")
        then
          ok = false
        end
      end
    else
      if _expected.size() > 0 then
        ok = false
        h.log("Expected highlights but got null")
      end
    end
    ok
