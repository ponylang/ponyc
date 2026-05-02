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
  let _compiler: LspCompiler
  let _packages: Map[String, PackageState]
  let _global_errors: Array[Diagnostic val]
  var _compile_run: USize = 0
  var _compiling: Bool = false
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

  be done_compiling(
    program_dir: FilePath,
    result: (Program val | Array[Error val] val),
    run: USize)
  =>
    """
    Handle compilation completion.
    """
    if this._client.supports_window_work_done_progress()
    then
      let message =
        match \exhaustive\ result
        | let p: Program val => "Success"
        | let errors: Array[Error val] val => errors.size().string() + " Errors"
        end
      this._channel.send(
        Notification(
          Methods.progress(),
          JsonObject
            .update(
              "token",
              this._compilation_token(program_dir))
            .update(
              "value",
              JsonObject
                .update("kind", "end")
                .update("message", message)
            )
        )
      )
    end
    this._compiling = false
    // TODO: check if we are awaiting compilation
    // for some files and compare their hash against
    // the current document hashes
    this._channel.log(
      "done compilation run " + run.string() +
      " of " + program_dir.path)

    // ignore older compile runs
    if run > this._compile_run then
      this._compile_run = run
      // group errors by file
      let errors_by_file = Map[String, pc.Vec[JsonValue]].create(4)

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

      match \exhaustive\ result
      | let program: Program val =>
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
      | let errors: Array[Error val] val =>
        this._channel.log(
          "Compilation failed with " +
          errors.size().string() + " errors")

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
      elseif this._client.supports_workspace_diagnostic_refresh() then
        // we only need to issue a refresh if
        // the client doesn't support publish
        // diagnostics

        // tell the client to refresh all
        // diagnostics for us
        this._request_sender.send_request(
          Methods.workspace().diagnostic().refresh(),
          None)
      end
      if this._client.supports_inlay_hint_refresh() then
        // tell the client to re-request inlay hints for all open documents
        this._request_sender.send_request(
          Methods.workspace().inlay_hint().refresh(), None)
      end
      if this._client.supports_folding_range_refresh() then
        // tell the client to re-request folding ranges for all open documents
        this._request_sender.send_request(
          Methods.workspace().folding_range().refresh(), None)
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
    if this._client.supports_window_work_done_progress()
    then
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
    _compiler.compile(package, workspace.dependency_paths, this)
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
    match _get_index_and_module(document_path)
    | (let index: PositionIndex val, let module: Module val) =>
      match index.find_node_at(
        USize.from[I64](line + 1),
        USize.from[I64](column + 1))
      | let node: AST box => (node, module)
      end
    end

  fun ref _find_method_node(
    document_path: String,
    sel_line: I64,
    sel_col: I64): (AST box | None)
  =>
    """
    Resolve the method AST node at the given position. Returns None when the
    document has not been compiled or no method node is found at that position.
    """
    if (sel_line < 0) or (sel_col < 0) then
      return None
    end
    match _find_node_and_module(document_path, sel_line, sel_col)
    | (let node: AST box, _) =>
      CallHierarchy._method_for_node(node)
    end

  fun ref _get_index_and_module(
    document_path: String): ((PositionIndex val, Module val) | None)
  =>
    """
    Returns the position index and compiled module for the given document
    path, or None if the document is not in any workspace, has not yet
    been compiled, or its index is not yet available.
    """
    try
      let package: FilePath = this._find_workspace_package(document_path)?
      match \exhaustive\ this._get_package(package)
      | let pkg_state: PackageState =>
        match \exhaustive\ pkg_state.get_document(document_path)
        | let doc: DocumentState =>
          match (doc.position_index(), doc.module())
          | (let index: PositionIndex val, let module: Module val) =>
            (index, module)
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

  be prepare_rename(document_uri: String, request: RequestMessage val) =>
    """
    Handle textDocument/prepareRename request.

    Validates that the symbol at the given position can be renamed and returns
    its identifier range. The client uses this range to pre-select text in the
    rename dialog. Returns an error if the symbol is not renameable (literal,
    synthetic node, or defined outside the workspace).
    """
    this._channel.log("Handling textDocument/prepareRename")
    (let line, let column) =
      match \exhaustive\ _parse_hover_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let document_path = Uris.to_path(document_uri)
    match _find_node_and_module(document_path, line, column)
    | (let node: AST box, _) =>
      let target: AST val =
        match \exhaustive\ _ResolveASTTarget(node)
        | let t: AST val => t
        | None =>
          this._channel.send(
            ResponseMessage.create(
              request.id,
              None,
              ResponseError(
                ErrorCodes.request_failed(), "Symbol is not renameable")))
          return
        end
      let target_file: String val =
        try
          target.source_file() as String val
        else
          this._channel.send(
            ResponseMessage.create(
              request.id,
              None,
              ResponseError(
                ErrorCodes.request_failed(), "Symbol has no source location")))
          return
        end
      if not target_file.at(workspace.folder.path + Path.sep(), 0) then
        this._channel.send(
          ResponseMessage.create(
            request.id,
            None,
            ResponseError(
              ErrorCodes.request_failed(),
              "Symbol is defined outside the workspace")))
        return
      end
      let ident_node = ASTIdentifier.identifier_node(node)
      (let start_pos, let end_pos) = ident_node.span()
      let range =
        LspPositionRange(
          LspPosition.from_ast_pos(start_pos),
          LspPosition.from_ast_pos_end(end_pos))
      this._channel.send(ResponseMessage(request.id, range.to_json()))
      return
    end
    this._channel.send(ResponseMessage.create(request.id, None))

  be rename(document_uri: String, request: RequestMessage val) =>
    """
    Handle textDocument/rename request.
    """
    this._channel.log("Handling textDocument/rename")
    (let line, let column) =
      match \exhaustive\ _parse_hover_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let new_name: String val =
      try
        JsonNav(request.params)("newName").as_string()?
      else
        this._channel.send(
          ResponseMessage.create(
            request.id,
            None,
            ResponseError(
              ErrorCodes.invalid_params(),
              "Missing or invalid newName")))
        return
      end
    if not _IsValidPonyIdentifier(new_name) then
      this._channel.send(
        ResponseMessage.create(
          request.id,
          None,
          ResponseError(
            ErrorCodes.invalid_params(),
            "newName is not a valid Pony identifier")))
      return
    end
    let document_path = Uris.to_path(document_uri)
    match _find_node_and_module(document_path, line, column)
    | (let node: AST box, _) =>
      match \exhaustive\ Rename.collect(
        node,
        this._packages,
        workspace.folder.path,
        new_name)
      | let workspace_edit: JsonObject val =>
        this._channel.send(ResponseMessage(request.id, workspace_edit))
      | let err_msg: String val =>
        this._channel.send(
          ResponseMessage.create(
            request.id,
            None,
            ResponseError(ErrorCodes.request_failed(), err_msg)))
      end
      return
    end
    this._channel.send(ResponseMessage.create(request.id, None))

  be goto_definition(document_uri: String, request: RequestMessage val) =>
    """
    Handling the textDocument/definition request.
    """
    this._channel.log("handling " + request.method)
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
        match \exhaustive\ _node_location(ast_definition)
        | let loc: LspLocation val =>
          json_arr = json_arr.push(loc.to_json())
        | None =>
          this._channel.log(
            "No source file found for definition: " +
              ast_definition.debug())
        end
      end
      this._channel.send(ResponseMessage(request.id, json_arr))
      return
    | None => None
    end
    // send a null-response in every failure case
    this._channel.send(ResponseMessage.create(request.id, None))

  be type_definition(document_uri: String, request: RequestMessage val) =>
    """
    Handle textDocument/typeDefinition request.

    Finds the definition of the type of the symbol at the cursor position.
    E.g. for a local variable `x: MyClass`, this navigates to `MyClass`.
    """
    this._channel.log("handling textDocument/typeDefinition")
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
      match ast.ast_type()
      | let type_ast: AST =>
        for type_def in type_ast.definitions().values() do
          match \exhaustive\ _node_location(type_def)
          | let loc: LspLocation val =>
            json_arr = json_arr.push(loc.to_json())
          | None =>
            this._channel.log(
              "No source file found for type definition: " +
                type_def.debug())
          end
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

  be workspace_symbol(
    query: String val,
    aggregator: WorkspaceSymbolAggregator)
  =>
    """
    Handle workspace/symbol request.
    Collects symbols matching the query across all packages
    in this workspace.
    """
    this._channel.log("Handling workspace/symbol: '" + query + "'")
    let query_lower: String val = query.lower()
    var results = JsonArray
    for pkg_state in this._packages.values() do
      for doc_state in pkg_state.documents.values() do
        let file_uri = Uris.from_path(doc_state.path)
        let top_symbols = doc_state.document_symbols()
        for symbol in top_symbols.values() do
          if _symbol_matches(symbol.name, query_lower) then
            results =
              results.push(
                JsonObject
                  .update("name", symbol.name)
                  .update("kind", symbol.kind)
                  .update(
                    "location",
                    JsonObject
                      .update("uri", file_uri)
                      .update(
                        "range",
                        symbol.range.to_json())))
          end
          for child in symbol.children.values() do
            if _symbol_matches(child.name, query_lower) then
              results =
                results.push(
                  JsonObject
                    .update("name", child.name)
                    .update("kind", child.kind)
                    .update("containerName", symbol.name)
                    .update(
                      "location",
                      JsonObject
                        .update("uri", file_uri)
                        .update(
                          "range",
                          child.range.to_json())))
            end
          end
        end
      end
    end
    aggregator.add_results(results)

  fun _node_location(node: AST box): (LspLocation val | None) =>
    """
    Builds an `LspLocation` for `node`.

    Returns `None` if the node has no source file (e.g. synthesised nodes)
    or if the computed source span is inverted.
    """
    try
      let doc_path = node.source_file() as String val
      let range =
        match \exhaustive\ ASTClampedRange(node, doc_path, SiblingBound(node))
        | let r: LspPositionRange => r
        | None => error
        end
      LspLocation(Uris.from_path(doc_path), range)
    else
      None
    end

  fun _symbol_matches(name: String, query_lower: String): Bool =>
    """
    Returns true if `name` matches `query_lower` (already lowercased).
    An empty query matches everything; otherwise a case-insensitive substring
    match is performed.
    """
    if query_lower.size() == 0 then
      return true
    end
    try
      name.lower().find(query_lower)?
      true
    else
      false
    end

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

  be folding_range(document_uri: String, request: RequestMessage val) =>
    """
    Handle textDocument/foldingRange request.
    """
    this._channel.log("Handling textDocument/foldingRange")
    let document_path = Uris.to_path(document_uri)
    try
      let package: FilePath = this._find_workspace_package(document_path)?
      match \exhaustive\ this._get_package(package)
      | let pkg_state: PackageState =>
        match \exhaustive\ pkg_state.get_document(document_path)
        | let doc: DocumentState =>
          match \exhaustive\ doc.module()
          | let module: Module val =>
            let fold_ranges = FoldingRanges.collect(module)
            var json_arr = JsonArray
            for range in fold_ranges.values() do
              json_arr = json_arr.push(range)
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

  be selection_range(document_uri: String, request: RequestMessage val) =>
    """
    Handle textDocument/selectionRange request.

    Parses the `positions` array from the request params. For each position,
    finds the AST node under the cursor and builds a SelectionRange linked list
    from innermost to outermost. The response is a JsonArray parallel to the
    request positions array; entries are null when no node is found at a
    position. Returns null if the request has no valid `positions` array.
    """
    this._channel.log("Handling textDocument/selectionRange")
    let document_path = Uris.to_path(document_uri)

    // Each position maps to one response entry (null when no node found), so
    // the output array is always parallel to the input positions array.
    var out = JsonArray
    try
      let arr = JsonNav(request.params)("positions").as_array()?
      for pos_val in arr.values() do
        let entry: JsonValue =
          try
            let l = JsonNav(pos_val)("line").as_i64()?
            let c = JsonNav(pos_val)("character").as_i64()?
            match _find_node_and_module(document_path, l, c)
            | (let node: AST box, _) =>
              SelectionRanges.collect(node, document_path)
            else
              None
            end
          else
            None // malformed position entry — keep array parallel
          end
        out = out.push(entry)
      end
    else
      this._channel.send(
        ResponseMessage.create(
          request.id,
          None,
          ResponseError(
            ErrorCodes.invalid_params(),
            "Missing or invalid 'positions' array in selectionRange request")))
      return
    end
    this._channel.send(ResponseMessage(request.id, out))

  be signature_help(document_uri: String, request: RequestMessage val) =>
    """
    Handle textDocument/signatureHelp request.
    """
    this._channel.log("Handling textDocument/signatureHelp")
    (let line, let column) =
      match \exhaustive\ _parse_hover_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let document_path = Uris.to_path(document_uri)
    // Convert LSP 0-based cursor to 1-based AST coordinates.
    let cursor_line = USize.from[I64](line + 1)
    let cursor_col  = USize.from[I64](column + 1)
    match _get_index_and_module(document_path)
    | (let index: PositionIndex val, let module: Module val) =>
      // Scan the source text forward from the cursor, skipping whitespace
      // and commas, to find the next real token.  This handles the case
      // where the cursor is on a separator character or trailing whitespace
      // where no AST node exists — without magic column limits.
      (let tok_line, let tok_col) =
        match \exhaustive\ module.ast.source_contents()
        | let src: String box =>
          SignatureHelpSource.scan_to_token(src, cursor_line, cursor_col)
        | None =>
          (cursor_line, cursor_col)
        end
      match index.find_node_at(tok_line, tok_col)
      | let node: AST box =>
        match \exhaustive\ SignatureHelp.collect(
          node, tok_line, tok_col, this._channel)
        | let result: JsonObject =>
          this._channel.send(ResponseMessage(request.id, result))
          return
        | None => None
        end
      end
    end
    this._channel.send(ResponseMessage.create(request.id, None))

  be call_hierarchy_prepare(
    document_uri: String,
    request: RequestMessage val)
  =>
    """
    Handle textDocument/prepareCallHierarchy request.
    """
    this._channel.log("Handling textDocument/prepareCallHierarchy")
    (let line, let column) =
      match \exhaustive\ _parse_hover_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let document_path = Uris.to_path(document_uri)
    match \exhaustive\ _find_node_and_module(document_path, line, column)
    | (let node: AST box, _) =>
      match CallHierarchy.prepare(node)
      | let result: JsonArray =>
        this._channel.send(ResponseMessage(request.id, result))
        return
      end
    | None => None
    end
    this._channel.send(ResponseMessage.create(request.id, None))

  be call_hierarchy_incoming_calls(
    document_uri: String,
    sel_line: I64,
    sel_col: I64,
    request: RequestMessage val)
  =>
    """
    Handle callHierarchy/incomingCalls request.
    """
    this._channel.log("Handling callHierarchy/incomingCalls")
    let document_path = Uris.to_path(document_uri)
    match _find_method_node(document_path, sel_line, sel_col)
    | let node: AST box =>
      match CallHierarchy.incoming_calls(node, this._packages)
      | let result: JsonArray =>
        this._channel.send(ResponseMessage(request.id, result))
        return
      end
    end
    this._channel.send(ResponseMessage.create(request.id, None))

  be call_hierarchy_outgoing_calls(
    document_uri: String,
    sel_line: I64,
    sel_col: I64,
    request: RequestMessage val)
  =>
    """
    Handle callHierarchy/outgoingCalls request.
    """
    this._channel.log("Handling callHierarchy/outgoingCalls")
    let document_path = Uris.to_path(document_uri)
    match _find_method_node(document_path, sel_line, sel_col)
    | let node: AST box =>
      match CallHierarchy.outgoing_calls(node, this._packages)
      | let result: JsonArray =>
        this._channel.send(ResponseMessage(request.id, result))
        return
      end
    end
    this._channel.send(ResponseMessage.create(request.id, None))

  be type_hierarchy_prepare(
    document_uri: String,
    request: RequestMessage val)
  =>
    """
    Handle textDocument/prepareTypeHierarchy request.
    """
    this._channel.log("Handling textDocument/prepareTypeHierarchy")
    (let line, let column) =
      match \exhaustive\ _parse_hover_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let document_path = Uris.to_path(document_uri)
    match \exhaustive\ _find_node_and_module(document_path, line, column)
    | (let node: AST box, _) =>
      match TypeHierarchy.prepare(node)
      | let result: JsonArray =>
        this._channel.send(ResponseMessage(request.id, result))
        return
      end
    | None => None
    end
    this._channel.send(ResponseMessage.create(request.id, None))

  be type_hierarchy_supertypes(
    document_uri: String,
    sel_line: I64,
    sel_col: I64,
    request: RequestMessage val)
  =>
    """
    Handle typeHierarchy/supertypes request.
    """
    this._channel.log("Handling typeHierarchy/supertypes")
    let document_path = Uris.to_path(document_uri)
    match \exhaustive\ _find_node_and_module(document_path, sel_line, sel_col)
    | (let node: AST box, _) =>
      match TypeHierarchy.supertypes(node)
      | let result: JsonArray =>
        this._channel.send(ResponseMessage(request.id, result))
        return
      end
    | None => None
    end
    this._channel.send(ResponseMessage.create(request.id, None))

  be type_hierarchy_subtypes(
    document_uri: String,
    sel_line: I64,
    sel_col: I64,
    request: RequestMessage val)
  =>
    """
    Handle typeHierarchy/subtypes request.
    """
    this._channel.log("Handling typeHierarchy/subtypes")
    let document_path = Uris.to_path(document_uri)
    match \exhaustive\ _find_node_and_module(document_path, sel_line, sel_col)
    | (let node: AST box, _) =>
      match TypeHierarchy.subtypes(node, this._packages)
      | let result: JsonArray =>
        this._channel.send(ResponseMessage(request.id, result))
        return
      end
    | None => None
    end
    this._channel.send(ResponseMessage.create(request.id, None))

  be dispose() =>
    for package_state in this._packages.values() do
      package_state.dispose()
    end
