use ".."
use "pony_test"
use "files"
use "json"
use "collections"

actor _LspTestServer is Channel
  """
  Shared LSP server for integration tests. Initializes and compiles the
  workspace once, then dispatches individual requests to each test's TestHelper.
  The response checker supplied with each request handles method-specific
  dispatch and validation.
  """
  let _workspace_dir: String
  var _server: (BaseProtocol | None)
  var _ready: Bool
  var _initialized: Bool
  let _pending: Array[_PendingRequest]
  let _opened: Set[String]
  let _in_flight: Map[I64, _PendingRequest]
  var _next_id: I64

  new create(workspace_dir: String) =>
    _workspace_dir = workspace_dir
    _server = None
    _ready = false
    _initialized = false
    _pending = Array[_PendingRequest]
    _opened = Set[String]
    _in_flight = Map[I64, _PendingRequest]
    _next_id = 2

  be request(
    h: TestHelper,
    pos: _LspPosition val,
    checker: _ResponseChecker val)
  =>
    let file_path = Path.join(_workspace_dir, pos.workspace_file)
    let pending = _PendingRequest(file_path, pos, h, checker)
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

  fun ref _dispatch(pending: _PendingRequest) =>
    let id = _next_id
    _next_id = id + 1
    try
      (_server as BaseProtocol)(
        RequestMessage(
          id,
          pending.checker.lsp_method(),
          JsonObject
            .update(
              "textDocument",
              JsonObject.update("uri", Uris.from_path(pending.file_path)))
            .update(
              "position",
              JsonObject
                .update("line", pending.pos.line)
                .update("character", pending.pos.character))
        ).into_bytes()
      )
      _in_flight(id) = pending
    else
      pending.h.fail_action(pending.pos.action())
    end

  be send(msg: Message val) =>
    match msg
    | let res: ResponseMessage val =>
      try
        let id = res.id as RequestId
        if RequestIds.eq(id, I64(0)) then
          try (_server as BaseProtocol)(LspMsg.initialized()) end
        else
          try
            let id_i64 = id as I64
            (_, let pending) = _in_flight.remove(id_i64)?
            let action = pending.pos.action()
            if pending.checker.check(res, pending.h) then
              pending.h.complete_action(action)
            else
              pending.h.fail_action(action)
            end
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

class val _PendingRequest
  let file_path: String
  let pos: _LspPosition val
  let h: TestHelper
  let checker: _ResponseChecker val

  new val create(
    file_path': String,
    pos': _LspPosition val,
    h': TestHelper,
    checker': _ResponseChecker val)
  =>
    file_path = file_path'
    pos = pos'
    h = h'
    checker = checker'

primitive _RunLspChecks
  fun apply(
    h: TestHelper,
    server: _LspTestServer,
    workspace_file: String,
    checks: Array[(I64, I64, _ResponseChecker val)] val)
  =>
    h.long_test(10_000_000_000)
    for (line, character, checker) in checks.values() do
      let pos = _LspPosition(workspace_file, line, character)
      h.expect_action(pos.action())
      server.request(h, pos, checker)
    end
