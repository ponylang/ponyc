use ast = "pony_compiler"

primitive TypeNaming is ASTRule
  """
  Flags type names that do not follow CamelCase conventions.

  Checks all entity declarations: class, actor, primitive, struct, trait,
  interface, and type alias. The name must start with an uppercase letter
  (after optional leading `_` for private types) and contain only
  alphanumeric characters with no underscores.
  """
  fun id(): String val => "style/type-naming"
  fun category(): String val => "style"
  fun description(): String val => "type name should be CamelCase"
  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [
      ast.TokenIds.tk_class(); ast.TokenIds.tk_actor()
      ast.TokenIds.tk_primitive(); ast.TokenIds.tk_struct()
      ast.TokenIds.tk_trait(); ast.TokenIds.tk_interface()
      ast.TokenIds.tk_type()
    ]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    try
      let name_node = node(0)?
      match name_node.token_value()
      | let name: String val =>
        if not NamingHelpers.is_camel_case(name) then
          return recover val
            [ Diagnostic(
              id(),
              "type name '" + name + "' should be CamelCase",
              source.rel_path,
              name_node.line(),
              name_node.pos())]
          end
        end
      end
    end
    recover val Array[Diagnostic val] end
