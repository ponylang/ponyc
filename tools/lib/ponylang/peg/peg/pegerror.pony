use "collections"
use "term"

type Marker is (Source, USize, USize, String)

trait box PegError
  fun category(): String
  fun description(): String
  fun markers(): Iterator[Marker] => Array[Marker].values()

class SyntaxError is PegError
  let source: Source
  let offset: USize
  let parser: Parser

  new create(source': Source, offset': USize, parser': Parser) =>
    source = source'
    offset = offset'
    parser = parser'

  fun category(): String => "Syntax Error"

  fun description(): String =>
    """
    There is a syntax error that has prevented the parser from being able to
    understand the source text.
    """

  fun markers(): Iterator[Marker] =>
    [(source, offset, USize(1), "expected " + parser.error_msg())].values()

class val DuplicateDefinition is PegError
  let def: Token
  let prev: Token

  new val create(def': Token, prev': Token) =>
    def = def'
    prev = prev'

  fun category(): String => "Duplicate Definition"

  fun description(): String =>
    """
    One of the parse rules has been defined more than once.
    """

  fun markers(): Iterator[Marker] =>
    [ (def.source, def.offset, def.length, "rule has been defined more than once")
      (prev.source, prev.offset, prev.length, "previous definition is here")
    ].values()

class val MissingDefinition is PegError
  let token: Token

  new val create(token': Token) =>
    token = token'

  fun category(): String => "Missing Definition"

  fun description(): String =>
    """
    One of the parse rules references another rule that has not been defined.
    """

  fun markers(): Iterator[Marker] =>
    [ (token.source, token.offset, token.length, "rule has not been defined")
    ].values()

class val UnknownNodeLabel is PegError
  let label: Label

  new val create(label': Label) =>
    label = label'

  fun category(): String => "Unknown Node Label"

  fun description(): String =>
    """
    There is an internal error that has resulted in an unknown node label in
    the abstract syntax tree that describes the parsing expression grammar.
    """

primitive NoStartDefinition is PegError
  fun category(): String => "No Start Rule"
  fun description(): String =>
    """
    A parsing expression grammar must have a 'start' rule. This is the initial
    rule that is applied to the source text to parse it.
    """

primitive MalformedAST is PegError
  fun category(): String => "Malformed AST"
  fun description(): String =>
    """
    There is an internal error that has resulted in an abstract syntax tree
    that the compiler does not understand.
    """

primitive PegFormatError
  fun console(e: PegError val): ByteSeqIter =>
    let text =
      recover
        [ ANSI.cyan()
          "-- "; e.category(); " --\n\n"
          ANSI.reset()
        ]
      end

    for m in e.markers() do
      (let line, let col) = Position(m._1, m._2)
      let source = Position.text(m._1, m._2, col)
      let indent = Position.indent(source, col)
      let mark = recover String(m._3) end

      for i in Range(0, m._3) do
        mark.append("^")
      end

      let line_text: String = line.string()
      let line_indent = Position.indent(line_text, line_text.size() + 1)

      text.append(
        recover
          [ ANSI.grey()
            m._1.path; ":"; line_text; ":"; col.string(); ":"; m._3.string()
            "\n\n"
            line_text; ": "
            ANSI.reset()
            source; "\n"
            ANSI.red()
            line_indent; "  "; indent; consume mark; "\n"
            line_indent; "  "; indent; m._4
            ANSI.reset()
            "\n\n"
          ]
        end
        )
    end

    text.append(
      recover
        [ e.description()
          "\n\n"
        ]
      end
      )

    text

  fun json(e: PegError val): ByteSeqIter =>
    let text =
      recover
        [ "{\n  \"category\": \""
          e.category()
          "\"\n  \"description\": \""
          e.description()
          "\"\n  \"markers\":\n  [\n"
        ]
      end

    for m in e.markers() do
      (let line, let col) = Position(m._1, m._2)

      text.append(
        recover
          [ as String:
            "    {\n"
            "      \"file\": \""; m._1.path; "\"\n"
            "      \"line\": "; line.string(); "\n"
            "      \"column\": "; col.string(); "\n"
            "      \"length\": "; m._3.string(); "\n"
            "      \"text\": \""; m._4; "\"\n"
            "    }\n"
          ]
        end
        )
    end

    text.push("  ]\n}\n")
    text
