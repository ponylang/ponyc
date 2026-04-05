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
    test(_DocHighlightClassTypeTest.create(server, fixture))
    test(_DocHighlightTypeRefTest.create(server, fixture))
    test(_DocHighlightLiteralTest.create(server, fixture))

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
      h, action, at, [(21, 6); (24, 4); (24, 12); (27, 4); (30, 22)])

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
      h, action, at, [(21, 6); (24, 4); (24, 12); (27, 4); (30, 22)])

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
    _server.test_document_highlight(h, action, at, [(30, 8); (31, 4); (31, 13)])

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
    _server.test_document_highlight(h, action, at, [(80, 6); (85, 4); (90, 4)])

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
    _server.test_document_highlight(h, action, at, [(81, 8); (86, 4); (91, 4)])

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
      h, action, at, [(51, 6); (86, 20); (91, 20)])

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
      h, action, at, [(94, 10); (102, 15); (107, 12)])

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
      h, action, at, [(94, 14); (95, 18); (96, 4)])

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
      h, action, at, [(106, 8); (107, 4); (107, 8); (108, 4)])

class \nodoc\ iso _DocHighlightBeRefTest is UnitTest
  """
  Highlights the `run` behaviour from its declaration.
  Expects 2 occurrences:
    line 117 col  5  (be run declaration)
    line 121 col  4  (run(1) call in trigger)
  """
  let _server: _DocHighlightLspServer
  let _fixture: String val

  new iso create(server: _DocHighlightLspServer, fixture: String val) =>
    _server = server
    _fixture = fixture

  fun name(): String =>
    "document_highlight/integration/be_ref"

  fun apply(h: TestHelper) =>
    let at: (I64, I64) = (117, 5)
    (let line, let character) = at
    let action: String val =
      recover _fixture + ":" + line.string() + ":" + character.string() end
    h.long_test(10_000_000_000)
    h.expect_action(action)
    _server.test_document_highlight(h, action, at, [(117, 5); (121, 4)])

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
      h, action, at, [(33, 6); (81, 16); (86, 13); (91, 13)])

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
    _server.test_document_highlight(h, action, at, [(54, 6); (98, 22)])

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

class val _PendingDocHighlight
  let file_path: String
  let line: I64
  let character: I64
  let expected: Array[(I64, I64)] val
  let h: TestHelper
  let action: String

  new val create(
    file_path': String,
    line': I64,
    character': I64,
    expected': Array[(I64, I64)] val,
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
    expected: Array[(I64, I64)] val)
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
        // Log all actual positions to diagnose unexpected highlights
        for item in arr.values() do
          try
            let start = JsonNav(item)("range")("start")
            let l = start("line").as_i64()?
            let c = start("character").as_i64()?
            pending.h.log(
              "  actual highlight at (" +
              l.string() + ", " + c.string() + ")")
          end
        end
      end
      for (exp_line, exp_char) in pending.expected.values() do
        var found = false
        for item in arr.values() do
          try
            let start = JsonNav(item)("range")("start")
            let l = start("line").as_i64()?
            let c = start("character").as_i64()?
            if (l == exp_line) and (c == exp_char) then
              found = true
              break
            end
          end
        end
        if not pending.h.assert_true(
          found,
          "Expected highlight at line " +
          exp_line.string() + " col " +
          exp_char.string() + " not found")
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
