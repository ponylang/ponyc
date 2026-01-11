type ParseResult is (USize, (ParseOK | Parser))

type ParseOK is
  ( AST
  | Token
  | NotPresent
  | Skipped
  | Lex
  )

primitive NotPresent
  """
  Returned by Option when the parse isn't found
  """
  fun label(): Label => NoLabel

primitive Skipped
  """
  Returned by Skip when the parse is found, and Not when the parse isn't found.
  """

primitive Lex
  """
  Returned when a parse tree isn't neeeded
  """

trait box Parser
  fun parse(source: Source, offset: USize = 0, tree: Bool = true,
    hidden: Parser = NoParser): ParseResult

  fun error_msg(): String => "not to see an error in this parser"

  fun skip_hidden(source: Source, offset: USize, hidden: Parser): USize =>
    """
    Return a new start location, skipping over hidden tokens.
    """
    offset + hidden.parse(source, offset, false, NoParser)._1

  fun result(source: Source, offset: USize, from: USize, length: USize,
    tree: Bool, l: Label = NoLabel): ParseResult
  =>
    """
    Once a terminal parser has an offset and length, it should call `result` to
    return either a token (if a tree is requested) or a new lexical position.
    """
    if tree then
      ((from - offset) + length, Token(l, source, from, length))
    else
      ((from - offset) + length, Lex)
    end

  fun mul(that: Parser): Sequence => Sequence(this, that)
  fun div(that: Parser): Choice => Choice(this, that)
  fun neg(): Skip => Skip(this)
  fun opt(): Option => Option(this)
  fun many(sep: Parser = NoParser): Many => Many(this, sep, false)
  fun many1(sep: Parser = NoParser): Many => Many(this, sep, true)
  fun op_not(): Not => Not(this)
  fun op_and(): Not => Not(Not(this))
  fun hide(that: Parser): Hidden => Hidden(this, that)
  fun term(l: Label = NoLabel): Terminal => Terminal(this, l)
  fun eof(): EndOfFile => EndOfFile(this)

primitive NoParser is Parser
  fun parse(source: Source, offset: USize, tree: Bool, hidden: Parser)
    : ParseResult
  =>
    (0, Lex)

  fun error_msg(): String => "not to be using NoParser"

trait val Label
  fun text(): String

primitive NoLabel is Label fun text(): String => ""
