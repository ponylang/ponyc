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
    let server = _DocHighlightLspServer(workspace_dir, fixture)
    test(_DocHighlightFieldTest.create(server, fixture))
    test(_DocHighlightFieldRefTest.create(server, fixture))
    test(_DocHighlightLocalTest.create(server, fixture))
    test(_DocHighlightFletTest.create(server, fixture))
    test(_DocHighlightEmbedTest.create(server, fixture))
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

class \nodoc\ iso _DocHighlightFieldTest
  is UnitTest
  """
  Highlights the `count` field from its declaration site.
  Expects all 5 occurrences to be found:
    line 21 col  6  (var count declaration)
    line 24 col  4  (tick LHS)
    line 24 col 12  (tick RHS)
    line 27 col  4  (value() body)
    line 30 col 22  (use_local() body)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/field"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (21, 6)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h,
      action,
      at,
      [ (21, 6, 21, 11); (24, 4, 24, 9); (24, 12, 24, 17)
        (27, 4, 27, 9); (30, 22, 30, 27)])

class \nodoc\ iso _DocHighlightFieldRefTest
  is UnitTest
  """
  Highlights the `count` field from a reference site (line 27 col 4).
  Expects the same 5 occurrences as when starting from the declaration.
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/field_ref"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (27, 4)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h,
      action,
      at,
      [ (21, 6, 21, 11); (24, 4, 24, 9); (24, 12, 24, 17)
        (27, 4, 27, 9); (30, 22, 30, 27)])

class \nodoc\ iso _DocHighlightLocalTest
  is UnitTest
  """
  Highlights the `result` local variable from its declaration site.
  Expects 3 occurrences:
    line 30 col  8  (let result declaration)
    line 31 col  4  (first use in result + result)
    line 31 col 13  (second use in result + result)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/local"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (30, 8)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h, action, at, [(30, 8, 30, 14); (31, 4, 31, 10); (31, 13, 31, 19)])

class \nodoc\ iso _DocHighlightFletTest is UnitTest
  """
  Highlights the `_flag` let field from its declaration.
  Expects 3 occurrences:
    line 80 col  6  (_flag declaration)
    line 85 col  4  (assigned true in create)
    line 90 col  4  (assigned false in other)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/flet"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (80, 6)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h, action, at, [(80, 6, 80, 11); (85, 4, 85, 9); (90, 4, 90, 9)])

class \nodoc\ iso _DocHighlightEmbedTest is UnitTest
  """
  Highlights the `_inner` embed field from its declaration.
  Expects 3 occurrences:
    line 81 col  8  (_inner embed declaration)
    line 86 col  4  (assigned in create)
    line 91 col  4  (assigned in other)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/embed"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (81, 8)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h, action, at, [(81, 8, 81, 14); (86, 4, 86, 10); (91, 4, 91, 10)])

class \nodoc\ iso _DocHighlightNewRefTest is UnitTest
  """
  Highlights `create` constructor of _Inner from its declaration.
  Expects 3 occurrences:
    line 51 col  6  (new create declaration in _Inner)
    line 86 col 20  (_Inner.create() in create body)
    line 91 col 20  (_Inner.create() in other body)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/new_ref"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (51, 6)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h, action, at, [(51, 6, 51, 12); (86, 20, 86, 26); (91, 20, 91, 26)])

class \nodoc\ iso _DocHighlightFunRefTest is UnitTest
  """
  Highlights `add` from its declaration, covering tk_fun,
  tk_funref (direct call), and tk_funchain (chained call).
  Expects 3 occurrences:
    line 94 col 10  (fun ref add declaration)
    line 102 col 15  (get_self().add(1) — funchain)
    line 107 col 12  (w = w + add(n) — funref)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/fun_ref"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (94, 10)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h, action, at, [(94, 10, 94, 13); (102, 15, 102, 18); (107, 12, 107, 15)])

class \nodoc\ iso _DocHighlightParamTest is UnitTest
  """
  Highlights the `x` parameter of `add` from its declaration.
  Expects 3 occurrences:
    line 94 col 14  (x param declaration in add)
    line 95 col 18  (_val = _val + x — body use)
    line 96 col  4  (x — return value)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/param"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (94, 14)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h, action, at, [(94, 14, 94, 15); (95, 18, 95, 19); (96, 4, 96, 5)])

