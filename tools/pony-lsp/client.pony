use "json"

class val Client
  """
  Represents the LSP client this server is connected to
  with its capabilities.
  """
  let process_id: (I64 | None)
  let client_name: (String val | None)
  let client_version: (String val | None)
  let capabilities: JSONObject
  let _supports_configuration: Bool
  let _supports_configuration_dynamic_registration: Bool
  let _supports_publish_diagnostics: Bool
  let _supports_publish_diagnostic_related_info: Bool
  let _supports_workspace_diagnostic_refresh: Bool
  let _supports_inlay_hint_refresh: Bool
  let _supports_folding_range_refresh: Bool
  let _supports_window_work_done_progress: Bool

  new val from(initialize_params: JSONObject) =>
    """
    Create a Client from initialize request parameters.
    """
    this.process_id =
      try
        JSONPathParser.compile("$.processId")?
          .query_one(initialize_params) as I64
      else
        None
      end
    this.client_name =
      try
        JSONPathParser.compile("$.clientInfo.name")?
          .query_one(initialize_params) as String
      else
        None
      end
    this.client_version =
      try
        JSONPathParser.compile("$.clientInfo.version")?
          .query_one(initialize_params) as String
      else
        None
      end
    this.capabilities =
      try
        JSONPathParser.compile("$.capabilities")?
          .query_one(initialize_params) as JSONObject
      else
        JSONObject
      end
    this._supports_configuration =
      try
        JSONPathParser
          .compile("$.workspace.configuration")?
          .query_one(this.capabilities) as Bool
      else
        false
      end
    this._supports_configuration_dynamic_registration =
      try
        JSONPathParser.compile(
          "$.workspace.didChangeConfiguration.dynamicRegistration"
        )?.query_one(this.capabilities) as Bool
      else
        false
      end
    this._supports_publish_diagnostics =
      try
        JSONPathParser.compile(
          "$.textDocument.publishDiagnostics"
        )?.query_one(this.capabilities) as JSONObject
        true
      else
        false
      end
    this._supports_publish_diagnostic_related_info =
      try
        JSONPathParser.compile(
          "$.textDocument.publishDiagnostics.relatedInformation"
        )?.query_one(this.capabilities) as Bool
      else
        false
      end
    this._supports_workspace_diagnostic_refresh =
      try
        JSONPathParser.compile(
          "$.workspace.diagnostics.refreshSupport"
        )?.query_one(this.capabilities) as Bool
      else
        false
      end
    this._supports_inlay_hint_refresh =
      try
        JSONPathParser.compile(
          "$.workspace.inlayHint.refreshSupport"
        )?.query_one(this.capabilities) as Bool
      else
        false
      end
    this._supports_folding_range_refresh =
      try
        JSONPathParser.compile(
          "$.workspace.foldingRange.refreshSupport"
        )?.query_one(this.capabilities) as Bool
      else
        false
      end
    this._supports_window_work_done_progress =
      try
        JSONPathParser.compile(
          "$.window.workDoneProgress"
        )?.query_one(this.capabilities) as Bool
      else
        false
      end

  fun supports_configuration(): Bool =>
    """
    Returns `true` if the client supports the workspace/configuration request.
    """
    this._supports_configuration

  fun supports_configuration_dynamic_registration(): Bool =>
    """
    Returns `true` if the client supports registering
    the capability `workspace/didChangeConfiguration`.
    """
    this._supports_configuration_dynamic_registration

  fun supports_publish_diagnostics(): Bool =>
    """
    Checks that the client supports publishing diagnostics per text-document.
    """
    this._supports_publish_diagnostics

  fun supports_publish_diagnostic_related_info(): Bool =>
    """
    Returns `true` if the client supports `relatedInformation` on `Diagnostic`
    items. `false` denotes the client doesn't support or we don't know.
    """
    this._supports_publish_diagnostic_related_info

  fun supports_workspace_diagnostic_refresh(): Bool =>
    """
    Checks for the presence of `refreshSupport` in
    DiagnosticWorkspaceClientCapabilities.
    """
    this._supports_workspace_diagnostic_refresh

  fun supports_inlay_hint_refresh(): Bool =>
    """
    Returns `true` if the client supports the `workspace/inlayHint/refresh`
    request (InlayHintWorkspaceClientCapabilities.refreshSupport).
    """
    this._supports_inlay_hint_refresh

  fun supports_folding_range_refresh(): Bool =>
    """
    Returns `true` if the client supports the
    `workspace/foldingRange/refresh` request
    (FoldingRangeWorkspaceClientCapabilities.refreshSupport).
    """
    this._supports_folding_range_refresh

  fun supports_window_work_done_progress(): Bool =>
    """
    Returns `true` if the client supports
    server-initiated `window/workDoneProgress/create`.
    """
    this._supports_window_work_done_progress

  fun string(): String iso^ =>
    recover iso
      let s = String.create()
        .> append(
          try
            (this.client_name as String val) + "-"
          else
            ""
          end)
        .> append(
          try
            (this.client_version as String val) + " "
          else
            ""
          end)
        .> append(
          try
            "(" + (this.process_id as I64).string() + ")"
          else
            ""
          end)
        .> append("\r\nsupports:\r\n")
      if this.supports_configuration() then
        s.append("\tconfiguration\r\n")
      end
      if this.supports_configuration_dynamic_registration() then
        s.append(
          "\tdynamic registration for " +
          "didChangeConfiguration notifications\r\n"
        )
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
      if this.supports_inlay_hint_refresh() then
        s.append("\trefreshing inlay hints\r\n")
      end
      if this.supports_folding_range_refresh() then
        s.append("\trefreshing folding ranges\r\n")
      end
      if this.supports_window_work_done_progress() then
        s.append("\tserver-initiated workDoneProgress\r\n")
      end
      consume s
    end
