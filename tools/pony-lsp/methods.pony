primitive Methods
  """
  Collection of methods for LSP requests, notifications, responses
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

  fun shutdown(): String val =>
    "shutdown"

  fun client(): ClientMethods =>
    """
    Access client scoped methods
    """
    ClientMethods

  fun text_document(): TextDocumentMethods =>
    """
    Access testDocument scoped methods
    """
    TextDocumentMethods

  fun window(): WindowMethods =>
    """
    Access window scoped methods
    """
    WindowMethods

  fun workspace(): WorkspaceMethods =>
    """
    Access workspace scoped methods
    """
    WorkspaceMethods

  
primitive WorkspaceMethods
  """
  Collection of workspace scoped methods
  """
  fun configuration(): String val =>
    "workspace/configuration"

  fun did_change_configuration(): String val =>
    "workspace/didChangeConfiguration"

  fun diagnostic(): WorkspaceDiagnosticMethods =>
    WorkspaceDiagnosticMethods

primitive WorkspaceDiagnosticMethods
  """
  Collection of workspace/diagnostic related methods
  """
  fun refresh(): String val =>
    "workspace/diagnostic/refresh"

  
primitive TextDocumentMethods
  """
  Collection of textDocument scoped methods
  """
  fun definition(): String val =>
    "textDocument/definition"

  fun diagnostic(): String val =>
    "textDocument/diagnostic"

  fun did_close(): String val =>
    "textDocument/didClose"

  fun did_open(): String val =>
    "textDocument/didOpen"

  fun did_save(): String val =>
    "textDocument/didSave"

  fun document_symbol(): String val =>
    "textDocument/documentSymbol"

  fun hover(): String val =>
    "textDocument/hover"

  fun publish_diagnostics(): String val =>
    "textDocument/publishDiagnostics"


primitive ClientMethods
  """
  Collection of client scoped methods
  """
  fun register_capability(): String val =>
    "client/registerCapability"

primitive WindowMethods
  """
  Collection of window scoped methods
  """
  fun log_message(): String val =>
    "window/logMessage"
