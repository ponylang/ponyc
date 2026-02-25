use "json"
use "pony_compiler"

class ref Diagnostic
    """
    See: https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#diagnostic
    """
    let range: LspPositionRange
    let severity: I64
    // left out intentionally
    // let code: (I64 | String | None)
    // let code_description: (CodeDescription | None)
    let message: String
    let related_information: Array[DiagnosticRelatedInformation]

    new ref create(range': LspPositionRange, severity': I64, message': String) =>
        range = range'
        severity = severity'
        message = message'
        related_information = Array[DiagnosticRelatedInformation].create(0)

    new val from_error(err: Error) =>
        this.range = LspPositionRange.from_single_pos(LspPosition.from_ast_pos(err.position))
        this.severity = DiagnosticSeverities.err()
        this.message = err.msg
        this.related_information = Array[DiagnosticRelatedInformation].create(err.infos.size())
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
            let location = LspLocation.create(
                file,
                LspPositionRange.from_single_pos(
                    LspPosition.from_ast_pos(info.position)
                )
            )
            this.related_information.push(
                DiagnosticRelatedInformation.create(location, info.msg)
            )
        end

    fun to_json(): JsonValue =>
        var obj = JsonObject
          .update("range", this.range.to_json())
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
    fun tag information(): I64 => 2
    fun tag hint(): I64 => 2

class val DiagnosticRelatedInformation
    let location: LspLocation
    let message: String

    new create(location': LspLocation, message': String) =>
        location = location'
        message = message'

    fun to_json(): JsonValue =>
        JsonObject
          .update("location", this.location.to_json())
          .update("message", this.message)