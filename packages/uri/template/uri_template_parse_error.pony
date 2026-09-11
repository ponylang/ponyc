class val URITemplateParseError
  """
  Describes why a URI template string failed to parse.

  Contains a human-readable error message and the byte offset in the
  template string where the error was detected.
  """
  let message: String val
  let offset: USize

  new val create(message': String val, offset': USize) =>
    """
    Create a parse error with the given message and byte offset.
    """
    message = message'
    offset = offset'

  fun string(): String iso^ =>
    """
    Format as "offset N: message".
    """
    let num: String val = offset.string()
    let out =
      recover iso
        String(7 + num.size() + 2 + message.size())
      end
    out .> append("offset ")
      .> append(num)
      .> append(": ")
      .> append(message)
    consume out
