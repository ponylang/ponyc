class Choice is Parser
  """
  Given a sequence of parser rules, return the result of the first rule that
  matches. Note that the result is deterministic: if more than one of the rules
  could match, the first in the list will be chosen.
  """
  let _seq: Array[Parser]

  new create(a: Parser, b: Parser) =>
    _seq = [a; b]

  new concat(a: Choice box, b: Parser) =>
    let r = a._seq.clone()
    r.push(b)
    _seq = consume r

  fun div(that: Parser): Choice =>
    concat(this, that)

  fun parse(source: Source, offset: USize, tree: Bool, hidden: Parser)
    : ParseResult
  =>
    var fail: (USize, Parser) = (0, this)

    for p in _seq.values() do
      match \exhaustive\ p.parse(source, offset, tree, hidden)
      | (let advance: USize, let r: ParseOK) => return (advance, r)
      | (let advance: USize, let r: Parser) =>
        if advance > fail._1 then
          fail = (advance, r)
        end
      end
    end
    fail

  fun error_msg(): String => try _seq(0)?.error_msg() else "an empty choice" end