class \nodoc\ iso _DocHighlightVarLocalTest is UnitTest
  """
  Highlights the `w` var local from its declaration.
  Expects 4 occurrences:
    line 106 col  8  (var w declaration)
    line 107 col  4  (w = w + ... LHS)
    line 107 col  8  (w = w + ... RHS)
    line 108 col  4  (w return value)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/var_local"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (106, 8)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h,
      action,
      at,
      [(106, 8, 106, 9); (107, 4, 107, 5); (107, 8, 107, 9); (108, 4, 108, 5)])

class \nodoc\ iso _DocHighlightBeRefTest is UnitTest
  """
  Highlights the `run` behaviour from its declaration.
  Expects 2 occurrences:
    line 121 col  5  (be run declaration)
    line 125 col  4  (run(1) call in trigger)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/be_ref"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (121, 5)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h, action, at, [(121, 5, 121, 8); (125, 4, 125, 7)])

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
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/class_type"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (81, 16)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h,
      action,
      at,
      [(33, 6, 33, 12); (81, 16, 81, 22); (86, 13, 86, 19); (91, 13, 91, 19)])

class \nodoc\ iso _DocHighlightClassDeclTest is UnitTest
  """
  Highlights the `_Inner` class from its declaration site.
  Expects the same 4 occurrences as _DocHighlightClassTypeTest:
    line  33 col  6  (class _Inner declaration)
    line  81 col 16  (embed _inner: _Inner type annotation)
    line  86 col 13  (_Inner.create() receiver in create)
    line  91 col 13  (_Inner.create() receiver in other)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/class_decl"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (33, 6)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h,
      action,
      at,
      [(33, 6, 33, 12); (81, 16, 81, 22); (86, 13, 86, 19); (91, 13, 91, 19)])

class \nodoc\ iso _DocHighlightTypeDeclTest is UnitTest
  """
  Highlights the `_HighlightMore` class from its declaration site.
  Expects the same 2 occurrences as _DocHighlightTypeRefTest:
    line  54 col  6  (class _HighlightMore declaration)
    line  98 col 22  (_HighlightMore in get_self return type)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/type_decl"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (54, 6)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h, action, at, [(54, 6, 54, 20); (98, 22, 98, 36)])

class \nodoc\ iso _DocHighlightTypeRefTest is UnitTest
  """
  Highlights the `_HighlightMore` class from a reference site,
  covering tk_class and tk_nominal in a return type annotation.
  Expects 2 occurrences:
    line  54 col  6  (class _HighlightMore declaration)
    line  98 col 22  (_HighlightMore in get_self return type)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/type_ref"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (98, 22)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h, action, at, [(54, 6, 54, 20); (98, 22, 98, 36)])

class \nodoc\ iso _DocHighlightLiteralTest is UnitTest
  """
  Placing the cursor on a literal value should produce no highlights.
  Tests `true` on line 85 col 12 (_flag = true in create).
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/literal"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (85, 12)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(h, action, at, [])

class \nodoc\ iso _DocHighlightNoneTest is UnitTest
  """
  Placing the cursor on `None` (a type-literal expression) should produce no
  highlights. `None` desugars to `None.create()` synthetically; it has no
  referenceable identity.
  Tests `None` on line 122 col 4 (body of be run in _HighlightRunner).
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/none_literal"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (122, 4)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(h, action, at, [])

class \nodoc\ iso _DocHighlightInnerXFieldTest is UnitTest
  """
  Highlights the `x` fvar field of _Inner from its declaration.
  Expects 2 occurrences:
    line 49 col 6  (var x declaration in _Inner)
    line 52 col 4  (x = 0 assignment in create)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/inner_x_field"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (49, 6)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h,
      action,
      at,
      [(49, 6, 49, 7); (52, 4, 52, 5)])

class \nodoc\ iso _DocHighlightValFieldTest is UnitTest
  """
  Highlights the `_val` fvar field of _HighlightMore from its declaration.
  Expects 5 occurrences:
    line 82 col  6  (_val declaration)
    line 87 col  4  (_val = 0 in create)
    line 92 col  4  (_val = 1 in other)
    line 95 col  4  (_val = ... LHS in add)
    line 95 col 11  (... + _val RHS in add)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/val_field"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (82, 6)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h,
      action,
      at,
      [ (82, 6, 82, 10); (87, 4, 87, 8); (92, 4, 92, 8)
        (95, 4, 95, 8); (95, 11, 95, 15)])

class \nodoc\ iso _DocHighlightDoWorkParamTest is UnitTest
  """
  Highlights the `n` parameter of do_work from its declaration.
  Expects 3 occurrences:
    line 104 col 18  (n param declaration in do_work)
    line 105 col 17  (let v: U32 = n)
    line 107 col 16  (w = w + add(n))
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/do_work_param"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (104, 18)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h,
      action,
      at,
      [(104, 18, 104, 19); (105, 17, 105, 18); (107, 16, 107, 17)])

