type L is Literal

class Literal is Parser
  let _text: String

  new val create(from: String) =>
    _text = from

  fun parse(source: Source, offset: USize, tree: Bool, hidden: Parser)
    : ParseResult
  =>
    let from = skip_hidden(source, offset, hidden)

    if source.content.at(_text, from.isize()) then
      result(source, offset, from, _text.size(), tree)
    else
      (from - offset, this)
    end

  fun error_msg(): String => _text
