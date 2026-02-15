use "collections"
use "files"
use "itertools"

use "pony_compiler"

use ".."

class PackageState
  let path: FilePath
  let documents: Map[String, DocumentState]
  let _channel: Channel

  var _package: FromCompilerRun[Package]
  var _compiler_run_id: USize

  new create(path': FilePath, channel: Channel) =>
    path = path'
    _channel = channel
    documents = documents.create()
    _package = _package.empty()
    _compiler_run_id = 0

  fun debug(): String val =>
    "Package " + this.path.path + " (" + (
      try
        (this.package() as Package).qualified_name
      else
        ""
      end
    ) + "):\n\t" + "\n\t".join(
      Iter[(String box, DocumentState box)](documents.pairs())
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
    this._package.get(this._compiler_run_id)

  fun get_document(document_path: String): (this->DocumentState | None) =>
    try
      this.documents(document_path)?
    end

  fun has_document(document_path: String): Bool =>
    this.documents.contains(document_path)

  fun ref insert_new(document_path: String): DocumentState =>
    """
    Insert a new module by the given `document_path` into this package
    """
    let doc_state = DocumentState.create(document_path, this._channel)

    try
      // populate the document from the last compilation result if available
      let pkg = this.package() as Package
      let module = pkg.find_module(document_path) as Module
      doc_state.update(this._compiler_run_id, module)
    end
    this.documents.insert(
      document_path,
      doc_state
    )

  fun ref ensure_document(document_path: String): DocumentState =>
    """
    Returns the document state in a tuple together with a boolean, denoting
    whether this documents needs compilation
    """
    match this.get_document(document_path)
    | let d: DocumentState =>
      d
    | None =>
      this.insert_new(document_path)
    end


  fun ref update(result: Package val, run_id: USize) =>
    """
    Update the current package state with the result of the compilation run identified by `run_id`.
    """
    if run_id >= this._compiler_run_id then
      this._compiler_run_id = run_id
      this._package.update(run_id, result)
      this._channel.log("Updating package " + result.path)
      this._channel.log(this.debug())
      // for each open document, update the document state if we have a module for it
      for (doc_path, doc_state) in this.documents.pairs() do
        // TODO: ensure both module and package-state paths are normalized
        match result.find_module(doc_path)
        | let m: Module val =>
          // update each document state
          doc_state.update(run_id, m)
        | None =>
          this._channel.log("No module found for " + doc_path)
        end
      end
    end

  fun ref add_diagnostic(run_id: USize, diagnostic: Diagnostic) =>
    if run_id >= this._compiler_run_id then
      this._compiler_run_id = run_id
      // TODO:
    end

  fun dispose() =>
    for doc_state in this.documents.values() do
      doc_state.dispose()
    end

class DocumentState
  let path: String
  let _channel: Channel

  var _module: FromCompilerRun[Module val]
  var _diagnostics: FromCompilerRun[Array[Diagnostic] ref]
  var _position_index: FromCompilerRun[PositionIndex val]
  var _document_symbols: FromCompilerRun[Array[DocumentSymbol]]
  var _hash: FromCompilerRun[USize]
  var compiler_run_id: USize

  new create(path': String, channel': Channel) =>
    path = path'
    _channel = channel'
    _diagnostics = _diagnostics.empty()
    _module = _module.empty()
    _position_index = _position_index.empty()
    _document_symbols = _document_symbols.empty()
    _hash = _hash.empty()
    compiler_run_id = 0

  fun ref update(run_id: USize, module': Module val) =>
    if run_id >= this.compiler_run_id then
      this.compiler_run_id = run_id
      // clear out diagnostics
      this._module.update(run_id, module')
      this._position_index = FromCompilerRun[PositionIndex val].create(run_id, module'.create_position_index())
      this._document_symbols = FromCompilerRun[Array[DocumentSymbol]].create(run_id, DocumentSymbols.from_module(module', this._channel))
      this._hash.update(run_id, module'.hash())
    end

  fun ref add_diagnostic(run_id: USize, diagnostic: Diagnostic) =>
    // also accept diagnostics from the last/current run, as they trickle in one
    // after the other
    if run_id == this.compiler_run_id then
      // same run, update diagnostics
      this._diagnostics.update_with(
        run_id,
        {
          (diags: (Array[Diagnostic] | None)): Array[Diagnostic] =>
            match diags
            | let arr: Array[Diagnostic] =>
              arr.push(diagnostic)
              consume arr
            | None =>
              [diagnostic]
            end
        })
    elseif run_id > this.compiler_run_id then
      // new run, discard old diagnostics
      this._diagnostics.update(run_id, [diagnostic])
    end

  fun box needs_compilation(): Bool =>
    (this.module() is None) and (this.diagnostics() is None)

  fun box module(): (Module val | None) =>
    this._module.get(compiler_run_id)

  fun box diagnostics(): (Array[Diagnostic] box | None) =>
    this._diagnostics.get(compiler_run_id)

  fun box position_index(): (PositionIndex val | None) =>
    this._position_index.get(compiler_run_id)

  fun box module_hash(): (USize | None) =>
    this._hash.get(compiler_run_id)

  fun ref document_symbols(): Array[DocumentSymbol] =>
    """
    Get or create the current document symbols
    """
    match this.module()
    | let mod: Module val =>
      let created = DocumentSymbols.from_module(mod, this._channel)
      this._document_symbols.update(this.compiler_run_id, created)
      created
    else
      // no module available yet, no documentsymbols available yet
      this._channel.log("No module available yet for " + this.path)
      []
    end

  fun dispose() =>
    None

class ref FromCompilerRun[T: Any #read]
  """
  Something associated with a certain compiler run, identified by a `USize`.
  """
  var _thing: (T | None)
  var compiler_run_id: USize

  new ref empty() =>
    this.compiler_run_id = 0
    this._thing = None

  new ref create(compiler_run_id': USize, thing': T) =>
    this.compiler_run_id = compiler_run_id'
    this._thing = thing'

  fun ref update(compiler_run_id': USize, new_thing: (T | None)) =>
    // ignore update if the compiler_run_id' is old
    if compiler_run_id' >= this.compiler_run_id then
      this.compiler_run_id = compiler_run_id'
      this._thing = new_thing
    end

  fun ref update_with(compiler_run_id': USize, closure: {((T | None)): (T^ | None) }) =>
    """
    Execute the given closure on the current state of the encapsulated thing,
    which might be `None`.
    """
    if compiler_run_id' >= this.compiler_run_id then
      this.compiler_run_id = compiler_run_id'
      this._thing = closure.apply(this._thing)
    end

  fun get(current_run_id: USize): (this->T | None) =>
    if current_run_id == compiler_run_id then
      this._thing
    else
      None
    end

  fun apply(): this->T ? =>
    this._thing as this->T
