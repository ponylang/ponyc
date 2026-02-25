class val DocSourceFile
  """A source file included in the documentation output."""
  let package_name: String
  let filename: String
  let content: String

  new val create(
    package_name': String,
    filename': String,
    content': String)
  =>
    package_name = package_name'
    filename = filename'
    content = content'

class val DocSourceLocation
  """The source location of a documented element."""
  let file_path: String
  let line: USize

  new val create(file_path': String, line': USize) =>
    file_path = file_path'
    line = line'
