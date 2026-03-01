use "collections"
use "files"
use "json"
use "workspace"

primitive _Uninitialized is Equatable[_LspState]
  fun string(): String val =>
    "uninitialized"
  fun eq(that: box->_LspState): Bool =>
    that is _Uninitialized

primitive _Initialized is Equatable[_LspState]
  fun string(): String val =>
    "initialized"
  fun eq(that: box->_LspState): Bool =>
    that is _Initialized

primitive _ShuttingDown is Equatable[_LspState]
  fun string(): String val =>
    "shutting down"
  fun eq(that: box->_LspState): Bool =>
    that is _ShuttingDown

type _LspState is (_Uninitialized | _Initialized | _ShuttingDown)

actor LanguageServer is (Notifier & RequestSender)
  let _channel: Channel
  let _compiler: LspCompiler
  let _router: WorkspaceRouter
  let _env: Env
  let _file_auth: FileAuth

  // current LSP state
  var _state: _LspState = _Uninitialized
  var _expect_responses_for: Array[(RequestId, String)]
  var _request_id_gen: I64
  var _client: (Client | None)

  new create(channel': Channel, env': Env, compiler': LspCompiler) =>
    channel'.set_notifier(this)
    _channel = channel'
    _compiler = compiler'
    _router = WorkspaceRouter.create()
    _env = env'
    _file_auth = FileAuth(env'.root)
    _expect_responses_for = _expect_responses_for.create(4)
    _request_id_gen = I64(0)
    _client = None
    //_channel.log("PonyLSP Server ready")

  fun tag _get_document_uri(
    params: (JsonObject | JsonArray | None)): String ?
  =>
    JsonNav(params)("textDocument")("uri").as_string()?

  be handle_parse_error(err: ParseError val) =>
    this._channel.log("Parse Error: " + err.string(), Warning)

  fun ref handle_request(r: RequestMessage val) =>
    match this._state
    | _Uninitialized =>
      match r.method
      | Methods.initialize() => this.handle_initialize(r)
      else
        this._channel.send(
          ResponseMessage.create(
            r.id,
            None,
            ResponseError(ErrorCodes.server_not_initialized(), "Expected initialize, got " + r.method)
          )
        )
      end
    | _Initialized =>
      this._channel.log("\n\n<-\n" + r.json().string())
      match \exhaustive\ r.method
      | Methods.text_document().definition() =>
        try
          let document_uri = _get_document_uri(r.params)?
          // TODO: exptract params into class according to spec
          (_router.find_workspace(document_uri) as WorkspaceManager).goto_definition(document_uri, r)
        else
          this._channel.send(
            ResponseMessage.create(
              r.id,
              None,
              ResponseError(ErrorCodes.internal_error(), "[" + r.method + "] No workspace found for '" + r.json().string() + "'")
            )
          )
        end
      | Methods.text_document().hover() =>
        try
          let document_uri = _get_document_uri(r.params)?
          // TODO: exptract params into class according to spec
          (_router.find_workspace(document_uri) as WorkspaceManager).hover(document_uri, r)
        else
          this._channel.send(
            ResponseMessage.create(
              r.id,
              None,
              ResponseError(ErrorCodes.internal_error(), "[" + r.method + "] No workspace found for request '" + r.json().string() + "'")
            )
          )
        end
      | Methods.text_document().document_symbol() =>
        try
          let document_uri = _get_document_uri(r.params)?
          (_router.find_workspace(document_uri) as WorkspaceManager).document_symbols(document_uri, r)
        else
          this._channel.send(
            ResponseMessage.create(
              r.id,
              None,
              ResponseError(ErrorCodes.internal_error(), "[" + r.method + "] No workspace found for request '" + r.json().string() + "'")
            )
          )
        end
      | Methods.text_document().diagnostic() =>
        try
          let document_uri = _get_document_uri(r.params)?
          (_router.find_workspace(document_uri) as WorkspaceManager).document_diagnostic(document_uri, r)
        else
          this._channel.send(
            ResponseMessage.create(
              r.id,
              None,
              ResponseError(ErrorCodes.internal_error(), "[" + r.method + "] No workspace found for request '" + r.json().string() + "'")
            )
          )
        end
      | Methods.shutdown() =>
        this._state = _ShuttingDown
        this._channel.send(ResponseMessage.create(r.id, None))
      else
        this._channel.send(
          ResponseMessage.create(
            r.id,
            None,
            ResponseError(ErrorCodes.method_not_found(), "Method not implemented: " + r.method)
          )
        )
      end
    | _ShuttingDown =>
      // we don't handle no requests no more
      this._channel.send(
        ResponseMessage.create(
          r.id,
          None,
          ResponseError(ErrorCodes.invalid_request(), "shutting down")
        )
      )
    end

  fun ref handle_response(r: ResponseMessage val) =>
    match this._state
    | _Initialized | _Uninitialized =>
      for i in Range[USize](0, this._expect_responses_for.size()) do
        try
          (let expected_request_id, let expected_method) = this._expect_responses_for(i)?
          if
            try
              RequestIds.eq(expected_request_id, (r.id as RequestId))
            else
              false
            end
          then
            // we got a matching response
            this._expect_responses_for.delete(i)?
            // handle the response
            match \exhaustive\ expected_method
            | Methods.workspace().configuration() =>
              this.handle_configuration(r)
            | Methods.client().register_capability() =>
              this.handle_register_capability_response(r)
            | let other: String =>
              this._channel.log("Unhandled response method " + other)
            end
          else
            this._channel.log("Unhandled response\n" + r.json().string())
          end
        end
      else
        this._channel.log("Not expecting any response")
      end
    | _ShuttingDown =>
      // ignore
      None
    end

  fun ref handle_notification(n: Notification val) =>
    match n.method
    | Methods.initialized() => handle_initialized(n)
    | Methods.exit() =>
      this._channel.log("Exiting.")
      this._channel.dispose() // lets hope this unregisters us as dirty
      this._env.exitcode(if this._state is _ShuttingDown then 0 else 1 end)
      return
    | Methods.text_document().did_open() =>
        try
          let document_uri = _get_document_uri(n.params)?
          // TODO: extract params into class according to spec
          (_router.find_workspace(document_uri) as WorkspaceManager).did_open(document_uri, n)
        else
          this._channel.log("[" + n.method + "] No workspace found for '" + n.json().string() + "'")
        end
    | Methods.text_document().did_save() =>
      try
        let document_uri = _get_document_uri(n.params)?
        // TODO: extract params into class according to spec
        (_router.find_workspace(document_uri) as WorkspaceManager).did_save(document_uri, n)
      else
        this._channel.log("[" + n.method + "] No workspace found for '" + n.json().string() + "'")
      end
    | Methods.text_document().did_close() =>
      try
        let document_uri = _get_document_uri(n.params)?
        // TODO: extract params into class according to spec
        (_router.find_workspace(document_uri) as WorkspaceManager).did_close(document_uri, n)
      else
        this._channel.log("[" + n.method + "] No workspace found for '" + n.json().string() + "'")
      end
    | Methods.workspace().did_change_configuration() =>
      try
        let settings = JsonPathParser.compile("$.settings")?.query_one(n.params) as JsonObject
        this.handle_did_change_configuration(settings)
      else
        this._channel.log("[" + n.method + "] Invalid or missing settings")
      end
    else
      this._channel.log("Method not implemented: " + n.method)
    end

  be handle_message(msg: Message val) =>
    match msg
    | let r: RequestMessage val =>
      handle_request(r)
    | let n: Notification val =>
      handle_notification(n)
    | let r: ResponseMessage val =>
      handle_response(r)
    end

  fun ref handle_initialize(msg: RequestMessage val) =>
    match msg.params
    | let params: JsonObject val =>
      // extract server_options from "initializationOptions"
      let server_options =
        try
          ServerOptions.from_json(params("initializationOptions")? as JsonObject)
        end
      let client = Client.from(params)
      this._client = client
      this._channel.log("Connected to client: " + client.string())
      // extract workspace folders, rootUri, rootPath in that order:
      let found_workspace: JsonValue =
        try
          JsonPathParser.compile("$['workspaceFolders','rootUri','rootPath']")?.query(params)(0)?
        else
          None
        end
      let scanner = WorkspaceScanner.create(this._channel)
      match found_workspace
      | let workspace_str: String =>
          try
            this._channel.log("Scanning workspace " + workspace_str)
            let pony_workspaces = scanner.scan(this._file_auth, workspace_str)
            for pony_workspace in pony_workspaces.values() do
              let mgr = WorkspaceManager.create(pony_workspace, this._file_auth, this._channel, this, client, this._compiler)
              this._channel.log("Adding workspace " + pony_workspace.debug())
              this._router.add_workspace(pony_workspace.folder, mgr)?
            end
          end
      | let workspace_arr: JsonArray =>
        for workspace_obj in workspace_arr.values() do
          try
            let obj = workspace_obj as JsonObject
            let name = obj("name")? as String
            let uri = obj("uri")? as String
            this._channel.log("Scanning workspace " + uri)
            for pony_workspace in scanner.scan(this._file_auth, Uris.to_path(uri), name).values() do
              let mgr = WorkspaceManager.create(pony_workspace, this._file_auth, this._channel, this, client, this._compiler)
              this._channel.log("Adding workspace " + pony_workspace.debug())
              this._router.add_workspace(pony_workspace.folder, mgr)?
            end
          end
        end
      end
      // update our request-id-gen to the first message we received so we won't
      // repeat it
      match msg.id
      | let msg_id: I64 =>
        this._request_id_gen = msg_id + 1
      end
      this._channel.send(
        ResponseMessage.create(
          msg.id,
          JsonObject
            .update("capabilities", JsonObject
              // vscode only supports UTF-16, but pony positions are only counting bytes
              // .update("positionEncoding", "utf-8")
              // we can handle hover requests
              .update("hoverProvider", true)
              .update("textDocumentSync", JsonObject
                .update("change", I64(0))
                .update("openClose", true)
                .update("save", JsonObject
                // we need the document contents upon save to determine changes since the previous state compilation picked up
                .update("includeText", true)))
              .update("definitionProvider", true)
              .update("diagnosticProvider", JsonObject
                .update("identifier", "pony-lsp")
                .update("interFileDependencies", true)
                .update("workspaceDiagnostics", false)
              )
              .update("documentSymbolProvider", true))
            .update("serverInfo", JsonObject
              .update("name", "Pony Language Server")
              .update("version", PonyLspVersion.version_string()))
        )
      )
      this._state = _Initialized
    end

  fun ref _next_request_id(): RequestId =>
    this._request_id_gen = this._request_id_gen + 1

  fun ref handle_initialized(notification: Notification val) =>
    // register for did change configuration, if supported
    var can_get_settings = false
    try
      if (this._client as Client).supports_configuration_dynamic_registration() then
        this.register_capability(Methods.workspace().did_change_configuration(), None)
        can_get_settings = true
      end
    end
    // request configuration, if supported
    try
      if (this._client as Client).supports_configuration() then
        this.send_configuration_request()
        can_get_settings = true
      end
    end
    // if none of these work, we have no way to get our config :(
    if not can_get_settings then
      this._channel.log("Cannot get ponyc settings workspace." where message_type = Warning)
    end

  fun ref handle_did_change_configuration(params: JsonObject val) =>
    // example output: for settings: {"pony-lsp":{"defines":["FOO","BAR"]}}
    try
      let pony_lsp_settings = params("pony-lsp")? as JsonObject
      let settings = Settings.from_json(pony_lsp_settings)
      this._channel.log("Received didChangeConfiguration response: " + settings.to_json().string())
      this._compiler.apply_settings(settings)
    else
      this._channel.log("Invalid didChangeConfiguration response")
    end

  fun handle_configuration(r: ResponseMessage val) =>
    try
      // LSPAny[]
      let json_settings = (r.result as JsonArray)(0)? as JsonObject
      this._channel.log("Received workspace/configuration response = " + json_settings.string())

      this._compiler.apply_settings(Settings.from_json(json_settings))
    end

  fun handle_register_capability_response(r: ResponseMessage val) =>
    // TODO: handle
    None

  be send_request(method: String val, params: (JsonObject | JsonArray | None)) =>
    let request_id: RequestId = this._next_request_id()
    this._channel.send(
      RequestMessage.create(
        request_id,
        method,
        params
      )
    )
    this._expect_responses_for.push((request_id, method))

  be register_capability(register_for_method: String val, register_options: JsonValue) =>
    """
    Register to the LSP client for a certain capability/method.

    See: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#client_registerCapability
    """
    let id = register_for_method.hash()
    this.send_request(
      Methods.client().register_capability(),
      JsonObject
        .update("registrations", JsonArray.push(
          JsonObject
            .update("id", id.string())
            .update("method", register_for_method)
            .update("registerOptions", register_options)
        )
      )
    )

  be send_configuration_request() =>
    """
    Get all `pony-lsp`-scoped settings from the LSP client (editor).
    """
    this.send_request(
      Methods.workspace().configuration(),
      JsonObject
        .update("items", JsonArray.push(
          JsonObject.update("section", "pony-lsp")
        )
      )
    )

  be dispose() =>
    this._router.dispose()
    this._channel.dispose()
