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

  let _folders: WorkspaceFolders
  let _file_auth: FileAuth
  let _client: Client
  let _channel: Channel
  let _request_sender: RequestSender
  let _compiler: LspCompiler
  let _modules: Map[String, ModuleState]
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
    workspace_folders: Array[String val] val,
    file_auth': FileAuth,
    channel': Channel,
    request_sender': RequestSender,
    client': Client,
    compiler': LspCompiler)
  =>
    _file_auth = file_auth'
    _folders = WorkspaceFolders(_file_auth, workspace_folders)
    _client = client'
    _channel = channel'
    _request_sender = request_sender'
    _compiler = compiler'
    _modules = _modules.create()
    _global_errors =
      Array[Diagnostic val].create(4)

  fun ref _ensure_module(module_path: FilePath): ModuleState =>
    try
      this._modules(module_path.path)?
    else
      this._modules.insert(
        module_path.path,
        ModuleState.create(ModuleDataBuilder.from_path(module_path), this._channel))
    end

  fun _get_module(module_path: FilePath): (this->ModuleState | None) =>
    try
      this._modules(module_path.path)?
    else
      None
    end

  be add_modules(mods: Array[ModuleData val] val) =>
    // lets not overwrite here
    for data in mods.values() do
      this._modules.insert_if_absent(
        data.folder.path,
        ModuleState.create(data, this._channel)
      )
    end

  fun box _find_workspace_module(document_path: String): this->ModuleState ? =>
    """
    Try to find the module in the workspace from the
    given `document_path` by iterating up the directory
    tree to see if any dir belongs to one of the
    modules of the workspace.

    Errors if no module could be found.
    """
    var dir_path = Path.dir(document_path)
    while (dir_path != ".") do
      try
        let mod = this._modules(dir_path)?
        return mod
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
          JSONObject
            .update(
              "token",
              this._compilation_token(program_dir))
            .update(
              "value",
              JSONObject
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
      let errors_by_file = Map[String, pc.Vec[JSONValue]].create(4)

      if this._client.supports_publish_diagnostics() then
        // pre-fill with empty list of errors for
        // all files for which we have errors now
        for mod in this._modules.values() do
          for doc in mod.document_paths() do
            errors_by_file(doc) = pc.Vec[JSONValue]
          end
        end
      end

      // clear out global errors
      this._global_errors.clear()

      match \exhaustive\ result
      | let program: Program val =>
        this._channel.log("Successfully compiled " + program_dir.path)
        // create/update module states for every
        // module being part of the program
        for mod in program.modules() do
          let module_path = FilePath(this._file_auth, mod.path)
          let module_state = this._ensure_module(module_path)
          module_state.update(mod, run)

          var requires_another_compilation = false
          for (await_comp_file, await_comp_hash) in
            this._awaiting_compilation_for.pairs()
          do
            let await_mod_path = Path.base(await_comp_file)
            if module_state.data.folder.path == await_mod_path then
              try
                // get the hash of the module file
                // ponyc considered for compilation
                let new_mod_hash =
                  (module_state.get_document(await_comp_file) as DocumentState)
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
              program_dir.path + " needs another compilation...")
            this._compile(module_state)
          end
        end
      | let errors: Array[Error val] val =>
        this._channel.log(
          "Compilation failed with " + errors.size().string() + " errors")

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
            // TODO: check path
            let err_module_path = FilePath(this._file_auth, Path.base(err_file))
            // store the error in the document-state
            let module_state = this._ensure_module(err_module_path)
            let document_state = module_state.ensure_document(err_file)
            document_state.add_diagnostic(run, diagnostic)

            if this._client.supports_publish_diagnostics() then
              // collect errors by file
              errors_by_file.upsert(
                err_file,
                pc.Vec[JSONValue].push(diagnostic.to_json()),
                {(current: pc.Vec[JSONValue], provided: pc.Vec[JSONValue]) =>
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
            let file_errors: pc.Vec[JSONValue] = errors_by_file(file)?
            let msg =
              Notification.create(
                Methods.text_document().publish_diagnostics(),
                JSONObject
                  .update("uri", Uris.from_path(file))
                  .update("diagnostics", JSONArray(consume file_errors)))
            this._channel.send(msg)
          end
        end

        // publish global diagnostics if any
        let num_global_errs = this._global_errors.size()
        if (num_global_errs > 0) then
          let json_global_diagnostics =
            pc.Vec[JSONValue].concat(
              Iter[Diagnostic](
                this._global_errors.values())
                  .map[JSONValue]({(diagnostic) => diagnostic.to_json() }))
          let global_notifications =
            Notification.create(
              Methods.text_document().publish_diagnostics(),
              JSONObject
                .update("uri", "global")
                .update("diagnostics", JSONArray(json_global_diagnostics)))
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

  fun ref _compile(module: ModuleState) =>
    """
    Central entry point for triggering a compilation.

    Does all the notifying and bookkeeping before
    actually triggering the compilation.
    """
    this._channel.log(
      "Compiling module " + module.data.folder.path + " with dependencies: " +
      ", ".join(module.data.dependencies.values()))

    let token = this._compilation_token(module.data.folder)
    if this._client.supports_window_work_done_progress()
    then
      let chan = this._channel
      let progress_begin_notification =
        Notification(
          Methods.progress(),
          JSONObject
            .update("token", token)
            .update(
              "value",
              JSONObject
                .update("kind", "begin")
                .update(
                  "title",
                  "Compiling " + Path.base(module.data.folder.path) +
                  " (" + this._compile_run.string() + ")")
                .update("cancellable", false)
            )
        )
      this._request_sender.send_request(
        Methods.window().work_done_progress().create(),
        JSONObject.update("token", token),
        object is ResponseNotify
          let _chan: Channel = chan
          be notify(method: String val, r: ResponseMessage val) =>
            if r.is_result() then
              _chan.send(progress_begin_notification)
            end
        end
      )
    end
    _compiler.compile(module.data.folder, module.data.dependencies, this)
    _compiling = true

  fun _compilation_token(module: FilePath): String =>
    recover val
      String.create(128)
        .> append("pony-lsp")
        .> append(module.path)
        .> append("/compile/")
        .> append(this._compile_run.string())
    end

  be did_open(document_uri: String, notification: Notification val) =>
    """
    Handling the textDocument/didOpen notification.
    """
    let document_path = Uris.to_path(document_uri)
    this._channel.log("handling did_open of " + document_path)

    let module_path = FilePath(this._file_auth, Path.dir(document_path))
    let module_state: ModuleState = this._ensure_module(module_path)
    this._channel.log(
      "did_open in pony module @ " + module_state.data.folder.path)
    let doc_state = module_state.ensure_document(document_path)
    if doc_state.needs_compilation() then
      if this._compiling then
        let current_hash = doc_state.module_hash()
        this._awaiting_compilation_for.insert(document_path, current_hash)
      else
        _channel.log(
          "No module found for document " + document_path +
          ". Need to compile.")
        this._compile(module_state)
      end
    end

  be did_close(document_uri: String, notification: Notification val) =>
    """
    Handling the textDocument/didClose notification.
    """
    let document_path = Uris.to_path(document_uri)
    this._channel.log("handling did_close of " + document_path)

    let module_path = FilePath(this._file_auth, Path.dir(document_path))
    let module_state: ModuleState = this._ensure_module(module_path)
    try
      module_state.remove_document(document_path)?
    end

  be did_save(document_uri: String, notification: Notification val) =>
    """
    Handling the textDocument/didSave notification.
    """
    let document_path = Uris.to_path(document_uri)
    this._channel.log("handling did_save of " + document_path)
    let module_path = FilePath(this._file_auth, Path.dir(document_path))
    let module_state = this._ensure_module(module_path)
    if this._compiling then
      // put the hash of the doc at saving time
      let text_content_hash =
        try
          (JSONPathParser.compile("$.text")?
            .query_one(notification.params) as String).hash()
        end
      this._awaiting_compilation_for.insert(document_path, text_content_hash)
    else
      this._compile(module_state)
    end

  be hover(document_uri: String, request: RequestMessage val) =>
    """
    Handling the textDocument/hover request.
    """
    // Parse position from request params
    (let line, let column) =
      match \exhaustive\ _parse_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return // Error already sent
      end

    let document_path = Uris.to_path(document_uri)

    // Find AST node at the position
    match \exhaustive\ _find_node_and_file(document_path, line, column)
    | (let hover_node: AST box, _) =>
      this._channel.log("Found AST node: " + hover_node.debug())

      // Suppress hover when the cursor lands directly on a declaration keyword
      // (fun/be/new, let/var/embed for fields, let/var for locals) or a
      // capability keyword (iso/trn/ref/val/box/tag).
      // _refine_node promotes the name tk_id and cap keywords to their parent
      // node, so compare the cursor position against each keyword's span.
      let cursor_line = USize.from[I64](line + 1)
      let cursor_col = USize.from[I64](column + 1)
      let keyword_len: USize =
        match hover_node.id()
        | TokenIds.tk_fun() | TokenIds.tk_new()
        | TokenIds.tk_flet() | TokenIds.tk_let()
        | TokenIds.tk_fvar() | TokenIds.tk_var() => 3
        | TokenIds.tk_be() => 2
        | TokenIds.tk_embed() => 5
        else 0
        end
      if keyword_len > 0 then
        let node_line = hover_node.position().line()
        let node_col = hover_node.position().column()
        if (cursor_line == node_line) and
          (cursor_col >= node_col) and
          (cursor_col <= ((node_col + keyword_len) - 1))
        then
          this._channel.send(ResponseMessage.create(request.id, None))
          return
        end
      end
      // Suppress hover on capability keywords (iso/trn/ref/val/box/tag).
      // _refine_node navigates from cap keywords to their parent, so scan
      // the refined node's children for a cap keyword covering the cursor.
      for child in hover_node.children() do
        let child_id = child.id()
        if (child_id == TokenIds.tk_iso()) or (child_id == TokenIds.tk_trn()) or
          (child_id == TokenIds.tk_ref()) or (child_id == TokenIds.tk_val()) or
          (child_id == TokenIds.tk_box()) or (child_id == TokenIds.tk_tag())
        then
          let cap_col = child.position().column()
          if (cursor_line == child.position().line()) and
            (cursor_col >= cap_col) and
            (cursor_col <= (cap_col + 2))
          then
            this._channel.send(ResponseMessage.create(request.id, None))
            return
          end
        end
      end

      // Extract hover information and build response
      match \exhaustive\ HoverFormatter.create_hover(hover_node)
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

  fun ref _parse_position(
    request: RequestMessage val
  ): ((I64, I64) | None) =>
    """
    Extract line and column from request params.
    Sends error response and returns None if invalid.
    """
    try
      let nav = JSONNav(request.params)("position")
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

  fun ref _find_node_and_file(
    document_path: String,
    line: I64,
    column: I64): ((AST box, SourceFile val) | None)
  =>
    """
    Find the AST node and its file at the given position.
    Returns None if the document or file is not found,
    or no node exists at that position.
    """
    match _get_index_and_file(document_path)
    | (let index: PositionIndex val, let f: SourceFile val) =>
      match index.find_node_at(
        USize.from[I64](line + 1),
        USize.from[I64](column + 1))
      | let node: AST box => (node, f)
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
    match _find_node_and_file(document_path, sel_line, sel_col)
    | (let node: AST box, _) =>
      CallHierarchy._method_for_node(node)
    end

  fun ref _find_entity_node(
    document_path: String,
    sel_line: I64,
    sel_col: I64): (AST box | None)
  =>
    """
    Resolve the entity AST node at the given position. Returns None when the
    document has not been compiled or no entity node is found at that position.
    """
    if (sel_line < 0) or (sel_col < 0) then
      return None
    end
    match _find_node_and_file(document_path, sel_line, sel_col)
    | (let node: AST box, _) =>
      TypeHierarchy._entity_for_node(node)
    end

  fun ref _get_index_and_file(
    document_path: String): ((PositionIndex val, SourceFile val) | None)
  =>
    """
    Returns the position index and compiled module for the given document
    path, or None if the document is not in any workspace, has not yet
    been compiled, or its index is not yet available.
    """
    try
      let mod_state = this._find_workspace_module(document_path)?
      match \exhaustive\ mod_state.get_document(document_path)
      | let doc: DocumentState =>
        match (doc.position_index(), doc.file())
        | (let index: PositionIndex val, let source_file: SourceFile val) =>
          (index, source_file)
        else
          this._channel.log(
            "No index or module available for " + document_path)
          None
        end

      else
        this._channel.log("No document state available for " + document_path)
        None
      end
    else
      this._channel.log(
        "No package state available for document: " + document_path)
      None
    end

  fun _build_hover_response(hover_text: String, ast: AST box): JSONValue =>
    """
    Build the LSP Hover response JSON from hover text and AST node.
    """
    let hover_contents = JSONObject
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

    JSONObject
      .update("contents", hover_contents)
      .update("range", hover_range.to_json())

  be document_highlight(document_uri: String, request: RequestMessage val) =>
    """
    Handle textDocument/documentHighlight request.
    """
    this._channel.log("Handling textDocument/documentHighlight")
    (let line, let column) =
      match \exhaustive\ _parse_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let document_path = Uris.to_path(document_uri)
    match _find_node_and_file(document_path, line, column)
    | (let node: AST box, let f: SourceFile val) =>
      let highlights = DocumentHighlights.collect(node, f)
      var json_arr = JSONArray
      for (range, kind) in highlights.values() do
        json_arr =
          json_arr.push(
            JSONObject
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
      match \exhaustive\ _parse_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let include_declaration: Bool =
      try
        JSONNav(request.params)("context")("includeDeclaration").as_bool()?
      else
        true
      end
    let document_path = Uris.to_path(document_uri)
    match _find_node_and_file(document_path, line, column)
    | (let node: AST box, _) =>
      let locations =
        References.collect(node, this._modules, include_declaration)
      var json_arr = JSONArray
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
      match \exhaustive\ _parse_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let document_path = Uris.to_path(document_uri)
    match _find_node_and_file(document_path, line, column)
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
      if not this._folders.contains(target_file) then
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
      match \exhaustive\ _parse_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let new_name: String val =
      try
        JSONNav(request.params)("newName").as_string()?
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
    match _find_node_and_file(document_path, line, column)
    | (let node: AST box, _) =>
      match \exhaustive\ Rename.collect(
        node,
        this._modules,
        new_name,
        this._folders)

      | let workspace_edit: JSONObject val =>
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
      match \exhaustive\ _parse_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let document_path = Uris.to_path(document_uri)
    match \exhaustive\ _find_node_and_file(document_path, line, column)
    | (let ast: AST box, _) =>
      this._channel.log(ast.debug())
      var json_arr = JSONArray
      for ast_definition in ast.definitions().values() do
        match \exhaustive\ _node_location(ast_definition)
        | let loc: LspLocation val =>
          json_arr = json_arr.push(loc.to_json())
        | None =>
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

  be type_definition(document_uri: String, request: RequestMessage val) =>
    """
    Handle textDocument/typeDefinition request.

    Finds the definition of the type of the symbol at the cursor position.
    E.g. for a local variable `x: MyClass`, this navigates to `MyClass`.
    """
    this._channel.log("handling textDocument/typeDefinition")
    (let line, let column) =
      match \exhaustive\ _parse_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let document_path = Uris.to_path(document_uri)
    match \exhaustive\ _find_node_and_file(document_path, line, column)
    | (let ast: AST box, _) =>
      this._channel.log(ast.debug())
      var json_arr = JSONArray
      match ast.ast_type()
      | let type_ast: AST =>
        for type_def in type_ast.definitions().values() do
          match \exhaustive\ _node_location(type_def)
          | let loc: LspLocation val =>
            json_arr = json_arr.push(loc.to_json())
          | None =>
            this._channel.log(
              "No source file found for type definition: " + type_def.debug())
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

      let mod_state: ModuleState = this._find_workspace_module(document_path)?
      // this._channel.log(mod_state.debug())
      match \exhaustive\ mod_state.get_document(document_path)
      | let doc: DocumentState =>
        let symbols = doc.document_symbols()
        var json_arr = JSONArray
        for symbol in symbols.values() do
          json_arr = json_arr.push(symbol.to_json())
        end
        this._channel.send(ResponseMessage(request.id, json_arr))
        return
      | None =>
        this._channel.log("No document state available for " + document_path)
      end
    else
      this._channel.log(
        "No module state available for document: " + document_path)
    end
    // send a null-response in every failure case
    this._channel.send(ResponseMessage.create(request.id, None))

  be workspace_symbol(
    query: String val,
    request: RequestMessage val)
  =>
    """
    Handle workspace/symbol request.
    Collects symbols matching the query across all modules
    in this workspace.
    """
    this._channel.log("Handling workspace/symbol: '" + query + "'")
    let query_lower: String val = query.lower()
    var results = JSONArray
    for pkg_state in this._modules.values() do
      for doc_state in pkg_state.document_states() do
        let file_uri = Uris.from_path(doc_state.path)
        let top_symbols = doc_state.document_symbols()
        for symbol in top_symbols.values() do
          if _symbol_matches(symbol.name, query_lower) then
            results =
              results.push(
                JSONObject
                  .update("name", symbol.name)
                  .update("kind", symbol.kind)
                  .update(
                    "location",
                    JSONObject
                      .update("uri", file_uri)
                      .update("range", symbol.range.to_json())))
          end
          for child in symbol.children.values() do
            if _symbol_matches(child.name, query_lower) then
              results =
                results.push(
                  JSONObject
                    .update("name", child.name)
                    .update("kind", child.kind)
                    .update("containerName", symbol.name)
                    .update(
                      "location",
                      JSONObject
                        .update("uri", file_uri)
                        .update("range", child.range.to_json())))
            end
          end
        end
      end
    end
    this._channel.send(ResponseMessage(request.id, results))

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
    var diagnostics = pc.Vec[JSONValue]
    try
      let mod_state: ModuleState = this._find_workspace_module(document_path)?
      match \exhaustive\ mod_state.get_document(document_path)
      | let doc: DocumentState =>
        try
          diagnostics =
            diagnostics
              .concat(
                Iter[Diagnostic]((doc.diagnostics() as Array[Diagnostic] box)
                  .values())
              .map[JSONValue]({(diagnostic) => diagnostic.to_json() }))
        end
      | None =>
        this._channel.log(
          "[textDocument/diagnostic] No document state available for "
          + document_path)
      end
    else
      this._channel.log(
          "[textDocument/diagnostic] No module state available for: "
          + document_path)
    end
    this._channel.send(
      ResponseMessage.create(
        request.id,
        JSONObject
          .update("kind", "full")
          .update("resultId", this._compile_run.string())
          .update("items", JSONArray(diagnostics))
      )
    )

  be inlay_hint(document_uri: String, request: RequestMessage val) =>
    """
    Handle textDocument/inlayHint request.
    """
    this._channel.log("Handling textDocument/inlayHint")
    let document_path = Uris.to_path(document_uri)
    try
      let mod_state: ModuleState = this._find_workspace_module(document_path)?
      match \exhaustive\ mod_state.get_document(document_path)
      | let doc: DocumentState =>
        match \exhaustive\ doc.file()
        | let source_file: SourceFile val =>
          let range: (None | (I64, I64, I64, I64)) =
            try
              let p = request.params
              let sl = JSONNav(p)("range")("start")("line").as_i64()?
              let sc = JSONNav(p)("range")("start")("character").as_i64()?
              let el = JSONNav(p)("range")("end")("line").as_i64()?
              let ec = JSONNav(p)("range")("end")("character").as_i64()?
              (sl, sc, el, ec)
            else
              None
            end
          let hints = InlayHints.collect(source_file, range)
          var json_arr = JSONArray
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
    else
      this._channel.log(
          "No module state available for " + document_path)
    end
    this._channel.send(ResponseMessage.create(request.id, None))

  be folding_range(document_uri: String, request: RequestMessage val) =>
    """
    Handle textDocument/foldingRange request.
    """
    this._channel.log("Handling textDocument/foldingRange")
    let document_path = Uris.to_path(document_uri)
    try
      let mod_state: ModuleState = this._find_workspace_module(document_path)?
      match \exhaustive\ mod_state.get_document(document_path)
      | let doc: DocumentState =>
        match \exhaustive\ doc.file()
        | let source_file: SourceFile val =>
          let fold_ranges = FoldingRanges.collect(source_file)
          var json_arr = JSONArray
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
    else
      this._channel.log(
        "No module state available for " + document_path)
    end
    this._channel.send(ResponseMessage.create(request.id, None))

  be selection_range(document_uri: String, request: RequestMessage val) =>
    """
    Handle textDocument/selectionRange request.

    Parses the `positions` array from the request params. For each position,
    finds the AST node under the cursor and builds a SelectionRange linked list
    from innermost to outermost. The response is a JSONArray parallel to the
    request positions array; entries are null when no node is found at a
    position. Returns null if the request has no valid `positions` array.
    """
    this._channel.log("Handling textDocument/selectionRange")
    let document_path = Uris.to_path(document_uri)

    // Each position maps to one response entry (null when no node found), so
    // the output array is always parallel to the input positions array.
    var out = JSONArray
    try
      let arr = JSONNav(request.params)("positions").as_array()?
      for pos_val in arr.values() do
        let entry: JSONValue =
          try
            let l = JSONNav(pos_val)("line").as_i64()?
            let c = JSONNav(pos_val)("character").as_i64()?
            match _find_node_and_file(document_path, l, c)
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
      match \exhaustive\ _parse_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let document_path = Uris.to_path(document_uri)
    // Convert LSP 0-based cursor to 1-based AST coordinates.
    let cursor_line = USize.from[I64](line + 1)
    let cursor_col  = USize.from[I64](column + 1)
    match _get_index_and_file(document_path)
    | (let index: PositionIndex val, let f: SourceFile val) =>
      // Scan the source text forward from the cursor, skipping whitespace
      // and commas, to find the next real token.  This handles the case
      // where the cursor is on a separator character or trailing whitespace
      // where no AST node exists — without magic column limits.
      (let tok_line, let tok_col) =
        match \exhaustive\ f.ast.source_contents()
        | let src: String box =>
          SignatureHelpSource.scan_to_token(src, cursor_line, cursor_col)
        | None =>
          (cursor_line, cursor_col)
        end
      match index.find_node_at(tok_line, tok_col)
      | let node: AST box =>
        match \exhaustive\ SignatureHelp.collect(
          node, tok_line, tok_col)
        | let result: JSONObject =>
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
      match \exhaustive\ _parse_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let document_path = Uris.to_path(document_uri)
    match \exhaustive\ _find_node_and_file(document_path, line, column)
    | (let node: AST box, _) =>
      match CallHierarchy.prepare(node)
      | let result: JSONArray =>
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
      match CallHierarchy.incoming_calls(node, this._modules)
      | let result: JSONArray =>
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
      match CallHierarchy.outgoing_calls(node, this._modules)
      | let result: JSONArray =>
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
      match \exhaustive\ _parse_position(request)
      | (let l: I64, let c: I64) => (l, c)
      | None => return
      end
    let document_path = Uris.to_path(document_uri)
    match \exhaustive\ _find_node_and_file(document_path, line, column)
    | (let node: AST box, _) =>
      match TypeHierarchy.prepare(node)
      | let result: JSONArray =>
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
    match _find_entity_node(document_path, sel_line, sel_col)
    | let node: AST box =>
      match TypeHierarchy.supertypes(node)
      | let result: JSONArray =>
        this._channel.send(ResponseMessage(request.id, result))
        return
      end
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
    match _find_entity_node(document_path, sel_line, sel_col)
    | let node: AST box =>
      match TypeHierarchy.subtypes(node, this._modules)
      | let result: JSONArray =>
        this._channel.send(ResponseMessage(request.id, result))
        return
      end
    end
    this._channel.send(ResponseMessage.create(request.id, None))

  be dispose() =>
    for module_state in this._modules.values() do
      module_state.dispose()
    end
