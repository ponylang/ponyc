use "json"

class val Diagnostic
  """
  LSP diagnostic representing a compiler error or
  warning at a specific location.
  """
  // only the range is serialised, but useful to keep
  // the actual file around for expanding related info
  let location: LspLocation
  let severity: I64
  // left out intentionally
  // let code: (I64 | String | None)
  // let code_description: (CodeDescription | None)
  let message: String val
  let related_information:
    Array[DiagnosticRelatedInformation] val

  new val create(
    location': LspLocation,
    severity': I64,
    message': String,
    related_information': Array[DiagnosticRelatedInformation] val = [])
  =>
    location = location'
    severity = severity'
    message = message'
    related_information = related_information'

  fun val file_uri(): String val =>
    this.location.uri

  fun val expand_related(): Array[Diagnostic] val =>
    """
    Turn each piece of related information into its own
    Diagnostic item pointing at the same place as the
    root diagnostic.

    This is necessary for some clients (e.g. neovim), as
    they otherwise wouldn't show related information.
    """
    let expanded =
      recover trn
        Array[Diagnostic].create(this.related_information.size() + 1)
      end
    expanded.push(this)
    for related_info in this.related_information.values() do
      expanded.push(
        Diagnostic(
          related_info.location,
          DiagnosticSeverities.hint(),
          related_info.message,
          [DiagnosticRelatedInformation(this.location, "original diagnostic")]
        )
      )
    end
    consume expanded

  fun to_json(): JSONValue =>
    var obj = JSONObject
      .update("range", this.location.range.to_json())
      .update("severity", this.severity)
      .update("source", "pony-lsp")
      .update("message", this.message)
    if this.related_information.size() > 0 then
      var arr = JSONArray
      for rel_info in this.related_information.values() do
        arr = arr.push(rel_info.to_json())
      end
      obj = obj.update("relatedInformation", arr)
    end
    obj

primitive DiagnosticSeverities
  """
  LSP diagnostic severity constants.
  """
  fun err(): I64 => 1
  fun warning(): I64 => 2
  fun information(): I64 => 3
  fun hint(): I64 => 4

class val DiagnosticRelatedInformation
  """
  Related information for a diagnostic, pointing to
  another location in the source.
  """
  let location: LspLocation
  let message: String

  new val create(location': LspLocation, message': String) =>
    location = location'
    message = message'

  fun to_json(): JSONValue =>
    JSONObject
      .update("location", this.location.to_json())
      .update("message", this.message)
