use "files"

class val SourceFile
  """
  A source file loaded into memory with its content pre-split into lines.

  Lines are split on `\n` after stripping `\r` characters, so both Unix and
  Windows line endings are handled. Line indices are 0-based in the `lines`
  array but 1-based in diagnostics.
  """
  let path: String val
  let rel_path: String val
  let content: String val
  let lines: Array[String val] val

  new val create(
    path': String val,
    content': String val,
    cwd: String val)
  =>
    path = path'
    content = content'
    rel_path =
      try
        Path.rel(cwd, path')?
      else
        path'
      end
    lines = _split_lines(content')

  fun tag _split_lines(text: String val): Array[String val] val =>
    """
    Split text into lines, stripping carriage returns. An empty string
    produces a single-element array containing an empty string, matching
    the convention that a file always has at least one line.
    """
    let stripped: String val =
      if text.contains("\r") then
        let s = text.clone()
        s.remove("\r")
        consume s
      else
        text
      end
    let parts = stripped.split_by("\n")
    // split_by on "a\n" produces ["a", ""] which is correct —
    // a trailing newline means there's a line after it (empty).
    // An empty string produces [""] which is also correct.
    recover val
      let result = Array[String val](parts.size())
      for part in (consume parts).values() do
        result.push(part)
      end
      result
    end
