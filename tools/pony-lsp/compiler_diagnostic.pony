use "pony_compiler"

primitive CompilerDiagnostic
  """
  Converts compiler errors with source files to LSP diagnostics.
  """
  fun apply(err: Error): (Diagnostic | None) =>
    """
    Returns None when the error has no source file. The caller must report
    that error separately from document diagnostics.
    """
    let file_path =
      match err.file
      | let path: String if path.size() > 0 => path
      else
        return None
      end
    let related =
      recover trn
        Array[DiagnosticRelatedInformation](err.infos.size())
      end
    for info in err.infos.values() do
      let info_path =
        match info.file
        | let path: String if path.size() > 0 => path
        else
          file_path
        end
      related.push(
        DiagnosticRelatedInformation(
          LspLocation(
            Uris.from_path(info_path),
            LspPositionRange.from_single_pos(
              LspPosition.from_ast_pos(info.position))),
          info.msg))
    end
    Diagnostic(
      LspLocation(
        Uris.from_path(file_path),
        LspPositionRange.from_single_pos(
          LspPosition.from_ast_pos(err.position))),
      DiagnosticSeverities.err(),
      err.msg,
      consume related)
