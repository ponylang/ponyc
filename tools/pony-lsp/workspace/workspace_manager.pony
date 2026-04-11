use ".."
use "collections"
use pc = "collections/persistent"
use "files"
use "itertools"

use "pony_compiler"
use "json"

actor WorkspaceManager
  """
  Handling all operations on a workspace.

  A workspace is initially created by scanning its directory
  and extracting information from available `corral.json` files.

  More information is obtained during execution of the type-checking
  pipeline of the libponyc compiler. After first successful compilation
  we should have all information at hand to do LSP work.
  """

  let workspace: WorkspaceData
  let _file_auth: FileAuth
  let _client: Client
  let _channel: Channel
  let _request_sender: RequestSender
  let _packages: Map[String, PackageState]
  let _global_errors: Array[Diagnostic val]
  let _compiler: LspCompiler
  // we get notified about all passes, mostly for progress reporting
  let _compile_passes: Array[PassId] val = [
    PassParse
    PassSyntax
    PassSugar
    PassScope
    PassImport
    PassNameResolution
    PassFlatten
    PassTraits
    PassRefer
    PassExpr
    PassCompleteness
    PassVerify
    PassFinaliser
  ]
  var _compile_run: USize = 0
  var _compiling: Bool = false
  var _current_request: (RequestMessage val | None) = None
  var _awaiting_compilation_for: Map[String, (USize | None)] =
    _awaiting_compilation_for.create()
    """
    mapping of file_path to its hash (if any) that
    got saved while a compilation was still running
    """

  new create(
    workspace': WorkspaceData,
    file_auth': FileAuth,
    channel': Channel,
    request_sender': RequestSender,
    client': Client,
    compiler': LspCompiler)
  =>
    workspace = workspace'
    _file_auth = file_auth'
    _client = client'
    _channel = channel'
    _request_sender = request_sender'
    _compiler = compiler'
    _packages = _packages.create()
    _global_errors =
      Array[Diagnostic val].create(4)

  fun ref _ensure_package(package_path: FilePath): PackageState =>
    try
      this._packages(package_path.path)?
    else
      this._packages.insert(
        package_path.path,
        PackageState.create(package_path, this._channel))
    end

  fun _get_package(package_path: FilePath ): (this->PackageState | None) =>
    try
      this._packages(package_path.path)?
    else
      None
    end

  fun box _find_workspace_package(document_path: String): FilePath ? =>
    """
    Try to find the package in the workspace from the
    given `document_path` by iterating up the directory
    tree to see if any dir belongs to one of the
    packages of the workspace.

    Errors if no package could be found.
    """
    var dir_path = Path.dir(document_path)
    while (dir_path != ".") do
      if this._packages.contains(dir_path) then
        return
          if Path.is_abs(dir_path) then
            FilePath(this._file_auth, dir_path)
          else
            this.workspace.folder.join(dir_path)?
          end
      end
      let parent = Path.dir(dir_path)
      if parent == dir_path then
        break // hit filesystem root
      end
      dir_path = parent
    end
    error

  fun _progress_report_nofitication(package: FilePath, pass: PassId): Notification =>
    let token = this._compilation_token(package)
    (let percentage: I64, let message: String) = match pass
    | PassParse =>
      (I64(5), "Pass: " + PassSyntax.string())
    | PassSyntax =>
      (I64(10), "Pass: " + PassSugar.string())
    | PassSugar =>
      (I64(15), "Pass: " + PassScope.string())
    | PassScope =>
      (I64(20), "Pass: " + PassImport.string())
    | PassImport =>
      (I64(25), "Pass: " + PassNameResolution.string())
    | PassNameResolution =>
      (I64(30), "Pass: " + PassFlatten.string())
    | PassFlatten =>
      (I64(35), "Pass: " + PassTraits.string())
    | PassTraits =>
      (I64(40), "Pass: " + PassRefer.string())
    | PassRefer =>
      (I64(50), "Pass: type-checking") // more meaningful than expr
    | PassExpr =>
      (I64(85), "Pass: " + PassCompleteness.string())
    | PassCompleteness =>
      (I64(90), "Pass: " + PassVerify.string())
    | PassVerify =>
      (I64(95), "Pass: " + PassFinaliser.string())
    | PassFinaliser =>
      (I64(100), "Done")
    else
      // shouldn't happen
      (I64(0), "")
    end
    Notification(
      Methods.progress(),
      JsonObject
        .update("token", token)
        .update(
          "value",
          JsonObject
            .update("kind", "report")
            .update(
              "title",
              "Compiling " + Path.base(package.path) +
              " (" + this._compile_run.string() + ")")
            .update("message", message)
            .update("percentage", percentage)
            .update("cancellable", false)
        )
    )

  be on_compile_result(
    program_dir: FilePath,
    run: USize,
    pass: PassId,
    result: (Program val | Array[Error val] val))
  =>
    """
    Handle compilation completion.
    """
    if run > this._compile_run then
      // notify client about progress
      let notification = this._progress_report_nofitication(program_dir, pass)
      if this._client.supports_window_work_done_progress() then
        this._channel.send(notification)
      end

      // group errors by file
      let errors_by_file = Map[String, pc.Vec[JsonValue]].create()

      let done_compiling =
        match (result, pass)
        | (let program: Program val, PassFinaliser) | (let errors: Array[Error val] val, _) => true
        else
          false
        end

      // preparations when compilation is done - on error or success
      if done_compiling then
        if this._client.supports_window_work_done_progress() then
          let message =
            match \exhaustive\ result
            | let p: Program val => "Success"
            | let errors: Array[Error val] val => errors.size().string() + " Errors"
            end
          this._channel.send(
            Notification(
              Methods.progress(),
              JsonObject
                .update("token", this._compilation_token(program_dir))
                .update("value", JsonObject
                  .update("kind", "end")
                  .update("message", message))
            )
          )
        end
        // mark that we are done with compilation
        this._compiling = false

        this._channel.log(
          "done compilation run " + run.string() +
          " of " + program_dir.path)

        // ignore older compile runs
        this._compile_run = run.max(this._compile_run)

        if this._client.supports_publish_diagnostics() then
          // pre-fill with empty list of errors for
          // all files for which we have errors now
          for pkg in this._packages.values() do
            for doc in pkg.documents.keys() do
              errors_by_file(doc) = pc.Vec[JsonValue]
            end
          end
        end

        // clear out global errors
        this._global_errors.clear()
      end

      // handle compilation result, if compilation is fully done (last pass) or not
      match (result, pass)
      | (let program: Program val, PassParse) =>
        // parsing was successful
        // TODO: use parse pass data already to pre-fill some package and module states
        None
      | (let program: Program val, PassFinaliser) =>
        this._channel.log("Successfully compiled " + program_dir.path)
        // create/update package states for every
        // package being part of the program
        for package in program.packages() do
          let package_path = FilePath(this._file_auth, package.path)
          let package_state = this._ensure_package(package_path)
          package_state.update(package, run)

          var requires_another_compilation = false
          for (await_comp_file, await_comp_hash) in
            this._awaiting_compilation_for.pairs()
          do
            let await_pkg_path = Path.base(await_comp_file)
            if package_state.path.path == await_pkg_path then
              try
                // get the hash of the module file
                // ponyc considered for compilation
                let new_mod_hash =
                  (package_state.get_document(await_comp_file) as DocumentState)
                    .module_hash()
                this._awaiting_compilation_for.remove(await_comp_file)?
                requires_another_compilation =
                  requires_another_compilation or
                    match \exhaustive\ (await_comp_hash, new_mod_hash)
                    | (None, None) => false
                    | (None, let h: USize)
                    | (let h: USize, None) => true
                    | (let old_hash: USize, let new_hash: USize) =>
                      old_hash != new_hash
                    end
              end
            end
          end
          if requires_another_compilation then
            this._channel.log(
              program_dir.path +
              " needs another compilation...")
            this._compile(program_dir)
          end
        end
      | (let errors: Array[Error val] val, let err_pass: PassId) =>
        this._channel.log(
          "Compilation failed with " +
          errors.size().string() + " errors after pass " + err_pass.string())

        let diag_iter =
          Iter[Error](errors.values())
            .map[Diagnostic]({(err): Diagnostic => Diagnostic.from_error(err) })
            .flat_map[Diagnostic](
              {(diag): Iterator[Diagnostic] =>
                diag.expand_related().values()})
        for diagnostic in diag_iter do
          this._channel.log(
            "ERROR: " + diagnostic.message +
            " in file " + diagnostic.file_uri())
          match \exhaustive\ diagnostic.file_uri()
          | "" =>
            // collect global errors
            this._global_errors.push(diagnostic)

          | let err_file_uri: String val =>
            let err_file = Uris.to_path(err_file_uri)

            // store the error in the document-state
            try
              let package: FilePath = this._find_workspace_package(err_file)?
              let package_state = this._ensure_package(package)
              let document_state = package_state.ensure_document(err_file)
              document_state.add_diagnostic(run, diagnostic)
            end

            if this._client.supports_publish_diagnostics() then
              // collect errors by file
              errors_by_file.upsert(
                err_file,
                pc.Vec[JsonValue].push(diagnostic.to_json()),
                {(current: pc.Vec[JsonValue], provided: pc.Vec[JsonValue]) =>
                  current.concat(provided.values())
                }
              )
            end
          end
        end
      end

      if done_compiling then
        // notify client about errors, if any
        // create (or clear) error diagnostics
        // message for each open file
        if this._client.supports_publish_diagnostics() then
          for file in errors_by_file.keys() do
            try
              let file_errors: pc.Vec[JsonValue] = errors_by_file(file)?
              let msg =
                Notification.create(
                  Methods.text_document().publish_diagnostics(),
                  JsonObject
                    .update("uri", Uris.from_path(file))
                    .update("diagnostics", JsonArray(consume file_errors)))
              this._channel.send(msg)
            end
          end

          // publish global diagnostics if any
          let num_global_errs = this._global_errors.size()
          if (num_global_errs > 0) then
            let json_global_diagnostics =
              pc.Vec[JsonValue].concat(
                Iter[Diagnostic](
                  this._global_errors.values())
                    .map[JsonValue]({(diagnostic) => diagnostic.to_json() }))
            let global_notifications =
              Notification.create(
                Methods.text_document().publish_diagnostics(),
                JsonObject
                  .update("uri", "global")
                  .update("diagnostics", JsonArray(json_global_diagnostics)))
            this._channel.send(global_notifications)
          end
        end
      end
    else
      this._channel.log(
        "Received compilation result for run " + run.string() +
        " but already received results for run " + this._compile_run.string())
    end

  fun ref _compile(package: FilePath) =>
    """
    Central entry point for triggering a compilation.

    Does all the notifying and bookkeeping before
    actually triggering the compilation.
    """
    this._channel.log(
      "Compiling package " + package.path + " with dependency-paths: " +
      ", ".join(workspace.dependency_paths.values()))

    let token = this._compilation_token(package)
    if this._client.supports_window_work_done_progress() then
      let chan = this._channel
      let progress_begin_notification =
        Notification(
          Methods.progress(),
          JsonObject
            .update("token", token)
            .update(
              "value",
              JsonObject
                .update("kind", "begin")
                .update(
                  "title",
                  "Compiling " + Path.base(package.path) +
                  " (" + this._compile_run.string() + ")")
                .update("message", "Pass: Parse")
                .update("percentage", I64(5))
                .update("cancellable", false)
            )
        )
      this._request_sender.send_request(
        Methods.window().work_done_progress().create(),
        JsonObject.update("token", token),
        object is ResponseNotify
          let _chan: Channel = chan
          be notify(method: String val, r: ResponseMessage val) =>
            if r.is_result() then
              _chan.send(progress_begin_notification)
            end
        end
      )
    end
    // get results after parse pass and after finaliser pass
    _compiler.compile(package, workspace.dependency_paths, this, this._compile_passes)
    _compiling = true

  fun _compilation_token(package: FilePath): String =>
    recover val
      String.create(128)
        .> append("pony-lsp")
        .> append(package.path)
        .> append("/compile/")
        .> append(this._compile_run.string())
    end

  be did_open(document_uri: String, notification: Notification val) =>
    """
    Handling the textDocument/didOpen notification.
    """
    let document_path = Uris.to_path(document_uri)
    this._channel.log("handling did_open of " + document_path)

    let package: FilePath =
      try
        this._find_workspace_package(document_path)?
      else
        var package_path = Path.dir(document_path)
        if not Path.is_abs(package_path) then
          try
            this.workspace.folder.join(package_path)?
          else
            _channel.log("Error determining package_path for " + document_path)
            return
          end
        else
          FilePath(this._file_auth, package_path)
        end
      end
    this._channel.log("did_open in pony package @ " + package.path)
    let package_state = this._ensure_package(package)
    let doc_state = package_state.ensure_document(document_path)
    if doc_state.needs_compilation() then
      if this._compiling then
        let current_hash = doc_state.module_hash()
        this._awaiting_compilation_for.insert(document_path, current_hash)
      else
        _channel.log(
          "No module found for document " + document_path +
          ". Need to compile.")
        this._compile(package)
      end
    end

  be did_close(document_uri: String, notification: Notification val) =>
    """
    Handling the textDocument/didClose notification.
    """
    let document_path = Uris.to_path(document_uri)
    this._channel.log("handling did_close of " + document_path)
    try
      let package: FilePath = this._find_workspace_package(document_path)?
      let package_state = this._ensure_package(package)
      try
        let document_state = package_state.documents.remove(document_path)?._2
        document_state.dispose()
      end
    else
      _channel.log("document not in workspace: " + document_path)
    end

  be did_save(document_uri: String, notification: Notification val) =>
    """
    Handling the textDocument/didSave notification.
    """
    let document_path = Uris.to_path(document_uri)
    this._channel.log("handling did_save of " + document_path)
    try
      let package: FilePath = this._find_workspace_package(document_path)?
      let package_state = this._ensure_package(package)
      if this._compiling then
        // put the hash of the doc at saving time
        let text_content_hash =
          try
            (JsonPathParser.compile("$.text")?
              .query_one(notification.params) as String).hash()
          end
        this._awaiting_compilation_for.insert(document_path, text_content_hash)
      else
        this._compile(package)
      end
    else
      _channel.log("document not in workspace: " + document_path)
    end

  be hover(document_uri: String, request: RequestMessage val) =>
    """
    Handling the textDocument/hover request.
    """
    this._current_request = request

    // Parse position from request params
    (let line, let column) =
      match \exhaustive\ _parse_hover_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return // Error already sent
      end

    let document_path = Uris.to_path(document_uri)

    // Find AST node at the position
    match \exhaustive\ _find_node_and_module(document_path, line, column)
    | (let hover_node: AST box, _) =>
      this._channel.log("Found AST node: " + hover_node.debug())

      // Extract hover information and build response
      match \exhaustive\ HoverFormatter.create_hover(hover_node, this._channel)
      | let hover_text: String =>
        // Use the original hover node for highlighting,
        // not any definition it may reference
        let hover_response = _build_hover_response(hover_text, hover_node)
        this._channel.send(ResponseMessage( request.id, hover_response))
        return
      | None =>
        this._channel.log(
          "No hover info available for node type: " +
          TokenIds.string(hover_node.id()))
      end
    | None =>
      this._channel.log("No hover data found for position")
    end

    // Send null response on failure
    this._channel.send(ResponseMessage.create(request.id, None))

  fun ref _parse_hover_position(
    request: RequestMessage val
  ): ((I64, I64) | None) =>
    """
    Extract line and column from hover request params.
    Sends error response and returns None if invalid.
    """
    try
      let nav = JsonNav(request.params)("position")
      let l = nav("line").as_i64()? // 0-based
      let c = nav("character").as_i64()? // 0-based
      (l, c)
    else
      _channel.send(
        ResponseMessage.create(
          request.id,
          None,
          ResponseError(ErrorCodes.invalid_params(), "Invalid position")))
      None
    end

  fun ref _find_node_and_module(
    document_path: String,
    line: I64,
    column: I64): ((AST box, Module val) | None)
  =>
    """
    Find the AST node and its module at the given position.
    Returns None if the document or package is not found,
    or no node exists at that position.
    """
    try
      let package: FilePath = this._find_workspace_package(document_path)?

      match \exhaustive\ this._get_package(package)
      | let pkg_state: PackageState =>
        match \exhaustive\ pkg_state.get_document(document_path)
        | let doc: DocumentState =>
          match (doc.position_index(), doc.module())
          | (let index: PositionIndex, let module: Module val) =>
            match \exhaustive\ index.find_node_at(
              USize.from[I64](line + 1),
              USize.from[I64](column + 1))
            | let node: AST box => (node, module)
            | None => None
            end
          else
            this._channel.log(
              "No index or module available for " + document_path)
            None
          end
        else
          this._channel.log("No document state available for " + document_path)
          None
        end
      | None =>
        this._channel.log(
          "No package state available for package: " + package.path)
        None
      end
    else
      this._channel.log("document not in workspace: " + document_path)
      None
    end

  fun _build_hover_response(hover_text: String, ast: AST box): JsonValue =>
    """
    Build the LSP Hover response JSON from hover text and AST node.
    """
    let hover_contents = JsonObject
      .update("kind", "markdown")
      .update("value", hover_text)

    // For declarations, use identifier span;
    // for references, use the whole node
    let highlight_node = ASTIdentifier.identifier_node(ast)
    (let start_pos, let end_pos) = highlight_node.span()
    let hover_range =
      LspPositionRange(
        LspPosition.from_ast_pos(start_pos),
        LspPosition.from_ast_pos_end(end_pos))

    JsonObject
      .update("contents", hover_contents)
      .update("range", hover_range.to_json())

  be document_highlight(document_uri: String, request: RequestMessage val) =>
    """
    Handle textDocument/documentHighlight request.
    """
    this._channel.log("Handling textDocument/documentHighlight")
    (let line, let column) =
      match \exhaustive\ _parse_hover_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let document_path = Uris.to_path(document_uri)
    match _find_node_and_module(document_path, line, column)
    | (let node: AST box, let module: Module val) =>
      let highlights = DocumentHighlights.collect(node, module)
      var json_arr = JsonArray
      for (range, kind) in highlights.values() do
        json_arr =
          json_arr.push(
            JsonObject
              .update("range", range.to_json())
              .update("kind", kind))
      end
      this._channel.send(ResponseMessage(request.id, json_arr))
      return
    end
    this._channel.send(ResponseMessage.create(request.id, None))

  be references(document_uri: String, request: RequestMessage val) =>
    """
    Handle textDocument/references request.
    """
    this._channel.log("Handling textDocument/references")
    (let line, let column) =
      match \exhaustive\ _parse_hover_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let include_declaration: Bool =
      try
        JsonNav(request.params)("context")("includeDeclaration").as_bool()?
      else
        true
      end
    let document_path = Uris.to_path(document_uri)
    match _find_node_and_module(document_path, line, column)
    | (let node: AST box, _) =>
      let locations =
        References.collect(node, this._packages, include_declaration)
      var json_arr = JsonArray
      for loc in locations.values() do
        json_arr = json_arr.push(loc.to_json())
      end
      this._channel.send(ResponseMessage(request.id, json_arr))
      return
    end
    this._channel.send(ResponseMessage.create(request.id, None))

  be goto_definition(document_uri: String, request: RequestMessage val) =>
    """
    Handling the textDocument/definition request.
    """
    this._channel.log("handling textDocument/definition")
    this._current_request = request
    (let line, let column) =
      match \exhaustive\ _parse_hover_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let document_path = Uris.to_path(document_uri)
    match \exhaustive\ _find_node_and_module(document_path, line, column)
    | (let ast: AST box, _) =>
      this._channel.log(ast.debug())
      var json_arr = JsonArray
      for ast_definition in ast.definitions().values() do
        (let start_pos, let end_pos) = ast_definition.span()
        try
          json_arr =
            json_arr.push(
              LspLocation(
                Uris.from_path(ast_definition.source_file() as String val),
                LspPositionRange(
                  LspPosition.from_ast_pos(start_pos),
                  LspPosition.from_ast_pos_end(end_pos))
              ).to_json()
            )
        else
          this._channel.log(
            "No source file found for definition: " + ast_definition.debug())
        end
      end
      this._channel.send(ResponseMessage(request.id, json_arr))
      return
    | None => None
    end
    // send a null-response in every failure case
    this._channel.send(ResponseMessage.create(request.id, None))

  be document_symbols(document_uri: String, request: RequestMessage val) =>
    """
    Handle textDocument/documentSymbol request.
    """
    this._channel.log("Handling textDocument/documentSymbol")
    let document_path = Uris.to_path(document_uri)
    try
      let package: FilePath = this._find_workspace_package(document_path)?
      match \exhaustive\ this._get_package(package)
      | let pkg_state: PackageState =>
        // this._channel.log(pkg_state.debug())
        match \exhaustive\ pkg_state.get_document(document_path)
        | let doc: DocumentState =>
          let symbols = doc.document_symbols()
          var json_arr = JsonArray
          for symbol in symbols.values() do
            json_arr = json_arr.push(symbol.to_json())
          end
          this._channel.send(ResponseMessage(request.id, json_arr))
          return
        | None =>
          this._channel.log("No document state available for " + document_path)
        end
      | None =>
        this._channel.log(
          "No package state available for package: " + package.path)
      end
    else
      this._channel.log("document not in workspace: " + document_path)
    end
    // send a null-response in every failure case
    this._channel.send(ResponseMessage.create(request.id, None))

  be document_diagnostic(document_uri: String, request: RequestMessage val) =>
    """
    Handle textDocument/diagnostic request.
    """
    this._channel.log("Handling textDocument/diagnostic")
    let document_path = Uris.to_path(document_uri)
    var diagnostics = pc.Vec[JsonValue]
    try
      let package: FilePath = this._find_workspace_package(document_path)?
      match \exhaustive\ this._get_package(package)
      | let pkg_state: PackageState =>
        match \exhaustive\ pkg_state.get_document(document_path)
        | let doc: DocumentState =>
          try
            diagnostics =
              diagnostics
                .concat(
                  Iter[Diagnostic]((doc.diagnostics() as Array[Diagnostic] box)
                    .values())
                .map[JsonValue]({(diagnostic) => diagnostic.to_json() }))
          end
        | None =>
          this._channel.log(
            "[textDocument/diagnostic] No document state available for "
            + document_path)
        end
      | None =>
        this._channel.log(
          "[textDocument/diagnostic] No package state available for package: "
          + package.path)
      end
    else
      this._channel.log(
        "[textDocument/diagnostic] Unable to find workspace package for: "
        + document_uri)
    end
    this._channel.send(
      ResponseMessage.create(
        request.id,
        JsonObject
          .update("kind", "full")
          .update("resultId", this._compile_run.string())
          .update("items", JsonArray(diagnostics))
      )
    )

  be inlay_hint(document_uri: String, request: RequestMessage val) =>
    """
    Handle textDocument/inlayHint request.
    """
    this._channel.log("Handling textDocument/inlayHint")
    let document_path = Uris.to_path(document_uri)
    try
      let package: FilePath = this._find_workspace_package(document_path)?
      match \exhaustive\ this._get_package(package)
      | let pkg_state: PackageState =>
        match \exhaustive\ pkg_state.get_document(document_path)
        | let doc: DocumentState =>
          match \exhaustive\ doc.module()
          | let module: Module val =>
            let range: (None | (I64, I64, I64, I64)) =
              try
                let p = request.params
                let sl = JsonNav(p)("range")("start")("line").as_i64()?
                let sc = JsonNav(p)("range")("start")("character").as_i64()?
                let el = JsonNav(p)("range")("end")("line").as_i64()?
                let ec = JsonNav(p)("range")("end")("character").as_i64()?
                (sl, sc, el, ec)
              else
                None
              end
            let hints = InlayHints.collect(module, range)
            var json_arr = JsonArray
            for hint in hints.values() do
              json_arr = json_arr.push(hint)
            end
            this._channel.send(ResponseMessage(request.id, json_arr))
            return
          | None =>
            this._channel.log(
              "No module available for " + document_path)
          end
        | None =>
          this._channel.log(
            "No document state available for " + document_path)
        end
      | None =>
        this._channel.log(
          "No package state available for " + document_path)
      end
    else
      this._channel.log("document not in workspace: " + document_path)
    end
    this._channel.send(ResponseMessage.create(request.id, None))

  be dispose() =>
    for package_state in this._packages.values() do
      package_state.dispose()
    end
