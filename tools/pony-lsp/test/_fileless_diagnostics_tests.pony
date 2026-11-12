use ".."
use ws = "../workspace"
use "files"
use "json"
use "pony_compiler"
use "pony_test"

use @ast_source[NullablePointer[_DiagnosticSource]](ast: USize)
use @strcmp[I32](left: Pointer[U8] tag, right: Pointer[U8] tag)

struct \nodoc\ _DiagnosticSource
  var file: Pointer[U8] tag = Pointer[U8]

primitive \nodoc\ _DiagnosticErrors
  fun apply(
    h: TestHelper,
    dir: FilePath,
    change_file: String = "",
    filename: (String | None) = None)
    : Array[Error] val ?
  =>
    let session = CompileSession(dir, [_stdlib()] where limit = PassParse)
    try
      let program = session.program() as Program
      let module = program.module() as Module
      var source = NullablePointer[_DiagnosticSource].none()
      for file in module.files() do
        if Path.base(file.file) == change_file then
          source = @ast_source(file.ast.raw.usize())
          let pointer = source()?.file
          if pointer.is_null() or
            not h.assert_eq[I32](0, @strcmp(pointer, file.file.cstring()))
          then
            error
          end
        end
      end
      var saved: Pointer[U8] tag = Pointer[U8]
      if not source.is_none() then
        let s = source()?
        saved = s.file
        s.file = match filename
          | let path: String => path.cstring()
          | None => recover val Pointer[U8] end
          end
      else
        h.assert_eq[String]("", change_file)
      end
      h.assert_false(session.continue_to(PassFinaliser))
      let errors = session.errors()
      if not source.is_none() then
        source()?.file = saved
      end
      session.dispose()
      errors
    else
      session.dispose()
      error
    end

  fun _stdlib(): String =>
    Path.join(Path.dir(__loc.file()), "../../../packages")

class \nodoc\ iso _CompilerDiagnosticTest is UnitTest
  fun name(): String => "diagnostics/optional_compiler_location"

  fun apply(h: TestHelper) ? =>
    h.assert_true(CompilerDiagnostic(Error.message("no file")) is None)
    let dir = FilePath(FileAuth(h.env.root),
      Path.join(Path.dir(__loc.file()), "error_workspace"))
    let main_uri = Uris.from_path(Path.join(dir.path, "main.pony"))
    let errors = _DiagnosticErrors(h, dir)?
    let root = CompilerDiagnostic(errors(0)?) as Diagnostic
    h.assert_eq[String](main_uri, root.file_uri())
    h.assert_eq[String]("argument not assignable to parameter", root.message)
    h.assert_eq[I64](1, root.severity)
    let range = JSONNav(root.to_json())("range")
    h.assert_eq[I64](2, range("start")("line").as_i64()?)
    h.assert_eq[I64](36, range("start")("character").as_i64()?)
    h.assert_eq[I64](2, range("end")("line").as_i64()?)
    h.assert_eq[I64](36, range("end")("character").as_i64()?)
    h.assert_eq[USize](3, root.related_information.size())

    let empty = _DiagnosticErrors(h, dir, "main.pony", "")?
    h.assert_eq[String]("", empty(0)?.file as String)
    h.assert_true(CompilerDiagnostic(empty(0)?) is None)
    let missing = _DiagnosticErrors(h, dir, "main.pony")?
    h.assert_true(missing(0)?.file is None)
    h.assert_true(CompilerDiagnostic(missing(0)?) is None)
    h.assert_eq[USize](3, missing(0)?.infos.size())

    for filename in [as (String | None): None; ""].values() do
      let fallback = _DiagnosticErrors(h, dir, "frombnicator.pony", filename)?
      let diag = CompilerDiagnostic(fallback(0)?) as Diagnostic
      let info = diag.related_information(1)?
      h.assert_eq[String](main_uri, info.location.uri)
      h.assert_eq[String]("parameter type requires U8 val^", info.message)
      let info_range = JSONNav(info.location.range.to_json())
      h.assert_eq[I64](6, info_range("start")("line").as_i64()?)
      h.assert_eq[I64](13, info_range("start")("character").as_i64()?)
    end

class \nodoc\ _FilelessDiagnosticTest is WsUnitTest
  let _publish: Bool
  let _refresh: Bool

  new create(publish: Bool, refresh: Bool) =>
    _publish = publish
    _refresh = refresh

  fun name(): String =>
    "diagnostics/fileless_lifecycle/publish=" + _publish.string() +
      "/refresh=" + _refresh.string()

  fun setup_workspace(builder: WorkspaceBuilder) =>
    builder.file("main.pony",
      "actor Main\n  new create(env: Env) =>\n    Foo(\"bad\")\n")
    builder.file("foo.pony", "class Foo\n  new create(number: U8) => None\n")
    builder.dir("other")

  fun apply(dir: FilePath, h: TestHelper) ? =>
    h.long_test(20_000_000_000)
    let located = _DiagnosticErrors(h, dir)?
    let fileless = _DiagnosticErrors(h, dir, "main.pony")?
    let main_path = FilePath.from(dir, "main.pony")?
    with file = File(main_path) do
      file.set_length(0)
      file.write("actor Main\n  new create(env: Env) =>\n    Foo(1)\n")
    end
    let program = Compiler.compile(dir, [_DiagnosticErrors._stdlib()]
      where limit = PassFinaliser) as Program
    let auth = FileAuth(h.env.root)
    let folders = ws.WorkspaceFolders(auth, [dir.path])?
    _DiagnosticObserver(h, auth, folders, dir, located, fileless, program,
      _publish, _refresh)

