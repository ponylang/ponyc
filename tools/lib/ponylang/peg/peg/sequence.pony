class Sequence is Parser
  let _seq: Array[Parser]
  var _label: Label

  new create(a: Parser, b: Parser) =>
    _seq = [a; b]
    _label = NoLabel

  new concat(a: Sequence box, b: Parser) =>
    let r = a._seq.clone()
    r.push(b)
    _seq = consume r
    _label = a._label

  fun label(): Label => _label
  fun ref node(value: Label): Sequence => _label = value; this

  fun mul(that: Parser): Sequence =>
    concat(this, that)

  fun parse(source: Source, offset: USize, tree: Bool, hidden: Parser)
    : ParseResult
  =>
    if tree then
      _parse_tree(source, offset, hidden)
    else
      _parse_token(source, offset)
    end

  fun _parse_tree(source: Source, offset: USize, hidden: Parser): ParseResult =>
    var length = USize(0)
    let ast = AST(_label)

    for p in _seq.values() do
      match \exhaustive\ p.parse(source, offset + length, true, hidden)
      | (let advance: USize, Skipped) =>
        length = length + advance
      | (let advance: USize, let r: (Token | NotPresent)) =>
        ast.push(r)
        length = length + advance
      | (let advance: USize, let r: AST) =>
        match p
        | let p': (Sequence box | Many box) if p'.label() is NoLabel =>
          for child in r.children.values() do
            ast.push(child)
          end
        else
          ast.push(r)
        end

        length = length + advance
      | (let advance: USize, let r: Parser) => return (length + advance, r)
      else
        return (length, this)
      end
    end

    match \exhaustive\ ast.size()
    | 0 if _label is NoLabel => (length, Skipped)
    | 1 if _label is NoLabel => (length, ast.extract())
    else
      (length, consume ast)
    end

  fun _parse_token(source: Source, offset: USize): ParseResult =>
    var length = USize(0)

    for p in _seq.values() do
      match p.parse(source, offset + length, false, NoParser)
      | (0, NotPresent)
      | (0, Skipped) => None
      | (let advance: USize, Lex) => length = length + advance
      | (let advance: USize, let r: Parser) => return (length + advance, r)
      else
        return (length, this)
      end
    end

    (length, Lex)

  fun error_msg(): String =>
    try _seq(0)?.error_msg() else "an empty sequence" end
