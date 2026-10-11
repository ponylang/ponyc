class val DocSourceFile
  """
  A source file included in the documentation output.
  """
  let module_name: String
  let filename: String
  let content: String

  new val create(
    module_name': String,
    filename': String,
    content': String)
  =>
    module_name = module_name'
    filename = filename'
    content = content'

class val DocSourceLocation
  """
  The source location of a documented element.
  """
  let file_path: String
  let line: USize

  new val create(file_path': String, line': USize) =>
    file_path = file_path'
    line = line'