actor \nodoc\ _DiagnosticObserver is (Channel & RequestSender)
  let _h: TestHelper
  let _dir: FilePath
  let _publish: Bool
  let _refresh: Bool
  let _notifications: Array[Notification] = Array[Notification]
  var _refreshes: USize = 0
  var _phase: I64 = 1

  new create(
    h: TestHelper,
    auth: FileAuth,
    folders: ws.WorkspaceFolders,
    dir: FilePath,
    located: Array[Error] val,
    fileless: Array[Error] val,
    program: Program,
    publish: Bool,
    refresh: Bool)
  =>
    _h = h
    _dir = dir
    _publish = publish
    _refresh = refresh
    let caps = JSONObject
      .update("workspace", JSONObject.update("diagnostics",
        JSONObject.update("refreshSupport", refresh)))
      .update("textDocument", if publish then
        JSONObject.update("publishDiagnostics", JSONObject)
        else JSONObject end)
    let manager = ws.WorkspaceManager(folders, auth, this, this,
      Client.from(JSONObject.update("capabilities", caps)), TestCompiler(h))
    let mixed = recover val
      Array[Error]
        .>append(fileless)
        .>push(Error.message("second fileless error"))
        .>append(located)
    end
    manager.done_compiling(dir, located, 1)
    _barrier(manager, 1)
    manager.done_compiling(dir, mixed, 2)
    _barrier(manager, 2)
    manager.done_compiling(FilePath(auth, Path.join(dir.path, "other")),
      [Error.message("same failure")], 3)
    _barrier(manager, 3)
    manager.done_compiling(dir, program, 4)
    manager.done_compiling(dir, [Error.message("stale failure")], 3)
    manager.done_compiling(dir, [Error.message("equal failure")], 4)
    _barrier(manager, 4)

  fun _barrier(manager: ws.WorkspaceManager, id: I64) =>
    manager.document_diagnostic(Uris.from_path(Path.join(_dir.path, "main.pony")),
      RequestMessage(id, Methods.text_document().diagnostic(), JSONObject))

  be send(msg: Message val) =>
    match msg
    | let notification: Notification => _notifications.push(notification)
    | let response: ResponseMessage =>
      try
        _h.assert_true(RequestIds.eq(_phase, response.id as RequestId))
        _check(response)?
      else
        _h.fail("Invalid diagnostic response: " + response.string())
      end
      _notifications.clear()
      _refreshes = 0
      _phase = _phase + 1
      if _phase == 5 then _h.complete(true) end
    end

  fun ref _check(response: ResponseMessage) ? =>
    _h.assert_eq[String](if _phase < 4 then _phase.string() else "4" end,
      JSONNav(response.result)("resultId").as_string()?)
    if _phase == 4 then
      _h.assert_eq[USize](0, JSONNav(response.result)("items").as_array()?.size())
    end
    var shown: USize = 0
    var published: USize = 0
    var diagnostic_count: USize = 0
    for notification in _notifications.values() do
      let nav = JSONNav(notification.params)
      match notification.method
      | Methods.window().show_message() =>
        shown = shown + 1
        _h.assert_eq[I64](1, nav("type").as_i64()?)
        let expected: String val = if _phase == 2 then
          _dir.path + ":\nargument not assignable to parameter\n" +
            "argument type is String val\n" +
            Path.join(_dir.path, "foo.pony") +
            ":2:14: parameter type requires U8 val^\n" +
            "String val is not a subtype of U8 val^\nsecond fileless error"
          else Path.join(_dir.path, "other") + ":\nsame failure" end
        _h.assert_eq[String](expected, nav("message").as_string()?)
      | Methods.text_document().publish_diagnostics() =>
        published = published + 1
        let uri = nav("uri").as_string()?
        _h.assert_true(
          (uri == Uris.from_path(Path.join(_dir.path, "main.pony"))) or
          (uri == Uris.from_path(Path.join(_dir.path, "foo.pony"))), uri)
        let diagnostics = nav("diagnostics").as_array()?
        if _phase >= 3 then _h.assert_eq[USize](0, diagnostics.size()) end
        diagnostic_count = diagnostic_count + diagnostics.size()
      else
        _h.fail("Unexpected notification: " + notification.string())
      end
    end
    _h.assert_eq[USize](if (_phase == 2) or (_phase == 3) then 1 else 0 end,
      shown)
    _h.assert_eq[USize](if _publish then 2 else 0 end, published)
    _h.assert_eq[USize](
      if _publish and (_phase <= 2) then 4 else 0 end, diagnostic_count)
    _h.assert_eq[USize](if (not _publish) and _refresh then 1 else 0 end,
      _refreshes)

  be send_request(
    method: String,
    params: (JSONObject | JSONArray | None),
    notify: (ResponseNotify | None) = None)
  =>
    _h.assert_eq[String](Methods.workspace().diagnostic().refresh(), method)
    _refreshes = _refreshes + 1

  be log(data: String, message_type: MessageType = Debug) => None
  be set_notifier(notifier: Notifier tag) => None
  be dispose() => None
