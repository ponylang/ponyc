primitive Methods
  """
  Collection of methods for LSP requests, notifications, responses.
  """

  fun exit(): String val =>
    "exit"

  fun initialize(): String val =>
    "initialize"

  fun initialized(): String val =>
    "initialized"

  fun log_trace(): String val =>
    "$/logTrace"

  fun set_trace(): String val =>
    "$/setTrace"

  fun progress(): String val =>
    "$/progress"

  fun shutdown(): String val =>
    "shutdown"

  fun client(): ClientMethods =>
    """
    Access client scoped methods.
    """
    ClientMethods

  fun text_document(): TextDocumentMethods =>
    """
    Access testDocument scoped methods.
    """
    TextDocumentMethods

  fun window(): WindowMethods =>
    """
    Access window scoped methods.
    """
    WindowMethods

  fun workspace(): WorkspaceMethods =>
    """
    Access workspace scoped methods.
    """
    WorkspaceMethods

primitive WorkspaceMethods
  """
  Collection of workspace scoped methods.
  """

  fun configuration(): String val =>
    "workspace/configuration"

  fun did_change_configuration(): String val =>
    "workspace/didChangeConfiguration"

  fun diagnostic(): WorkspaceDiagnosticMethods =>
    WorkspaceDiagnosticMethods

  fun folding_range(): WorkspaceFoldingRangeMethods =>
    WorkspaceFoldingRangeMethods

  fun inlay_hint(): WorkspaceInlayHintMethods =>
    WorkspaceInlayHintMethods

  fun symbol(): String val =>
    "workspace/symbol"

primitive WorkspaceFoldingRangeMethods
  """
  Collection of workspace/foldingRange related methods.
  """

  fun refresh(): String val =>
    "workspace/foldingRange/refresh"

primitive WorkspaceInlayHintMethods
  """
  Collection of workspace/inlayHint related methods.
  """

  fun refresh(): String val =>
    "workspace/inlayHint/refresh"

primitive WorkspaceDiagnosticMethods
  """
  Collection of workspace/diagnostic related methods.
  """

  fun refresh(): String val =>
    "workspace/diagnostic/refresh"

primitive TextDocumentMethods
  """
  Collection of textDocument scoped methods.
  """

  fun declaration(): String val =>
    "textDocument/declaration"

  fun definition(): String val =>
    "textDocument/definition"

  fun type_definition(): String val =>
    "textDocument/typeDefinition"

  fun diagnostic(): String val =>
    "textDocument/diagnostic"

  fun did_close(): String val =>
    "textDocument/didClose"

  fun did_open(): String val =>
    "textDocument/didOpen"

  fun did_save(): String val =>
    "textDocument/didSave"

  fun document_highlight(): String val =>
    "textDocument/documentHighlight"

  fun document_symbol(): String val =>
    "textDocument/documentSymbol"

  fun folding_range(): String val =>
    "textDocument/foldingRange"

  fun selection_range(): String val =>
    "textDocument/selectionRange"

  fun hover(): String val =>
    "textDocument/hover"

  fun inlay_hint(): String val =>
    "textDocument/inlayHint"

  fun references(): String val =>
    "textDocument/references"

  fun prepare_rename(): String val =>
    "textDocument/prepareRename"

  fun rename(): String val =>
    "textDocument/rename"

  fun publish_diagnostics(): String val =>
    "textDocument/publishDiagnostics"

primitive ClientMethods
  """
  Collection of client scoped methods.
  """

  fun register_capability(): String val =>
    "client/registerCapability"

primitive WindowMethods
  """
  Collection of window scoped methods.
  """

  fun log_message(): String val =>
    "window/logMessage"

  fun work_done_progress(): WorkDoneProgressMethods =>
    WorkDoneProgressMethods.create_progress()

primitive WorkDoneProgressMethods
  """
  Collection of window/workDoneProgress methods.
  """

  new create_progress() =>
    """
    create method below collides with default constructor.
    """
    None

  fun create(): String val =>
    "window/workDoneProgress/create"

  fun cancel(): String val =>
    "window/workDoneProgress/cancel"
