use "files"

class val Source
  let path: String
  let content: String

  new val create(filepath: FilePath) ? =>
    path = filepath.path
    let file = OpenFile(filepath) as File
    content = file.read_string(file.size())
    file.dispose()

  new val from_string(content': String, path': String = "") =>
    (path, content) = (path', content')
