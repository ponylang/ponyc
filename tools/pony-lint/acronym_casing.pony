use ast = "pony_compiler"

primitive AcronymCasing is ASTRule
  """
  Flags type names that contain known acronyms in lowered form instead of
  fully uppercased.

  For example, `JsonDoc` should be `JSONDoc`, and `HttpClient` should be
  `HTTPClient`. The rule checks against a dictionary of common acronyms
  (HTTP, JSON, XML, URL, TCP, UDP, SSL, TLS, FFI, AST, API, etc.).
  """
  fun id(): String val => "style/acronym-casing"
  fun category(): String val => "style"

  fun description(): String val =>
    "acronym in type name should be fully uppercased"

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
        match NamingHelpers.has_lowered_acronym(name)
        | let acronym: String val =>
          return recover val
            [Diagnostic(id(),
              "acronym '" + acronym + "' in '" + name
                + "' should be fully uppercased",
              source.rel_path, name_node.line(), name_node.pos())]
          end
        end
      end
    end
    recover val Array[Diagnostic val] end
