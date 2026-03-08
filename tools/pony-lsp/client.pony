use "json"

class val Client
  """
  Represents the LSP client this server is connected to
  with its capabilities
  """
  let process_id: (I64 | None)
  let client_name: (String val | None)
  let client_version: (String val | None)
  let capabilities: JsonObject

  // extracted from client capabilities
  // if we need more information about the client, add here and extract in the
  // constructor
  let _supports_configuration: Bool
  let _supports_configuration_dynamic_registration: Bool
  let _supports_publish_diagnostics: Bool
  let _supports_publish_diagnostic_related_info: Bool
  let _supports_workspace_diagnostic_refresh: Bool
  let _supports_window_work_done_progress: Bool

  new val from(initialize_params: JsonObject) =>
    this.process_id =
      try
        JsonPathParser.compile("$.processId")?.query_one(initialize_params) as I64

      else
        None
      end
    this.client_name =
      try
        JsonPathParser.compile("$.clientInfo.name")?.query_one(initialize_params) as String
      else
        None
      end
    this.client_version =
      try
        JsonPathParser.compile("$.clientInfo.version")?.query_one(initialize_params) as String
      else
        None
      end
    this.capabilities =
      try
        JsonPathParser.compile("$.capabilities")?.query_one(initialize_params) as JsonObject
      else
        JsonObject
      end
    this._supports_configuration =
      try
        JsonPathParser.compile("$.workspace.configuration")?.query_one(this.capabilities) as Bool
      else
        false
      end
    this._supports_configuration_dynamic_registration =
      try
        JsonPathParser.compile("$.workspace.didChangeConfiguration.dynamicRegistration")?.query_one(this.capabilities) as Bool
      else
        false
      end
    this._supports_publish_diagnostics =
      try
        JsonPathParser.compile("$.textDocument.publishDiagnostics")?.query_one(this.capabilities) as JsonObject
        true
      else
        false
      end
    this._supports_publish_diagnostic_related_info =
      try
        JsonPathParser.compile("$.textDocument.publishDiagnostics.relatedInformation")?.query_one(this.capabilities) as Bool
      else
        false
      end
    this._supports_workspace_diagnostic_refresh =
      try
        JsonPathParser.compile("$.workspace.diagnostics.refreshSupport")?.query_one(this.capabilities) as Bool
      else
        false
      end
    this._supports_window_work_done_progress =
      try
        JsonPathParser.compile("$.window.workDoneProgress")?.query_one(this.capabilities) as Bool
      else
        false
      end

  fun supports_configuration(): Bool =>
    """
    Returns `true` if the client supports the workspace/configuration request
    """
    this._supports_configuration

  fun supports_configuration_dynamic_registration(): Bool =>
    """
    Returns `true` if the client supports registering the capability
    `workspace/didChangeConfiguration`.
    """
    this._supports_configuration_dynamic_registration

  fun supports_publish_diagnostics(): Bool =>
    """
    Checks that the client supports publishing diagnostics per text-document

    by checking for presence of [PublishDiagnosticsClientCapabilities](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#publishDiagnosticsClientCapabilities)
    """
    this._supports_publish_diagnostics

  fun supports_publish_diagnostic_related_info(): Bool =>
    """
    checks for `relatedInformation` in [PublishDiagnosticsClientCapabilities](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#publishDiagnosticsClientCapabilities)

    Returns `true` if the client supports `relatedInformation` on `Diagnostic`
    items. `false` denotes the client doesn't support or we don't know.
    """
    this._supports_publish_diagnostic_related_info

  fun supports_workspace_diagnostic_refresh(): Bool =>
    """
    Checks for the presence of `refreshSupport` in [DiagnosticWorkspaceClientCapabilities](https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnosticWorkspaceClientCapabilities).
    """
    this._supports_workspace_diagnostic_refresh

  fun supports_window_work_done_progress(): Bool =>
    """
    Returns `true` if the client supports server-initiated `window/workDoneProgress/create`.
    Checks for `window.workDoneProgress` to be `true`.
    """
    this._supports_window_work_done_progress

  fun string(): String iso^ =>
    recover iso
      let s = String.create()
        .>append(try (this.client_name as String val) + "-" else "" end)
        .>append(try (this.client_version as String val) + " " else "" end)
        .>append(try "(" + (this.process_id as I64).string() + ")" else "" end)
        .>append("\r\nsupports:\r\n")
      if this.supports_configuration() then
        s.append("\tconfiguration\r\n")
      end
      if this.supports_configuration_dynamic_registration() then
        s.append("\tdynamic registration for didChangeConfiguration notifications\r\n")
      end
      if this.supports_publish_diagnostics() then
        s.append("\tpublish diagnostics\r\n")
      end
      if this.supports_publish_diagnostic_related_info() then
        s.append("\trelated information in diagnostics\r\n")
      end
      if this.supports_workspace_diagnostic_refresh() then
        s.append("\trefreshing diagnostics\r\n")
      end
      if this.supports_window_work_done_progress() then
        s.append("\tserver-initiated workDoneProgress\r\n")
      end
      consume s
    end
