use ".."
use "pony_compiler"
use "json"

primitive SignatureHelp
  """
  Resolves signature help (parameter hints) for a call expression under
  the cursor.  Walks up from the cursor node to find the enclosing
  tk_call, resolves the callee to its definition, extracts per-parameter
  information, and returns the LSP SignatureHelp JSON object.
  """

  fun collect(
    node: AST box,
    cursor_line: USize,
    cursor_col: USize,
    channel: Channel): (JsonObject | None)
  =>
    match \exhaustive\ _find_enclosing_call(node, cursor_line, cursor_col)
    | (_, let callee: AST box, let active_param: USize) =>
      try
        // After typecheck, expr_qualify transforms
        // TK_QUALIFY(inner_funref, TK_TYPEARGS) into a node with the same ID
        // as the inner ref (e.g. TK_FUNREF) but retaining the original
        // children: child(0) = inner funref, child(1) = TK_TYPEARGS.
        // definitions() on the outer node fails because it sees a funref
        // child where it expects a receiver expression. Detect this by
        // checking whether child(0) is itself a function reference.
        let resolved_callee: AST box =
          try
            let c = callee(0)?
            let cid = c.id()
            if (cid == TokenIds.tk_funref()) or
              (cid == TokenIds.tk_beref()) or
              (cid == TokenIds.tk_newref()) or
              (cid == TokenIds.tk_newberef())
            then
              c
            else
              callee
            end
          else
            callee
          end
        let defs = resolved_callee.definitions()
        if defs.size() == 0 then
          return None
        end
        let def = defs(0)?

        let keyword: String val =
          match def.id()
          | TokenIds.tk_fun() => "fun"
          | TokenIds.tk_be() => "be"
          | TokenIds.tk_new() => "new"
          else
            return None
          end

        let param_strs: Array[String] =
          try
            _extract_param_list(def(3)?, channel)
          else
            Array[String]
          end

        (let label, let offsets) =
          _build_label(def, keyword, param_strs, channel)

        var params_json = JsonArray
        for (p_start, p_end) in offsets.values() do
          params_json =
            params_json.push(
              JsonObject.update(
                "label",
                JsonArray
                  .push(I64.from[USize](p_start))
                  .push(I64.from[USize](p_end))))
        end

        let base_sig = JsonObject
          .update("label", label)
          .update("parameters", params_json)

        let sig =
          try
            let doc_node = def(7)?
            if doc_node.id() == TokenIds.tk_string() then
              let docstring = doc_node.token_value() as String
              if docstring.size() > 0 then
                base_sig.update(
                  "documentation",
                  JsonObject
                    .update("kind", "markdown")
                    .update("value", docstring))
              else
                base_sig
              end
            else
              base_sig
            end
          else
            base_sig
          end

        // Omit activeParameter for zero-param signatures: index 0 into an
        // empty parameters array is out of range per LSP 3.17 spec.
        let base_result = JsonObject
          .update("signatures", JsonArray.push(sig))
          .update("activeSignature", I64(0))
        if param_strs.size() > 0 then
          base_result.update("activeParameter", I64.from[USize](active_param))
        else
          base_result
        end
      else
        None
      end
    | None =>
      None
    end

  fun _find_enclosing_call(
    node: AST box,
    cursor_line: USize,
    cursor_col: USize): ((AST box, AST box, USize) | None)
  =>
    """
    Walks up the AST from `node` looking for the innermost tk_call whose
    argument list contains the cursor.  Returns (call_node, callee_ref,
    active_param_index) on success.

    When a tk_call is encountered that does not contain the cursor in its
    argument list, it is skipped and the walk continues upward — this lets
    a position on the callee of an inner call still resolve to the
    surrounding outer call's signature.

    Two skip conditions:
    - Callee check: arrived via child(0) of tk_call and the call has args —
      cursor is on the callee expression, not inside the arg list.  For
      zero-arg calls the cursor is on the callee by definition, so the
      signature is returned immediately.
    - Position gate: cursor is on the same line as the first arg but before
      it (i.e. at the open paren of a same-line call).

    Named arguments: after typecheck the compiler resolves named args into
    positional order, so named-arg values are indistinguishable from
    positional args in the AST and activeParameter is reported correctly.
    Named-arg name tokens (the identifiers before `=`) are not part of any
    arg slot; find_node_at returns None for them and no signature is shown.
    """
    var current: AST box = node
    var pos_arg_child: (AST box | None) = None

    while true do
      match current.parent()
      | let parent: AST box =>
        let pid = parent.id()
        if pid == TokenIds.tk_positionalargs() then
          pos_arg_child = current
        elseif pid == TokenIds.tk_call() then
          let from_callee: Bool =
            try current == parent(0)? else false end

          if from_callee then
            // Zero-arg call: cursor is naturally on the callee — show sig.
            let is_zero_args: Bool =
              try parent(1)?.child() is None else true end
            if is_zero_args then
              try
                return (parent, parent(0)?, 0)
              end
              break
            end
            // Has args and cursor is on the callee — skip this call.
            pos_arg_child = None
          else
            // Check position gate: cursor before first arg on same line.
            let before_first_arg: Bool =
              try
                let first_seq = parent(1)?.child() as AST box
                let first_arg = first_seq.child() as AST box
                // Use span() so complex first args (e.g. a method call
                // whose AST node sits at the dot, not at the leftmost
                // token) are correctly bounded.
                (let arg_start, _) = first_arg.span()
                (cursor_line == arg_start.line()) and
                  (cursor_col < arg_start.column())
              else
                false
              end

            if before_first_arg then
              // Cursor is at the open paren — skip this call.
              pos_arg_child = None
            else
              let active: USize =
                match \exhaustive\ pos_arg_child
                | let fc: AST box =>
                  try
                    let args = parent(1)?
                    var idx: USize = 0
                    for arg in args.children() do
                      if arg == fc then break end
                      idx = idx + 1
                    end
                    idx
                  else
                    0
                  end
                | None => 0
                end
              try
                return (parent, parent(0)?, active)
              end
              break
            end
          end
        end
        current = parent
      else
        break
      end
    end
    None

  fun _extract_param_list(params: AST box, channel: Channel): Array[String] =>
    """
    Returns one string per parameter in the form "name: Type".
    """
    let result = Array[String]
    if params.id() == TokenIds.tk_params() then
      for param in params.children() do
        try
          let pid = param(0)?
          if pid.id() == TokenIds.tk_id() then
            let name = pid.token_value() as String
            let type_str =
              try
                let ptype = param(1)?
                if ptype.id() != TokenIds.tk_none() then
                  ": " + _TypeFormatter.extract_type(ptype, channel)
                else
                  ""
                end
              else
                ""
              end
            result.push(name + type_str)
          end
        end
      end
    end
    result

  fun _build_label(
    def: AST box,
    keyword: String val,
    param_strs: Array[String] box,
    channel: Channel): (String, Array[(USize, USize)])
  =>
    """
    Builds the full signature label string and computes per-parameter
    byte-offset pairs [start, end) into that string.
    Uses string concatenation (not mutation) so the label is String val.
    """
    let offsets = Array[(USize, USize)]

    // Build prefix via concatenation so the result is String iso^ -> val
    var label: String val = keyword
    try
      let cap = _TypeFormatter.extract_capability(def(0)?.id())
      if cap.size() > 0 then
        label = label + " " + cap
      end
    end
    label = label + " "
    try
      let name_node = def(1)?
      if name_node.id() == TokenIds.tk_id() then
        label = label + (name_node.token_value() as String)
      end
    end
    try
      let tp = def(2)?
      if tp.id() == TokenIds.tk_typeparams() then
        label = label + _TypeFormatter.extract_type_params(tp, channel)
      end
    end
    label = label + "("
    // Offsets are byte positions into `label`. LSP 3.17 specifies parameter
    // label offsets in the negotiated encoding (default: UTF-16 code units).
    // Pony identifiers and type names are ASCII-only, so byte offsets and
    // UTF-16 code unit offsets are equivalent. If non-ASCII ever appears in
    // a label (e.g. a unicode identifier), these offsets would be wrong.
    var first = true
    for param_str in param_strs.values() do
      let sep: String val =
        if first then
          ""
        else
          ", "
        end
      first = false
      let start: USize = label.size() + sep.size()
      label = label + sep + param_str
      offsets.push((start, label.size()))
    end
    label = label + ")"
    if (keyword == "fun") or (keyword == "be") then
      try
        let rt = def(4)?
        let rt_str = _TypeFormatter.extract_type(rt, channel)
        if rt_str.size() > 0 then
          label = label + ": " + rt_str
        end
      end
    end
    (label, offsets)
