use ast = "ast"

primitive MemberNaming is ASTRule
  """
  Flags member names that do not follow snake_case conventions.

  Checks fields (flet, fvar, embed), methods (fun, new, be), parameters,
  and local variables (let, var). The name must start with a lowercase letter
  (after optional leading `_` for private members) and contain only lowercase
  letters, digits, and underscores.

  Dontcare locals (`_`) are skipped — at PassParse these appear as TK_LET/VAR
  with a TK_ID child whose value is `"_"`.
  """
  fun id(): String val => "style/member-naming"
  fun category(): String val => "style"
  fun description(): String val => "member name should be snake_case"
  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [
      ast.TokenIds.tk_flet(); ast.TokenIds.tk_fvar()
      ast.TokenIds.tk_embed()
      ast.TokenIds.tk_fun(); ast.TokenIds.tk_new(); ast.TokenIds.tk_be()
      ast.TokenIds.tk_param()
      ast.TokenIds.tk_let(); ast.TokenIds.tk_var()
    ]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    let token_id = node.id()

    // Methods: name is child 1
    // Fields, params, locals: name is child 0
    let name_idx: USize =
      if (token_id == ast.TokenIds.tk_fun())
        or (token_id == ast.TokenIds.tk_new())
        or (token_id == ast.TokenIds.tk_be())
      then
        1
      else
        0
      end

    try
      let name_node = node(name_idx)?
      match name_node.token_value()
      | let name: String val =>
        // Skip dontcare pattern
        if name == "_" then
          return recover val Array[Diagnostic val] end
        end
        if not NamingHelpers.is_snake_case(name) then
          return recover val
            [Diagnostic(id(),
              "member name '" + name + "' should be snake_case",
              source.rel_path, name_node.line(), name_node.pos())]
          end
        end
      end
    end
    recover val Array[Diagnostic val] end
