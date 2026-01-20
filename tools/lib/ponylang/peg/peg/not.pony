class Not is Parser
  """
  If the parse succeeds, then fail. Otherwise, return a zero length Skipped.
  """
  let _a: Parser

  new create(a: Parser) =>
    _a = a

  fun parse(source: Source, offset: USize, tree: Bool, hidden: Parser)
    : ParseResult
  =>
    match _a.parse(source, offset, tree, hidden)
    | (let advance: USize, let r: ParseOK) => (advance, this)
    else
      (0, Skipped)
    end

  fun error_msg(): String => "not to find " + _a.error_msg()
