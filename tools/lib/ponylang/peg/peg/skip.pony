class Skip is Parser
  let _a: Parser

  new create(a: Parser) =>
    _a = a

  fun parse(source: Source, offset: USize, tree: Bool, hidden: Parser)
    : ParseResult
  =>
    match _a.parse(source, offset, false, hidden)
    | (let advance: USize, let r: ParseOK) => (advance, Skipped)
    | (let advance: USize, let r: Parser) => (advance, r)
    else
      (0, this)
    end

  fun error_msg(): String => _a.error_msg()
