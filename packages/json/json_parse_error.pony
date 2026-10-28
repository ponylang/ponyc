class val JSONParseError is Stringable
  """
  Structured parse error with location information.
  """

  let message: String
  let offset: USize
  let line: USize

  new val create(message': String, offset': USize, line': USize) =>
    message = message'
    offset = offset'
    line = line'

  fun string(): String =>
    recover val
      String(64)
        .> append("JSON parse error at line ")
        .> append(line.string())
        .> append(", byte ")
        .> append(offset.string())
        .> append(": ")
        .> append(message)
    end
