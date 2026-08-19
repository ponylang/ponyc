use "collections"
use "files"
use "itertools"

use "pony_compiler"

use ".."

class PackageState
  """
  State of a compiled package, containing its modules and document states.
  """
  let data: PackageData
  let _documents: Map[String, DocumentState]
  let _channel: Channel
  var _package: FromCompilerRun[Package]
  var _compiler_run_id: USize

  new create(data': PackageData, channel: Channel) =>
    data = data'
    _channel = channel
    _documents = _documents.create()
    _package = _package.empty()
    _compiler_run_id = 0

  fun debug(): String val =>
    "Package " + this.data.folder.path + " (" + (
      try
        (package() as Package).qualified_name
      else
        ""
      end
    ) + "):\n\t" + "\n\t".join(
      Iter[(String box, DocumentState box)](_documents.pairs())
        .map[String]({(kv) =>
          kv._1 + " (" +
            if kv._2.module() isnt None then
              "M"
            else
              "_"
            end + ")"
        })
    )

  fun package(): (Package | None) =>
    _package.get(_compiler_run_id)

  fun get_document(document_path: String): (this->DocumentState | None) =>
    try
      _documents(document_path)?
    end

  fun has_document(document_path: String): Bool =>
    _documents.contains(document_path)

  fun document_paths(): Iterator[String val]^ =>
    _documents.keys()

  fun ref document_states(): Iterator[DocumentState ref]^ =>
    _documents.values()

  fun ref remove_document(document_path: String) ? =>
    _documents.remove(document_path)?._2.dispose()

  fun ref insert_new(document_path: String): DocumentState =>
    """
    Insert a new module by the given `document_path` into this package.
    """
    let doc_state = DocumentState.create(document_path, _channel)

    try
      // populate the document from the last compilation result if available
      let pkg = package() as Package
      let module = pkg.find_module(document_path) as Module
      doc_state.update(_compiler_run_id, module)
    end
    _documents.insert(document_path, doc_state)

  fun ref ensure_document(document_path: String): DocumentState =>
    """
    Returns the document state in a tuple together with a boolean,
    denoting whether this documents needs compilation.
    """
    match \exhaustive\ get_document(document_path)
    | let d: DocumentState => d
    | None => insert_new(document_path)
    end

  fun ref update(result: Package val, run_id: USize) =>
    """
    Update the current package state with the result
    of the compilation run identified by `run_id`.
    """
    if run_id >= _compiler_run_id then
      _compiler_run_id = run_id
      _package.update(run_id, result)
      _channel.log("Updating package " + result.path)
      _channel.log(debug())
      // for each open document, update the
      // document state if we have a module for it
      for (doc_path, doc_state) in _documents.pairs() do
        // TODO: ensure both module and package-state paths are normalised
        match \exhaustive\ result.find_module(doc_path)
        | let m: Module val => doc_state.update(run_id, m)
        | None => _channel.log("No module found for " + doc_path)
        end
      end
    end

  fun ref add_diagnostic(run_id: USize, diagnostic: Diagnostic) =>
    if run_id >= _compiler_run_id then
      _compiler_run_id = run_id
      // TODO: store package/document diagnostics
    end

  fun dispose() =>
    for doc_state in _documents.values() do
      doc_state.dispose()
    end

class DocumentState
  """
  State of a single document within a package.
  """
  let path: String
  let _channel: Channel
  var _module: FromCompilerRun[Module val]
  var _diagnostics: FromCompilerRun[Array[Diagnostic] ref]
  var _position_index: FromCompilerRun[PositionIndex val]
  var _document_symbols: FromCompilerRun[Array[DocumentSymbol]]
  var _hash: FromCompilerRun[USize]
  var _compiler_run_id: USize

  new create(path': String, channel': Channel) =>
    path = path'
    _channel = channel'
    _diagnostics = _diagnostics.empty()
    _module = _module.empty()
    _position_index = _position_index.empty()
    _document_symbols = _document_symbols.empty()
    _hash = _hash.empty()
    _compiler_run_id = 0

  fun ref update(run_id: USize, module': Module val) =>
    if run_id >= _compiler_run_id then
      _compiler_run_id = run_id
      // clear out diagnostics
      _module.update(run_id, module')
      _position_index =
        FromCompilerRun[PositionIndex val]
          .create(run_id, module'.create_position_index())
      _document_symbols =
        FromCompilerRun[Array[DocumentSymbol]]
          .create(run_id, DocumentSymbols.from_module(module', _channel))
      _hash.update(run_id, module'.hash())
    end

  fun ref add_diagnostic(run_id: USize, diagnostic: Diagnostic) =>
    // also accept diagnostics from the last/current run,
    // as they trickle in one after the other
    if run_id == _compiler_run_id then
      // same run, update diagnostics
      _diagnostics.update_with(
        run_id,
        {
          (diags: (Array[Diagnostic] | None)): Array[Diagnostic] =>
            match \exhaustive\ diags
            | let arr: Array[Diagnostic] =>
              arr.push(diagnostic)
              consume arr
            | None =>
              [diagnostic]
            end
        })
    elseif run_id > _compiler_run_id then
      // new run, discard old diagnostics
      _diagnostics.update(run_id, [diagnostic])
    end

  fun box needs_compilation(): Bool =>
    (module() is None) and (diagnostics() is None)

  fun box module(): (Module val | None) =>
    _module.get(_compiler_run_id)

  fun box diagnostics(): (Array[Diagnostic] box | None) =>
    _diagnostics.get(_compiler_run_id)

  fun box position_index(): (PositionIndex val | None) =>
    _position_index.get(_compiler_run_id)

  fun box module_hash(): (USize | None) =>
    _hash.get(_compiler_run_id)

  fun ref document_symbols(): Array[DocumentSymbol] =>
    """
    Get or create the current document symbols.
    """
    match module()
    | let mod: Module val =>
      let created = DocumentSymbols.from_module(mod, _channel)
      _document_symbols.update(_compiler_run_id, created)
      created
    else
      // no module available yet, no documentsymbols available yet
      _channel.log("No module available yet for " + path)
      []
    end

  fun dispose() =>
    None

class ref FromCompilerRun[T: Any #read]
  """
  Something associated with a certain compiler run, identified by a `USize`.

  ## Staleness contract

  `update` and `update_with` accept any run ID `>=` the stored one, so that
  multiple writes within the same run (e.g. diagnostics trickling in) are all
  accepted, and advancing to a newer run is also accepted.

  `get` requires an exact `==` match. This is deliberate: a caller holding an
  older run ID must not receive data that was written for a newer run, and a
  caller holding a newer run ID must not receive stale data from an older run.
  The result is a window — between the first `add_diagnostic` call for a new
  run and the subsequent `update` call for that run — during which `get`
  returns `None` even though data exists. This keeps the visible state
  consistent: data only surfaces once the full state for a given run is ready.
  """
  var _thing: (T | None)
  var _compiler_run_id: USize

  new ref empty() =>
    _compiler_run_id = 0
    _thing = None

  new ref create(compiler_run_id': USize, thing': T) =>
    _compiler_run_id = compiler_run_id'
    _thing = thing'

  fun ref update(compiler_run_id': USize, new_thing: (T | None)) =>
    // ignore update if the compiler_run_id' is old
    if compiler_run_id' >= _compiler_run_id then
      _compiler_run_id = compiler_run_id'
      _thing = new_thing
    end

  fun ref update_with(
    compiler_run_id': USize, closure: {((T | None)): (T^ | None)}) =>
    """
    Execute the given closure on the current state
    of the encapsulated thing, which might be `None`.
    """
    if compiler_run_id' >= _compiler_run_id then
      _compiler_run_id = compiler_run_id'
      _thing = closure.apply(_thing)
    end

  fun get(current_run_id: USize): (this->T | None) =>
    if current_run_id == _compiler_run_id then
      _thing
    else
      None
    end
