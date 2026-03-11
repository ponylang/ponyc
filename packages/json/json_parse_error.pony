class val JsonParseError is Stringable
  """Structured parse error with location information."""

  let message: String
  let offset: USize
  let line: USize

  new val create(message': String, offset': USize, line': USize) =>
    message = message'
    offset = offset'
    line = line'

  fun string(): String iso^ =>
    let s = recover iso String(64) end
    s.append("JSON parse error at line ")
    s.append(line.string())
    s.append(", byte ")
    s.append(offset.string())
    s.append(": ")
    s.append(message)
    consume s
