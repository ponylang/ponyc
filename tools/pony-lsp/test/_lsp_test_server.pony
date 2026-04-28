use ".."
use "pony_test"
use "files"
use "json"
use "collections"

actor _LspTestServer is Channel
  """
  Test-local LSP server for integration tests. Each suite creates its own
  instance so that workspace compilation happens independently per suite.
  The response checker supplied with each request handles method-specific
  dispatch and validation.

  Readiness gate: the server buffers all incoming requests in `_pending` until
  the first `textDocument/publishDiagnostics` notification arrives, which
  signals that the initial workspace compilation is done. Only then are pending
  requests dispatched (and any new requests dispatched immediately). The server
  always sends `publishDiagnostics` after compilation, even for a clean build
  (with an empty diagnostics array).
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
    // id 0 = initialize request; ids 1-99 reserved for server-originated
    // requests (e.g. client/registerCapability, workspace/configuration).
    _next_id = 100

  be request(
    h: TestHelper,
    workspace_file: String,
    checker: _RoundTripCheck val,
    action: String)
  =>
    let file_path = Path.join(_workspace_dir, workspace_file)
    let pending = _PendingRequest(file_path, action, h, checker)
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
      var params =
        JsonObject.update(
          "textDocument",
          JsonObject.update("uri", Uris.from_path(pending.file_path)))
      match pending.checker.lsp_params()
      | let extra: JsonObject =>
        for (k, v) in extra.pairs() do
          params = params.update(k, v)
        end
      end
      (_server as BaseProtocol)(
        RequestMessage(id, pending.checker.lsp_method(), params).into_bytes())
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
          try (_server as BaseProtocol)(LspMsg.initialized()) end
        else
          match \exhaustive\ id
          | let id_i64: I64 =>
            try
              (_, let pending) = _in_flight.remove(id_i64)?
              let action = pending.action
              match res.err
              | let e: ResponseError val =>
                pending.h.log(
                  pending.checker.lsp_method() +
                  ": server error " + e.code.string() +
                  ": " + e.message)
              end
              if pending.checker.check(res, pending.h) then
                pending.h.complete_action(action)
              else
                pending.h.fail_action(action)
              end
            else
              _fail_all_in_flight()
            end
          | let _: String =>
            _fail_all_in_flight()
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
            if not _opened.contains(p.file_path) then
              _opened.set(p.file_path)
              _did_open(p.file_path)
            end
            _dispatch(p)
          end
          _pending.clear()
        end
      end
    end

  fun ref _fail_all_in_flight() =>
    for (_, p) in _in_flight.pairs() do
      p.h.fail_action(p.action)
    end
    _in_flight.clear()

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
  let action: String
  let h: TestHelper
  let checker: _RoundTripCheck val

  new val create(
    file_path': String,
    action': String,
    h': TestHelper,
    checker': _RoundTripCheck val)
  =>
    file_path = file_path'
    action = action'
    h = h'
    checker = checker'

primitive _RunLspChecks
  fun apply(
    h: TestHelper,
    server: _LspTestServer,
    workspace_file: String,
    checks: Array[_RoundTripCheck val] val)
  =>
    h.long_test(10_000_000_000)
    for checker in checks.values() do
      let action: String = (digestof checker).string()
      h.expect_action(action)
      server.request(h, workspace_file, checker, action)
    end
