class Option is Parser
  let _a: Parser

  new create(a: Parser) =>
    _a = a

  fun parse(source: Source, offset: USize, tree: Bool, hidden: Parser)
    : ParseResult
  =>
    match _a.parse(source, offset, tree, hidden)
    | (let advance: USize, let r: ParseOK) => (advance, r)
    else
      (0, NotPresent)
    end
