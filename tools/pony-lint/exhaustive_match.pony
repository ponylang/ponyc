use ast = "pony_compiler"

primitive ExhaustiveMatch is ASTRule
  """
  Flags exhaustive `match` expressions that lack the `\exhaustive\` annotation.

  The `\exhaustive\` annotation is a compiler assertion that demands explicit
  handling of all cases in a match. Without it, if a new variant is later added
  to a union type, the compiler silently injects `else None` instead of
  flagging the missing case. This rule encourages annotating exhaustive matches
  for that future protection.

  Detection relies on the compiler's own exhaustiveness analysis: after
  `PassExpr`, the else child (index 2) of a `TK_MATCH` node remains `TK_NONE`
  when the compiler confirmed exhaustiveness and no injection was needed.
  """
  fun id(): String val => "safety/exhaustive-match"
  fun category(): String val => "safety"

  fun description(): String val =>
    "exhaustive match should use \\exhaustive\\ annotation"

  fun default_status(): RuleStatus => RuleOn

  fun required_pass(): ast.PassId => ast.PassExpr

  fun node_filter(): Array[ast.TokenId] val =>
    [ast.TokenIds.tk_match()]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    if not node.has_annotation("exhaustive") then
      try
        let else_clause = node(2)?
        if else_clause.id() == ast.TokenIds.tk_none() then
          return recover val
            [ Diagnostic(
              id(),
              "exhaustive match should use \\exhaustive\\ annotation",
              source.rel_path,
              node.line(),
              node.pos()) ]
          end
        end
      end
    end
    recover val Array[Diagnostic val] end
