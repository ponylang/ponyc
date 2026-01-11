use "collections"

primitive Printer
  fun apply(p: ASTChild, depth: USize = 0, indent: String = "  ",
    s: String ref = String): String ref
  =>
    _indent(depth, indent, s)
    s.append("(")
    s.append(p.label().text())

    match p
    | let ast: AST =>
      s.append("\n")
      for child in ast.children.values() do
        Printer(child, depth + 1, indent, s)
      end
      _indent(depth, indent, s)
    | let token: Token =>
      s.append(" ")
      s.append(token.source.content, token.offset, token.length)
    end
    s.append(")\n")
    s

  fun _indent(depth: USize, indent: String, s: String ref) =>
    for i in Range(0, depth) do
      s.append(indent)
    end
