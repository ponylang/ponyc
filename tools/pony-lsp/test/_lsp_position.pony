class val _LspPosition
  """
  A position within a workspace file, identified by filename, line, and
  character offset. Encapsulates the action string used by pony_test to
  track individual LSP request/response pairs.
  """
  let workspace_file: String val
  let line: I64
  let character: I64

  new val create(
    workspace_file': String val,
    line': I64,
    character': I64)
  =>
    workspace_file = workspace_file'
    line = line'
    character = character'

  fun action(): String val =>
    workspace_file + ":" + line.string() + ":" + character.string()
