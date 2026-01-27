use ".."
use "collections"
use "files"

use "ast"
use "immutable-json"

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
  let _channel: Channel
  let _compiler: PonyCompiler
  let _packages: Map[String, PackageState]

  var _current_request: (RequestMessage val | None) = None

  new create(
    workspace': WorkspaceData,
    file_auth': FileAuth,
    channel': Channel,
    compiler': PonyCompiler)
  =>
    workspace = workspace'
    _file_auth = file_auth'
    _channel = channel'
    _compiler = compiler'
    _packages = _packages.create()

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
    this._channel.log("done compiling " + program_dir.path)
    // group errors by file
    let errors_by_file = Map[String, Array[JsonType] iso].create(4)
    // pre-fill with opened files
    // if we have no errors for them, they will get their errors cleared
    for pkg in this._packages.values() do
      for doc in pkg.documents.keys() do
        errors_by_file(doc) = []
      end
    end

    match result
    | let program: Program val =>
      this._channel.log("Successfully compiled " + program_dir.path)
      // create/update package states for every package being part of the program
      for package in program.packages() do
        let package_path = FilePath(this._file_auth, package.path)
        let package_state = this._ensure_package(package_path)
        package_state.update(package, run)
      end
    | let errors: Array[Error val] val =>

      this._channel.log("Compilation failed with " + errors.size().string() + " errors")

      for err in errors.values() do
        this._channel.log("ERROR: " + err.msg + " in file " + err.file.string())
        let diagnostic = Diagnostic.from_error(err)
        errors_by_file.upsert(
          err.file.string(),
          recover iso
            [as JsonType: diagnostic.to_json()]
          end,
          {(current: Array[JsonType] iso, provided: Array[JsonType] iso) =>
            current.append(consume provided)
            consume current
          }
        )
      end

    end
    // notify client about errors if any
    // create (or clear) error diagnostics message for each open file
    for file in errors_by_file.keys() do
      try
        let file_errors = recover val errors_by_file.remove(file)?._2 end
        let msg = Notification.create(
          "textDocument/publishDiagnostics",
          Obj("uri", Uris.from_path(file))("diagnostics", JsonArray(file_errors)).build()
        )
        this._channel.send(msg)
      end
    end


  be did_open(document_uri: String, notification: Notification val) =>
    """
    Handling the textDocument/didOpen notification
    """
    var document_path = Uris.to_path(document_uri)
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
    if not package_state.has_document(document_path) then
      (let inserted_doc_state, let has_module) = package_state.insert_new(document_path)
      if not has_module then
        _channel.log("No module found for document " + document_path + ". Need to compile.")
        _compiler.compile(package, workspace.dependency_paths, this)
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
      match package_state.get_document(document_path)
      | let doc_state: DocumentState =>
          None
          // TODO: check for differences to decide if we need to compile
          //let old_state_hash =
          //  match doc_state.module
          //  | let module: Module => module.hash()
          //  else
          //    0 // no module
          //  end
      | None =>
          // no document state found - wtf are we gonna do here?
          this._channel.log("No document state found for " + document_path + ". Dunno what to do!")
      end
      // re-compile changed program - continuing in `done_compiling`
      _channel.log("Compiling package " + package.path + " with dependency-paths: " + ", ".join(workspace.dependency_paths.values()))
      _compiler.compile(package, workspace.dependency_paths, this)
    else
      _channel.log("document not in workspace: " + document_path)
    end

  be hover(document_uri: String, request: RequestMessage val) =>
    """
    Handling the textDocument/hover request
    """
    this._current_request = request

    // Parse position from request params
    (let line, let column) = match _parse_hover_position(request)
    | (let l: I64, let c: I64) => (l, c)
    | None => return // Error already sent
    end

    let document_path = Uris.to_path(document_uri)

    // Find AST node at the position
    match _find_hover_node(document_path, line, column)
    | let hover_node: AST box =>
      this._channel.log("Found AST node: " + hover_node.debug())

      // Extract hover information and build response
      match HoverFormatter.create_hover(hover_node, this._channel)
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
      let l = JsonPath("$.position.line", request.params)?(0)? as I64 // 0-based
      let c = JsonPath("$.position.character", request.params)?(0)? as I64 // 0-based
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

      match this._get_package(package)
      | let pkg_state: PackageState =>
        match pkg_state.get_document(document_path)
        | let doc: DocumentState =>
          match doc.position_index
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

  fun _build_hover_response(hover_text: String, ast: AST box): JsonType =>
    """
    Build the LSP Hover response JSON from hover text and AST node.
    """
    let hover_contents = Obj(
      "kind", "markdown")(
      "value", hover_text
    ).build()

    // For declarations, use identifier span; for references, use the whole node
    let highlight_node = _get_node_for_highlight(ast)
    (let start_pos, let end_pos) = highlight_node.span()
    let hover_range = LspPositionRange(
      LspPosition.from_ast_pos(start_pos),
      LspPosition.from_ast_pos_end(end_pos)
    )

    Obj(
      "contents", hover_contents)(
      "range", hover_range.to_json()
    ).build()

  be goto_definition(document_uri: String, request: RequestMessage val) =>
    """
    Handling the textDocument/definition request
    """
    this._channel.log("handling goto_definition")
    this._current_request = request
    // extract the source code position
    (let line, let column) =
      try
        let l = JsonPath("$.position.line", request.params)?(0)? as I64 // 0-based
        let c = JsonPath("$.position.character", request.params)?(0)? as I64 // 0-based
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

      match this._get_package(package)
      | let pkg_state: PackageState =>
          //this._channel.log(pkg_state.debug())
          match pkg_state.get_document(document_path)
          | let doc: DocumentState =>
            match doc.position_index
            | let index: PositionIndex =>
              match index.find_node_at(USize.from[I64](line + 1), USize.from[I64](column + 1)) // pony lines and characters are 1-based, lsp are 0-based
              | let ast: AST box =>
                this._channel.log(ast.debug())
                var json_builder = Arr.create()
                // iterate through all found definitions
                for ast_definition in ast.definitions().values() do
                  // get position of the found definition
                  (let start_pos, let end_pos) = ast_definition.span()
                  try
                    // append new location
                    json_builder = json_builder(
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
                this._channel.send(ResponseMessage(request.id, json_builder.build()))
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
      match this._get_package(package)
      | let pkg_state: PackageState =>
          //this._channel.log(pkg_state.debug())
          match pkg_state.get_document(document_path)
          | let doc: DocumentState =>
            let symbols = doc.document_symbols()
            var json_builder = Arr
            for symbol in symbols.values() do
              json_builder = json_builder(symbol.to_json())
            end
            this._channel.send((ResponseMessage(request.id, json_builder.build())))
            return
          | None =>
            this._channel.log("No document state available for " + document_path)
          end
      | None =>
        this._channel.log("No package state available for package: " + package.path)
      end
      // send a null-response in every failure case
    else
      this._channel.log("document not in workspace: " + document_path)
    end
    this._channel.send(ResponseMessage.create(request.id, None))

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
