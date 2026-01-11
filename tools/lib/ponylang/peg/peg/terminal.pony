class Terminal is Parser
  """
  A terminal parser accumulates a single lexical token rather than an AST. It
  applies the hidden channel once before starting, but not while parsing its
  elements.
  """
  let _a: Parser
  let _label: Label

  new create(a: Parser, l: Label = NoLabel) =>
    _a = a
    _label = l

  fun parse(source: Source, offset: USize, tree: Bool, hidden: Parser)
    : ParseResult
  =>
    let from = skip_hidden(source, offset, hidden)

    match _a.parse(source, from, false, NoParser)
    | (let length: USize, Lex) =>
      result(source, offset, from, length, tree, _label)
    | (let advance: USize, let r: Parser) => (advance, r)
    else
      (0, this)
    end

  fun error_msg(): String => _a.error_msg()
