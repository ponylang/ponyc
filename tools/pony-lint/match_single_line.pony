use ast = "pony_compiler"

primitive MatchSingleLine is ASTRule
  """
  Flags `match \exhaustive\` expressions that fit entirely on a single line.

  Match expressions should always span multiple lines for readability. A
  single-line match \exhaustive\ is harder to scan and usually indicates the expression
  would be clearer as an `if`/`else` chain.
  """
  fun id(): String val => "style/match-no-single-line"
  fun category(): String val => "style"

  fun description(): String val =>
    "match expression must span multiple lines"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [ast.TokenIds.tk_match()]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check whether all descendants of the match \exhaustive\ node are on the same line as
    the `match \exhaustive\` keyword. If so, the entire match is single-line — flag it.
    """
    let match_line = node.line()
    let mlv = _MaxLineVisitor(match_line)
    node.visit(mlv)
    if mlv.max_line == match_line then
      recover val
        [ Diagnostic(
          id(),
          "match expression should span multiple lines",
          source.rel_path,
          match_line,
          node.pos())]
      end
    else
      recover val Array[Diagnostic val] end
    end
