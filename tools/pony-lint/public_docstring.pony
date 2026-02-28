use ast = "pony_compiler"

primitive PublicDocstring is ASTRule
  """
  Flags public types and methods that lack a docstring.

  "Public" means the name does not start with `_`. Several categories are
  excluded from checking:
  - Types and methods annotated with `\nodoc\` (excluded from documentation)
  - Methods inside `\nodoc\`-annotated types
  - Type aliases (Pony does not support docstrings on them)
  - Methods inside private types (not part of the public API)
  - `Main` actors (universally understood as the program entry point)

  Methods are exempt when either gate passes:
  1. The method name is a commonly-known convention (e.g. `create`, `string`,
     `eq`, `hash`, `dispose`, `next`).
  2. The method has a concrete body with 3 or fewer top-level expressions
     (covers trivial one-liner implementations).

  Abstract methods (body is TK_NONE) never get the simple-body exemption —
  they define contracts and should have docstrings.
  """
  fun id(): String val => "style/public-docstring"
  fun category(): String val => "style"

  fun description(): String val =>
    "public type or method should have a docstring"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [
      ast.TokenIds.tk_class(); ast.TokenIds.tk_actor()
      ast.TokenIds.tk_primitive(); ast.TokenIds.tk_struct()
      ast.TokenIds.tk_trait(); ast.TokenIds.tk_interface()
      ast.TokenIds.tk_fun(); ast.TokenIds.tk_new(); ast.TokenIds.tk_be()
    ]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check an entity or method node for a missing docstring. Skips `\nodoc\`
    annotations, private names, Main actors, methods inside private or
    `\nodoc\` types, exempt method names, and methods with simple bodies
    (≤ 3 top-level expressions).
    """
    let token_id = node.id()
    let is_method =
      (token_id == ast.TokenIds.tk_fun())
        or (token_id == ast.TokenIds.tk_new())
        or (token_id == ast.TokenIds.tk_be())

    // Skip \nodoc\-annotated nodes — excluded from documentation
    if node.has_annotation("nodoc") then
      return recover val Array[Diagnostic val] end
    end

    // Name: child 0 for entities, child 1 for methods
    let name_idx: USize = if is_method then 1 else 0 end

    try
      let name_node = node(name_idx)?
      match name_node.token_value()
      | let name: String val =>
        // Skip private names
        try
          if name(0)? == '_' then
            return recover val Array[Diagnostic val] end
          end
        end

        if is_method then
          // Skip methods inside private, \nodoc\, or anonymous types
          try
            let entity =
              (node.parent() as ast.AST).parent() as ast.AST
            // Anonymous objects are local — not public API
            if entity.id() == ast.TokenIds.tk_object() then
              return recover val Array[Diagnostic val] end
            end
            // \nodoc\ entity — not documented API
            if entity.has_annotation("nodoc") then
              return recover val Array[Diagnostic val] end
            end
            match entity(0)?.token_value()
            | let en: String val =>
              if en(0)? == '_' then
                return recover val Array[Diagnostic val] end
              end
            end
          end

          // Gate 1: commonly-known method names
          if _is_exempt_name(name) then
            return recover val Array[Diagnostic val] end
          end

          // Gate 2: simple body (≤ 3 top-level expressions)
          try
            let body = node(6)?
            if body.id() != ast.TokenIds.tk_none() then
              if body.num_children() <= 3 then
                return recover val Array[Diagnostic val] end
              end
            end
          end
        else
          // Entity types: skip Main actors
          if (token_id == ast.TokenIds.tk_actor())
            and (name == "Main")
          then
            return recover val Array[Diagnostic val] end
          end
        end

        // Check docstring presence
        if is_method then
          // At PassParse, method docstrings appear in two locations:
          // - Abstract methods: child 7 (between signature and absent body)
          // - Concrete methods: first expression of body (child 6)
          if not _method_has_docstring(node) then
            return recover val
              [ Diagnostic(
                id(),
                "public method '" + name + "' should have a docstring",
                source.rel_path,
                name_node.line(),
                name_node.pos())]
            end
          end
        else
          // Entity types: docstring at child 6
          try
            let doc_node = node(6)?
            if doc_node.id() == ast.TokenIds.tk_none() then
              return recover val
                [ Diagnostic(
                  id(),
                  "public type '" + name + "' should have a docstring",
                  source.rel_path,
                  name_node.line(),
                  name_node.pos())]
              end
            end
          end
        end
      end
    end
    recover val Array[Diagnostic val] end

  fun _method_has_docstring(node: ast.AST box): Bool =>
    // Check child 7: docstring for abstract methods (between signature
    // and absent body, parsed by `OPT TOKEN(NULL, TK_STRING)`)
    try
      if node(7)?.id() == ast.TokenIds.tk_string() then
        return true
      end
    end
    // Check body's first child: docstring for concrete methods
    // (first TK_STRING expression inside the TK_SEQ body)
    try
      let body = node(6)?
      if body.id() != ast.TokenIds.tk_none() then
        if body(0)?.id() == ast.TokenIds.tk_string() then
          return true
        end
      end
    end
    false

  fun _is_exempt_name(name: String val): Bool =>
    match name
    | "create" | "string"
    | "eq" | "ne"
    | "hash" | "hash64"
    | "compare" | "lt" | "le" | "ge" | "gt"
    | "dispose"
    | "has_next" | "next"
    | "values" | "size"
    => true
    else
      false
    end
