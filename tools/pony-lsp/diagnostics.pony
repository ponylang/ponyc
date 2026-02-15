use "json"
use "pony_compiler"

class val Diagnostic
    """
    See: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnostic
    """
    // only the range is serialized, but useful to keep the actual file around for expanding related inormation
    let location: LspLocation
    let severity: I64
    // left out intentionally
    // let code: (I64 | String | None)
    // let code_description: (CodeDescription | None)
    let message: String val
    let related_information: Array[DiagnosticRelatedInformation] val

    new val create(location': LspLocation, severity': I64, message': String, related_information': Array[DiagnosticRelatedInformation] val = []) =>
        location = location'
        severity = severity'
        message = message'
        related_information = related_information'

    new val from_error(err: Error) =>
        this.location = LspLocation(
          Uris.from_path(
            try
              err.file as String
            else
              ""
            end),
            LspPositionRange.from_single_pos(LspPosition.from_ast_pos(err.position))
        )
        this.severity = DiagnosticSeverities.err()
        this.message = err.msg
        let relinfos = recover trn Array[DiagnosticRelatedInformation].create(err.infos.size()) end
        for info in err.infos.values() do
            let file_path =
                try
                    info.file as String
                else
                    try
                        err.file as String
                    else
                        continue
                    end
                end
            let file = Uris.from_path(file_path)
            let rel_loc = LspLocation.create(
                file,
                LspPositionRange.from_single_pos(
                    LspPosition.from_ast_pos(info.position)
                )
            )
            relinfos.push(
                DiagnosticRelatedInformation.create(rel_loc, info.msg)
            )
        end
        this.related_information = consume relinfos

    fun val file_uri(): String val =>
      this.location.uri

    fun val expand_related(): Array[Diagnostic] val =>
      """
      Turn all related informations in to their own Diagnostic items
      pointing at the same place as the root diagnostic.

      This is necessary for some clients (e.g. neovim), as they otherwise
      wouldn't show related information
      """
      let expanded = recover trn Array[Diagnostic].create(this.related_information.size() + 1) end
      expanded.push(this)
      for related_info in this.related_information.values() do
        expanded.push(
          Diagnostic(
            related_info.location,
            DiagnosticSeverities.hint(),
            related_info.message,
            [
              DiagnosticRelatedInformation(this.location, "original diagnostic")
            ]
          )
        )
      end
      consume expanded

    fun to_json(): JsonValue =>
        var obj = JsonObject
          .update("range", this.location.range.to_json())
          .update("severity", this.severity)
          .update("source", "pony-lsp")
          .update("message", this.message)
        if this.related_information.size() > 0 then
            var arr = JsonArray
            for rel_info in this.related_information.values() do
                arr = arr.push(rel_info.to_json())
            end
            obj = obj.update("relatedInformation", arr)
        end
        obj



primitive DiagnosticSeverities
    fun tag err(): I64 => 1
    fun tag warning(): I64 => 2
    fun tag information(): I64 => 3
    fun tag hint(): I64 => 4

class val DiagnosticRelatedInformation
    let location: LspLocation
    let message: String

    new val create(location': LspLocation, message': String) =>
        location = location'
        message = message'

    fun to_json(): JsonValue =>
        JsonObject
          .update("location", this.location.to_json())
          .update("message", this.message)
