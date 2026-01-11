class EndOfFile is Parser
  let _a: Parser

  new create(a: Parser) =>
    _a = a

  fun parse(source: Source, offset: USize, tree: Bool, hidden: Parser)
    : ParseResult
  =>
    match _a.parse(source, offset, tree, hidden)
    | (let advance: USize, let r: ParseOK) =>
      let from = skip_hidden(source, offset + advance, hidden)
      if from == source.content.size() then
        (from - offset, r)
      else
        (from - offset, this)
      end
    | (let advance: USize, let r: Parser) => (advance, r)
    else
      (0, this)
    end

  fun error_msg(): String => "end-of-file"
