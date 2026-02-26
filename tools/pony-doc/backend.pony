use "files"

trait Backend
  """Generates documentation output from a `DocProgram` IR."""
  fun generate(
    program: DocProgram box,
    output_dir: FilePath,
    include_private: Bool)
    ?
