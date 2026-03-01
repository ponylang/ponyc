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

  A workspace is initially created by scanning its directory and extracting information from available
  `corral.json` files.

  More information is obtained during execution of the type-checking pipeline of the libponyc compiler.
  While the initial information must be enough to successfully type-check the program (populating the `$PONYPATH` env var),
  the AST we get back from type-checking contains all packages the program in this workspace consists of.

  After first successful compilation we should have all information at hand to do LSP work.
  i.e. all packages we can move to from opened files in the workspace should be available.
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
  var _current_request: (RequestMessage val | None) = None
  var _compiling: Bool = false
  var _awaiting_compilation_for: Map[String, (USize| None)] = _awaiting_compilation_for.create()
    """
    mapping of file_path to its hash (if any) that got saved while a compilation was still running
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
    _global_errors = Array[Diagnostic val].create(4)

  fun ref _ensure_package(package_path: FilePath): PackageState =>
    try
      this._packages(package_path.path)?
    else
      this._packages.insert(package_path.path, PackageState.create(package_path, this._channel))
    end

  fun _get_package(package_path: FilePath): (this->PackageState | None) =>
    try
      this._packages(package_path.path)?
    else
      None
    end

  fun box _find_workspace_package(document_path: String): FilePath ? =>
    """
    Try to find the package in the workspace from the given `document_path`
    by iterating up the directory tree to see if any dir belongs to
    one of the packages of the workspace.

    Errors if no package could be found.
    """
    var dir_path = Path.dir(document_path)
    while (dir_path != ".") do
      if this._packages.contains(dir_path) then
        return
          if Path.is_abs(dir_path) then
            // TODO: ensure files are on the PONYPATH?
            FilePath(this._file_auth, dir_path)
          else
            this.workspace.folder.join(dir_path)?
          end
      end
      dir_path = Path.dir(dir_path)
    end
    error

  be done_compiling(program_dir: FilePath, result: (Program val | Array[Error val] val), run: USize) =>
    this._compiling = false
    // TODO: check if we are awaiting compilation for some files
    //       and compare their hash against the current document hashes
    this._channel.log("done compilation run " + run.string() + " of " + program_dir.path)

    // ignore older compile runs
    if run > this._compile_run then
      this._compile_run = run
      // group errors by file
      let errors_by_file = Map[String, pc.Vec[JsonValue]].create(4)

      if this._client.supports_publish_diagnostics() then
        // pre-fill with errors by file for all currently opened files
        // if we have no errors for them, they will get their errors cleared
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
        // create/update package states for every package being part of the program
        for package in program.packages() do
          let package_path = FilePath(this._file_auth, package.path)
          let package_state = this._ensure_package(package_path)
          package_state.update(package, run)

          // TODO: for each entry in _awaiting_compilation_for: try to find it
          // in this package and get the new module and compare its hash
          var requires_another_compilation = false
          for (await_comp_file, await_comp_hash) in this._awaiting_compilation_for.pairs() do
            let await_pkg_path = Path.base(await_comp_file)
            if package_state.path.path == await_pkg_path then
              try
                // get the hash of the module file ponyc considered for compilation
                let new_mod_hash =
                  (package_state.get_document(await_comp_file) as DocumentState).module_hash()
                this._awaiting_compilation_for.remove(await_comp_file)?
                requires_another_compilation =
                  requires_another_compilation or
                    match \exhaustive\ (await_comp_hash, new_mod_hash)
                    |  (None, None) =>
                        // no change, module not there???
                        false
                    | (None, let h: USize) | (let h: USize, None) =>
                        // module didn't have a hash (probably errored), but now is available, we need to recompile to check
                        // or module was valid when awaiting compilation, but isn't anymore
                        true
                    | (let old_hash: USize, let new_hash: USize) if old_hash != new_hash =>
                        // contents differ, we need another round of compilation
                        old_hash != new_hash
                    else
                      false
                    end
              end
            end
          end
          if requires_another_compilation then
            this._channel.log(program_dir.path + " needs another compilation...")
            this._compiler.compile(program_dir, workspace.dependency_paths, this)
            this._compiling = true
          end
        end
      | let errors: Array[Error val] val =>
        this._channel.log("Compilation failed with " + errors.size().string() + " errors")

        let diag_iter = Iter[Error](errors.values())
          .map[Diagnostic]({(err): Diagnostic => Diagnostic.from_error(err)})
          .flat_map[Diagnostic]({(diag): Iterator[Diagnostic] => diag.expand_related().values()})
        for diagnostic in diag_iter do
          this._channel.log("ERROR: " + diagnostic.message + " in file " + diagnostic.file_uri())
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
      // create (or clear) error diagnostics message for each open file
      if this._client.supports_publish_diagnostics() then
        for file in errors_by_file.keys() do
          try
            let file_errors: pc.Vec[JsonValue] = errors_by_file(file)?
            let msg = Notification.create(
              Methods.text_document().publish_diagnostics(),
              JsonObject
                .update("uri", Uris.from_path(file))
                .update("diagnostics", JsonArray(consume file_errors))
            )
            this._channel.send(msg)
          end
        end

        // publish global diagnostics if we have any
        let num_global_errs = this._global_errors.size()
        if (num_global_errs > 0) then
          let json_global_diagnostics = pc.Vec[JsonValue].concat(Iter[Diagnostic](this._global_errors.values()).map[JsonValue]({(diagnostic) => diagnostic.to_json()}))
          let global_notifications = Notification.create(
            Methods.text_document().publish_diagnostics(),
            JsonObject
              // TODO: any better value to put here?
              .update("uri", "global")
              .update("diagnostics", JsonArray(json_global_diagnostics))
          )
          this._channel.send(global_notifications)
        end
      elseif this._client.supports_workspace_diagnostic_refresh() then
        // we only need to issue a refresh if the client doesn't support
        // publish diagnostics

        // tell the client to refresh all diagnostics for us
        // to make sure we don't see any old diagnostics anymore
        this._request_sender.send_request(Methods.workspace().diagnostic().refresh(), None)
      end
    else
      this._channel.log("Received compilation result for run " + run.string() +
      " but already received results for run " + this._compile_run.string())
    end


  be did_open(document_uri: String, notification: Notification val) =>
    """
    Handling the textDocument/didOpen notification
    """
    let document_path = Uris.to_path(document_uri)
    this._channel.log("handling did_open of " + document_path)

    // check if we have a package for this document
    // if so, get the package FilePath
    // if we have no package:
    //
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
        _channel.log("No module found for document " + document_path + ". Need to compile.")
        _compiler.compile(package, workspace.dependency_paths, this)
        _compiling = true
      end
    end

  be did_close(document_uri: String, notification: Notification val) =>
    """
    Handling the textDocument/didClose notification
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
    Handling the textDocument/didSave notification
    """
    let document_path = Uris.to_path(document_uri)
    this._channel.log("handling did_save of " + document_path)
    // TODO: don't compile multiple times for multiple documents being saved one
    // after the other
    try
      let package: FilePath = this._find_workspace_package(document_path)?
      let package_state = this._ensure_package(package)
      if this._compiling then
        // put the hash of the doc at the time of saving
        let text_content_hash = try (JsonPathParser.compile("$.text")?.query_one(notification.params) as String).hash() end
        this._awaiting_compilation_for.insert(document_path, text_content_hash)
      else
        this._channel.log("Compiling package " + package.path + " with dependency-paths: " + ", ".join(workspace.dependency_paths.values()))
        this._compiler.compile(package, workspace.dependency_paths, this)
        this._compiling = true
      end
    else
      _channel.log("document not in workspace: " + document_path)
    end

  be hover(document_uri: String, request: RequestMessage val) =>
    """
    Handling the textDocument/hover request
    """
    this._current_request = request

    // Parse position from request params
    (let line, let column) = match \exhaustive\ _parse_hover_position(request)
    | (let l: I64, let c: I64) => (l, c)
    | None => return // Error already sent
    end

    let document_path = Uris.to_path(document_uri)

    // Find AST node at the position
    match \exhaustive\ _find_hover_node(document_path, line, column)
    | let hover_node: AST box =>
      this._channel.log("Found AST node: " + hover_node.debug())

      // Extract hover information and build response
      match \exhaustive\ HoverFormatter.create_hover(hover_node, this._channel)
      | let hover_text: String =>
        // Use the original hover node for highlighting, not any definition it may reference
        let hover_response = _build_hover_response(hover_text, hover_node)
        this._channel.send(ResponseMessage(request.id, hover_response))
        return
      | None =>
        this._channel.log("No hover info available for node type: " + TokenIds.string(hover_node.id()))
      end
    | None =>
      this._channel.log("No hover data found for position")
    end

    // Send null response on failure
    this._channel.send(ResponseMessage.create(request.id, None))

  fun ref _parse_hover_position(request: RequestMessage val): ((I64, I64) | None) =>
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
          ResponseError(ErrorCodes.invalid_params(), "Invalid position")
        )
      )
      None
    end

  fun ref _find_hover_node(document_path: String, line: I64, column: I64): (AST box | None) =>
    """
    Find the AST node at the specified position.
    Returns None if document/package not found or position invalid.
    """
    try
      let package: FilePath = this._find_workspace_package(document_path)?

      match \exhaustive\ this._get_package(package)
      | let pkg_state: PackageState =>
        match \exhaustive\ pkg_state.get_document(document_path)
        | let doc: DocumentState =>
          match doc.position_index()
          | let index: PositionIndex =>
            // Find AST node at cursor position (convert 0-based to 1-based)
            index.find_node_at(USize.from[I64](line + 1), USize.from[I64](column + 1))
          else
            this._channel.log("No position index available for " + document_path)
            None
          end
        else
          this._channel.log("No document state available for " + document_path)
          None
        end
      | None =>
        this._channel.log("No package state available for package: " + package.path)
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

    // For declarations, use identifier span; for references, use the whole node
    let highlight_node = _get_node_for_highlight(ast)
    (let start_pos, let end_pos) = highlight_node.span()
    let hover_range = LspPositionRange(
      LspPosition.from_ast_pos(start_pos),
      LspPosition.from_ast_pos_end(end_pos)
    )

    JsonObject
      .update("contents", hover_contents)
      .update("range", hover_range.to_json())

  be goto_definition(document_uri: String, request: RequestMessage val) =>
    """
    Handling the textDocument/definition request
    """
    this._channel.log("handling goto_definition")
    this._current_request = request
    // extract the source code position
    (let line, let column) =
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
            ResponseError(ErrorCodes.invalid_params(), "Invalid position")
          )
        )
        return
      end
    let document_path = Uris.to_path(document_uri)
    try
      let package: FilePath = this._find_workspace_package(document_path)?

      match \exhaustive\ this._get_package(package)
      | let pkg_state: PackageState =>
          //this._channel.log(pkg_state.debug())
          match \exhaustive\ pkg_state.get_document(document_path)
          | let doc: DocumentState =>
            match \exhaustive\ doc.position_index()
            | let index: PositionIndex =>
              match index.find_node_at(USize.from[I64](line + 1), USize.from[I64](column + 1)) // pony lines and characters are 1-based, lsp are 0-based
              | let ast: AST box =>
                this._channel.log(ast.debug())
                var json_arr = JsonArray
                // iterate through all found definitions
                for ast_definition in ast.definitions().values() do
                  // get position of the found definition
                  (let start_pos, let end_pos) = ast_definition.span()
                  try
                    // append new location
                    json_arr = json_arr.push(
                      LspLocation(
                        Uris.from_path(ast_definition.source_file() as String val),
                        LspPositionRange(
                          LspPosition.from_ast_pos(start_pos),
                          LspPosition.from_ast_pos(end_pos)
                        )
                      ).to_json()
                    )
                  else
                    this._channel.log("No source file found for definition: " + ast_definition.debug())
                  end
                end
                this._channel.send(ResponseMessage(request.id, json_arr))
                return // exit, otherwise we send a null resul
              | None =>
                this._channel.log("No AST node found @ " + line.string() + ":" + column.string())
              end
            else
              this._channel.log("No position index available for " + document_path)
            end
          else
            this._channel.log("No document state available for " + document_path)
          end
      | None =>
        this._channel.log("No package state available for package: " + package.path)
      end
    else
      this._channel.log("document not in workspace: " + document_path)
    end
    // send a null-response in every failure case
    this._channel.send(ResponseMessage.create(request.id, None))

  be document_symbols(document_uri: String, request: RequestMessage val) =>
    this._channel.log("Handling textDocument/documentSymbol")
    let document_path = Uris.to_path(document_uri)
    try
      let package: FilePath = this._find_workspace_package(document_path)?
      match \exhaustive\ this._get_package(package)
      | let pkg_state: PackageState =>
          //this._channel.log(pkg_state.debug())
          match pkg_state.get_document(document_path)
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
        this._channel.log("No package state available for package: " + package.path)
      end
    else
      this._channel.log("document not in workspace: " + document_path)
    end
    // send a null-response in every failure case
    this._channel.send(ResponseMessage.create(request.id, None))

  be document_diagnostic(document_uri: String, request: RequestMessage val) =>
    this._channel.log("Handling textDocument/diagnostic")
    let document_path = Uris.to_path(document_uri)
    var diagnostics = pc.Vec[JsonValue]
    try
      let package: FilePath = this._find_workspace_package(document_path)?
      match \exhaustive\ this._get_package(package)
      | let pkg_state: PackageState =>
          match pkg_state.get_document(document_path)
          | let doc: DocumentState =>
            try
              diagnostics = diagnostics.concat(
                Iter[Diagnostic]((doc.diagnostics() as Array[Diagnostic] box).values()).map[JsonValue]({(diagnostic) => diagnostic.to_json()})
              )
            end
          | None =>
            this._channel.log("[textDocument/diagnostic] No document state available for " + document_path)
          end
      | None =>
        this._channel.log("[textDocument/diagnostic] No package state available for package: " + package.path)
      end
    else
      this._channel.log("[textDocument/diagnostic] Unable to find workspace package for: " + document_uri)
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


  fun _get_node_for_highlight(ast: AST box): AST box =>
    """
    Get the appropriate node for hover highlighting.
    For declarations (class, fun, let, etc.), return the identifier child.
    For references and type nodes, find the identifier within them.
    Otherwise return the original node.
    """
    match ast.id()
    | TokenIds.tk_class() | TokenIds.tk_actor() | TokenIds.tk_trait()
    | TokenIds.tk_interface() | TokenIds.tk_primitive() | TokenIds.tk_type()
    | TokenIds.tk_struct()
    | TokenIds.tk_flet() | TokenIds.tk_fvar() | TokenIds.tk_embed()
    | TokenIds.tk_let() | TokenIds.tk_var()
    | TokenIds.tk_param() =>
      // Declarations with identifier at child(0)
      try
        let id = ast(0)?
        if id.id() == TokenIds.tk_id() then
          return id
        end
      end
    | TokenIds.tk_fun() | TokenIds.tk_be() | TokenIds.tk_new()
    | TokenIds.tk_nominal() | TokenIds.tk_typeref() =>
      // Declarations/types with identifier at child(1)
      try
        let id = ast(1)?
        if id.id() == TokenIds.tk_id() then
          return id
        end
      end
    | TokenIds.tk_funref() | TokenIds.tk_beref()
    | TokenIds.tk_newref() | TokenIds.tk_newberef() | TokenIds.tk_funchain()
    | TokenIds.tk_bechain() =>
      // For method/function references, get the method name (sibling of receiver)
      try
        let receiver = ast.child() as AST
        let method = receiver.sibling() as AST
        if method.id() == TokenIds.tk_id() then
          return method
        end
      end
    | TokenIds.tk_reference() =>
      // For references, try to find the identifier child
      try
        let id = ast(0)?
        if id.id() == TokenIds.tk_id() then
          return id
        end
      end
    end
    // Default: return original node
    ast

  be dispose() =>
    for package_state in this._packages.values() do
      package_state.dispose()
    end