class \nodoc\ iso _DocHighlightDoWorkLetTest is UnitTest
  """
  Highlights the `v` let local of do_work from its declaration.
  Expects 2 occurrences:
    line 105 col  8  (let v declaration in do_work)
    line 106 col 17  (var w: U32 = v)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/do_work_let"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (105, 8)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h,
      action,
      at,
      [(105, 8, 105, 9); (106, 17, 106, 18)])

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
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/let_ref"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (31, 4)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h,
      action,
      at,
      [(30, 8, 30, 14); (31, 4, 31, 10); (31, 13, 31, 19)])

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
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/var_ref"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (107, 4)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h,
      action,
      at,
      [(106, 8, 106, 9); (107, 4, 107, 5); (107, 8, 107, 9); (108, 4, 108, 5)])

class \nodoc\ iso _DocHighlightParamRefTest is UnitTest
  """
  Highlights `x` from a paramref site (line 95 col 18).
  Verifies that starting from a reference produces the same highlights
  as starting from the declaration (_DocHighlightParamTest).
  Expects 3 occurrences:
    line 94 col 14  (x param declaration in add)
    line 95 col 18  (_val = _val + x — body use)
    line 96 col  4  (x — return value)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/param_ref"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (95, 18)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h,
      action,
      at,
      [(94, 14, 94, 15); (95, 18, 95, 19); (96, 4, 96, 5)])

class \nodoc\ iso _DocHighlightNewRefCallTest is UnitTest
  """
  Highlights `create` from a call site (_Inner.create() on line 86 col 20).
  Verifies that starting from a newref reference produces the same highlights
  as starting from the declaration (_DocHighlightNewRefTest).
  Expects 3 occurrences:
    line 51 col  6  (new create declaration in _Inner)
    line 86 col 20  (_Inner.create() in _HighlightMore.create body)
    line 91 col 20  (_Inner.create() in _HighlightMore.other body)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/new_ref_call"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (86, 20)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(
      h,
      action,
      at,
      [(51, 6, 51, 12); (86, 20, 86, 26); (91, 20, 91, 26)])

class \nodoc\ iso _DocHighlightIntLiteralTest is UnitTest
  """
  Placing the cursor on an integer literal should produce no highlights.
  Tests `0` on line 87 col 11 (_val = 0 in _HighlightMore.create).
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/int_literal"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (87, 11)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(h, action, at, [])

class \nodoc\ iso _DocHighlightFloatLiteralTest is UnitTest
  """
  Placing the cursor on a float literal should produce no highlights.
  Tests `3.14` on line 132 col 16 (let _f: F64 = 3.14 in _LiteralExamples).
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/float_literal"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (132, 16)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(h, action, at, [])

class \nodoc\ iso _DocHighlightStringLiteralTest is UnitTest
  """
  Placing the cursor on a string literal should produce no highlights.
  Tests `"hello"` on line 133 col 23 (in _LiteralExamples).
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/string_literal"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (133, 23)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(h, action, at, [])

class \nodoc\ iso _DocHighlightWhitespaceTest is UnitTest
  """
  Placing the cursor on a blank line should produce no highlights.
  Tests line 22 col 0 (blank line between count field and tick method).
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/whitespace"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (22, 0)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(h, action, at, [])

class \nodoc\ iso _DocHighlightOutOfBoundsTest is UnitTest
  """
  Placing the cursor past the end of a line should produce no highlights.
  Tests line 21 col 100 (past end of `  var count: U32 = 0`).
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/out_of_bounds"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (21, 100)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(h, action, at, [])

