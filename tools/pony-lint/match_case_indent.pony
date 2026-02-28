use ast = "pony_compiler"

primitive MatchCaseIndent is ASTRule
  """
  Flags `match` cases whose `|` is not aligned with the `match` keyword.

  The `|` introducing each case should start at the same column as the `match`
  keyword, regardless of how deeply nested the match is. This makes the
  match structure visually clear.
  """
  fun id(): String val => "style/match-case-indent"
  fun category(): String val => "style"

  fun description(): String val =>
    "match case '|' must align with 'match' keyword"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [ast.TokenIds.tk_match()]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    For each TK_CASE child of the match's TK_CASES node, verify that the
    `|` column matches the `match` keyword column.
    """
    let match_col = node.pos()
    let result = recover iso Array[Diagnostic val] end
    try
      let cases_node = node(1)?
      if cases_node.id() != ast.TokenIds.tk_cases() then
        return recover val Array[Diagnostic val] end
      end
      var i: USize = 0
      while i < cases_node.num_children() do
        try
          let case_node = cases_node(i)?
          if case_node.id() == ast.TokenIds.tk_case() then
            let case_col = case_node.pos()
            // Only check alignment when | is the first non-whitespace
            // on its line. Inline continuations (| "a" | "b") are
            // secondary pipes that can't align with match.
            try
              let line_text = source.lines(case_node.line() - 1)?
              if _is_first_nonws(line_text, case_col) then
                if case_col != match_col then
                  result.push(Diagnostic(
                    id(),
                    "'|' should align with 'match' keyword (column "
                      + match_col.string() + ")",
                    source.rel_path,
                    case_node.line(),
                    case_col))
                end
              end
            end
          end
        end
        i = i + 1
      end
    end
    consume result

  fun _is_first_nonws(line: String val, col: USize): Bool =>
    """
    Check if the character at `col` (1-based) is the first non-whitespace
    character on the line.
    """
    var i: USize = 0
    let target = col - 1
    while i < target do
      try
        let ch = line(i)?
        if (ch != ' ') and (ch != '\t') then return false end
      end
      i = i + 1
    end
    true
