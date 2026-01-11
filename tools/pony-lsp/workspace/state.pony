use "collections"
use "files"
use "itertools"

use "ast"

use ".."

class PackageState
  let path: FilePath
  let documents: Map[String, DocumentState]
  let _channel: Channel

  var package: (Package | None)
  var compiler_run_id: USize

  new create(path': FilePath, channel: Channel) =>
    path = path'
    _channel = channel
    documents = documents.create()
    package = None
    compiler_run_id = 0

  fun debug(): String val =>
    "Package " + this.path.path + " (" + (
      try
        (this.package as Package).qualified_name
      else
        ""
      end
    ) + "):\n\t" + "\n\t".join(
      Iter[(String box, DocumentState box)](documents.pairs())
        .map[String]({(kv) =>
          kv._1 + " (" +
            match kv._2.module
            | let m: Module => "M"
            else
              "_"
            end + ")"
        })
    )

  fun get_document(document_path: String): (this->DocumentState | None) =>
    try
      this.documents(document_path)?
    end

  fun has_document(document_path: String): Bool =>
    this.documents.contains(document_path)

  fun ref insert_new(document_path: String): (DocumentState, Bool) =>
    """
    Insert a new module by the given `document_path` into this package
    """
    var has_module = false
    let doc_state = DocumentState.create(document_path, this._channel)

    try
      // populate the document from the last compilation result if available
      let pkg = this.package as Package
      let module = pkg.find_module(document_path) as Module
      doc_state.update(module, this.compiler_run_id)
      has_module = true
    end
    (
      this.documents.insert(
        document_path,
        doc_state
      ),
      has_module
    )

  fun ref update(result: Package val, run_id: USize) =>
    """
    Update the current package state with the result of the compilation run identified by `run_id`.
    """
    this.compiler_run_id = run_id
    this.package = result
    this._channel.log("Updating package " + result.path)
    this._channel.log(this.debug())
    // for each open document, update the document state if we have a module for it
    for (doc_path, doc_state) in this.documents.pairs() do
      // TODO: ensure both module and package-state paths are normalized
      match result.find_module(doc_path)
      | let m: Module val =>
        // update each document state
        doc_state.update(m, run_id)
      | None =>
        this._channel.log("No module found for " + doc_path)
      end
    end

  fun dispose() =>
    for doc_state in this.documents.values() do
      doc_state.dispose()
    end

class DocumentState
  let path: String
  let _channel: Channel

  var module: (Module val | None)
  var position_index: (PositionIndex val | None)
  var _document_symbols: (Array[DocumentSymbol] | None)
  var hash: USize
  var compiler_run_id: USize

  new create(path': String, channel': Channel) =>
    path = path'
    _channel = channel'
    module = None
    position_index = None
    _document_symbols = None
    hash = 0
    compiler_run_id = 0

  fun ref update(module': Module val, run_id: USize) =>
    this.compiler_run_id = compiler_run_id
    this.module = module'
    this.position_index = module'.create_position_index()
    // only update if it was requested already
    if this._document_symbols isnt None then
      this._document_symbols = DocumentSymbols.from_module(module', this._channel)

    end
    this.hash = module'.hash()

  fun ref document_symbols(): Array[DocumentSymbol] =>
    """
    Get or create the current document symbols
    """
    match this.module
    | let mod: Module val =>
      let created =  DocumentSymbols.from_module(mod, this._channel)
      this._document_symbols = created
      created
    else
      // no module available yet, no documentsymbols available yet
      this._channel.log("No module available yet for " + this.path)
      []
    end


  fun dispose() =>
    None