class val _PendingDocHighlight
  let file_path: String
  let line: I64
  let character: I64
  let expected: Array[(I64, I64, I64, I64)] val
  let h: TestHelper
  let action: String

  new val create(
    file_path': String,
    line': I64,
    character': I64,
    expected': Array[(I64, I64, I64, I64)] val,
    h': TestHelper,
    action': String)
  =>
    file_path = file_path'
    line = line'
    character = character'
    expected = expected'
    h = h'
    action = action'

actor _DocHighlightLspServer is Channel
  """
  Shared LSP server for all document highlight integration tests.
  Initializes and compiles the workspace once, then dispatches
  individual documentHighlight requests.
  """
  let _workspace_dir: String
  let _fixture_file: String
  var _server: (BaseProtocol | None)
  var _ready: Bool
  var _initialized: Bool
  let _pending: Array[_PendingDocHighlight]
  let _opened: Set[String]
  let _in_flight: Map[I64, _PendingDocHighlight]
  var _next_id: I64

  new create(workspace_dir: String, fixture: String) =>
    _workspace_dir = workspace_dir
    _fixture_file = Path.join(workspace_dir, fixture)
    _server = None
    _ready = false
    _initialized = false
    _pending = Array[_PendingDocHighlight]
    _opened = Set[String]
    _in_flight = Map[I64, _PendingDocHighlight]
    _next_id = 2

  be test_document_highlight(
    h: TestHelper,
    action: String val,
    at: (I64, I64),
    expected: Array[(I64, I64, I64, I64)] val)
  =>
    (let line, let character) = at
    let file_path = _fixture_file
    let pending =
      _PendingDocHighlight(file_path, line, character, expected, h, action)
    if _ready then
      if not _opened.contains(file_path) then
        _opened.set(file_path)
        _did_open(file_path)
      end
      _dispatch(pending)
    else
      _pending.push(pending)
    end
    if not _initialized then
      _initialized = true
      let ponyc = try h.env.args(0)? else "" end
      let proto =
        BaseProtocol(LanguageServer(this, h.env, PonyCompiler("", ponyc)))
      _server = proto
      proto(LspMsg.initialize(_workspace_dir))
    end

  fun ref _dispatch(pending: _PendingDocHighlight) =>
    let id = _next_id
    _next_id = id + 1
    try
      (_server as BaseProtocol)(
        RequestMessage(
          id,
          Methods.text_document().document_highlight(),
          JsonObject
            .update(
              "textDocument",
              JsonObject
                .update("uri", Uris.from_path(pending.file_path)))
            .update(
              "position",
              JsonObject
                .update("line", pending.line)
                .update("character", pending.character))
        ).into_bytes()
      )
      _in_flight(id) = pending
    else
      pending.h.fail_action(pending.action)
    end

  be send(msg: Message val) =>
    match msg
    | let res: ResponseMessage val =>
      try
        let id = res.id as RequestId
        if RequestIds.eq(id, I64(0)) then
          try
            (_server as BaseProtocol)(LspMsg.initialized())
          end
        else
          try
            let id_i64 = id as I64
            (_, let pending) = _in_flight.remove(id_i64)?
            _check_response(res, pending)
          end
        end
      end
    | let req: RequestMessage val =>
      if req.method == Methods.workspace().configuration() then
        try
          let proto = _server as BaseProtocol
          proto(ResponseMessage(req.id, JsonArray).into_bytes())
          for p in _pending.values() do
            if not _opened.contains(p.file_path) then
              _opened.set(p.file_path)
              _did_open(p.file_path)
            end
          end
        end
      end
    | let n: Notification val =>
      if n.method == Methods.text_document().publish_diagnostics() then
        if not _ready then
          _ready = true
          for p in _pending.values() do
            _dispatch(p)
          end
          _pending.clear()
        end
      end
    end

  fun ref _check_response(
    res: ResponseMessage val,
    pending: _PendingDocHighlight)
  =>
    var ok = true
    match res.result
    | let arr: JsonArray =>
      let got = arr.size()
      let want = pending.expected.size()
      if not pending.h.assert_eq[USize](
        want,
        got,
        "Expected " + want.string() + " highlights, got " + got.string())
      then
        ok = false
        // Log all actual ranges to diagnose unexpected highlights
        for item in arr.values() do
          try
            let range = JsonNav(item)("range")
            let sl = range("start")("line").as_i64()?
            let sc = range("start")("character").as_i64()?
            let el = range("end")("line").as_i64()?
            let ec = range("end")("character").as_i64()?
            pending.h.log(
              "  actual highlight (" + sl.string() + ", " + sc.string() +
              ")–(" + el.string() + ", " + ec.string() + ")")
          end
        end
      end
      for (exp_sl, exp_sc, exp_el, exp_ec) in pending.expected.values() do
        var found = false
        for item in arr.values() do
          try
            let range = JsonNav(item)("range")
            let sl = range("start")("line").as_i64()?
            let sc = range("start")("character").as_i64()?
            let el = range("end")("line").as_i64()?
            let ec = range("end")("character").as_i64()?
            if (sl == exp_sl) and (sc == exp_sc) and
              (el == exp_el) and (ec == exp_ec)
            then
              found = true
              break
            end
          end
        end
        if not pending.h.assert_true(
          found,
          "Expected highlight (" + exp_sl.string() + ", " + exp_sc.string() +
          ")–(" + exp_el.string() + ", " + exp_ec.string() + ") not found")
        then
          ok = false
        end
      end
    else
      if pending.expected.size() > 0 then
        ok = false
        pending.h.log("Expected highlights but got null")
      end
    end
    if ok then
      pending.h.complete_action(pending.action)
    else
      pending.h.fail_action(pending.action)
    end

  fun ref _did_open(file_path: String) =>
    try
      (_server as BaseProtocol)(
        Notification(
          Methods.text_document().did_open(),
          JsonObject.update(
            "textDocument",
            JsonObject
              .update("uri", Uris.from_path(file_path))
              .update("languageId", "pony")
              .update("version", I64(1))
              .update("text", ""))
        ).into_bytes())
    end

  be log(data: String val, message_type: MessageType = Debug) =>
    None

  be set_notifier(notifier: Notifier tag) =>
    None

  be dispose() =>
    None
