class Forward is Parser
  """
  A forwarding parser is used to create mutually recursive parse rules. The
  forwarding parser can be used instead, and is updated when the real parse
  rule is created.
  """
  var _a: Parser = NoParser

  new create() =>
    None

  fun ref update(value: Parser) =>
    _a = value

  fun parse(source: Source, offset: USize, tree: Bool, hidden: Parser)
    : ParseResult
  =>
    if _a isnt NoParser then
      _a.parse(source, offset, tree, hidden)
    else
      (0, this)
    end

  fun complete(): Bool => _a isnt NoParser

  fun error_msg(): String => _a.error_msg()
