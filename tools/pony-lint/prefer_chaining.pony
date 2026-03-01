use ast = "pony_compiler"

primitive PreferChaining is ASTRule
  """
  Flags patterns where a value is bound to a local variable and methods are
  called on it repeatedly — when `.>` chaining would be cleaner. Also flags
  a single call followed by a trailing reference, since `.>` returns the
  receiver and eliminates the need for the intermediate variable.

  Anti-pattern (multiple calls with trailing reference):
  ```pony
  let r = Array[U32]
  r.push(1)
  r.push(2)
  r
  ```

  Anti-pattern (single call with trailing reference):
  ```pony
  let s = Set[String]
  s.set("foo")
  s
  ```

  Anti-pattern (multiple calls without trailing reference):
  ```pony
  let x = Bar
  x.baz(1)
  x.qux(2)
  ```

  Preferred:
  ```pony
  Array[U32]
    .> push(1)
    .> push(2)
  ```

  ```pony
  Set[String] .> set("foo")
  ```
  """
  fun id(): String val => "style/prefer-chaining"
  fun category(): String val => "style"

  fun description(): String val =>
    "local variable can be replaced with .> chaining"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [ast.TokenIds.tk_let(); ast.TokenIds.tk_var()]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check whether a `let` or `var` binding is only used to call methods,
    which `.>` chaining can express more concisely.
    """
    // Extract variable name from child 0 (TK_ID)
    let var_name: String val =
      try
        let name_node = node(0)?
        match name_node.token_value()
        | let name: String val => name
        else
          return recover val Array[Diagnostic val] end
        end
      else
        return recover val Array[Diagnostic val] end
      end

    // Navigate to parent — must be TK_ASSIGN
    let assign_node: ast.AST box =
      match \exhaustive\ node.parent()
      | let p: ast.AST box if p.id() == ast.TokenIds.tk_assign() => p
      else
        return recover val Array[Diagnostic val] end
      end

    // Walk siblings of the TK_ASSIGN, counting consecutive dot-calls
    // on our variable
    var call_count: USize = 0
    var current: (ast.AST box | None) = assign_node.sibling()

    while true do
      match \exhaustive\ current
      | let sib: ast.AST box if _is_dot_call_on(sib, var_name) =>
        call_count = call_count + 1
        current = sib.sibling()
      else
        break
      end
    end

    // Need at least 1 consecutive call
    if call_count == 0 then
      return recover val Array[Diagnostic val] end
    end

    // Fire when calls end the scope (2+ needed) or when followed by a
    // bare reference with no further siblings (1+ call suffices since .>
    // returns the receiver, eliminating the intermediate variable).
    let should_flag: Bool =
      match \exhaustive\ current
      | None =>
        call_count >= 2
      | let ref_node: ast.AST box
        if _is_bare_reference(ref_node, var_name)
      =>
        match ref_node.sibling()
        | let _: ast.AST box => false
        else
          true
        end
      else
        false
      end

    if should_flag then
      return recover val
        [ Diagnostic(
          id(),
          "'" + var_name + "' can be replaced with .> chaining",
          source.rel_path,
          node.line(),
          node.pos())]
      end
    end

    recover val Array[Diagnostic val] end

  fun _is_dot_call_on(node: ast.AST box, var_name: String val): Bool =>
    """
    Check whether `node` is a `TK_CALL` whose receiver is a dot-access
    on a `TK_REFERENCE` matching `var_name`:
    `TK_CALL(TK_DOT(TK_REFERENCE(TK_ID(var_name)), ...), ...)`.
    """
    if node.id() != ast.TokenIds.tk_call() then return false end
    try
      let dot = node(0)?
      if dot.id() != ast.TokenIds.tk_dot() then return false end
      let ref_node = dot(0)?
      if ref_node.id() != ast.TokenIds.tk_reference() then return false end
      let id_node = ref_node(0)?
      if id_node.id() != ast.TokenIds.tk_id() then return false end
      match id_node.token_value()
      | let name: String val => name == var_name
      else
        false
      end
    else
      false
    end

  fun _is_bare_reference(node: ast.AST box, var_name: String val): Bool =>
    """
    Check whether `node` is a bare `TK_REFERENCE` to `var_name`:
    `TK_REFERENCE(TK_ID(var_name))`.
    """
    if node.id() != ast.TokenIds.tk_reference() then return false end
    try
      let id_node = node(0)?
      if id_node.id() != ast.TokenIds.tk_id() then return false end
      match id_node.token_value()
      | let name: String val => name == var_name
      else
        false
      end
    else
      false
    end
