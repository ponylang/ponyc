use ".."
use "pony_compiler"

primitive _TypeFormatter
  """
  Pure AST-to-string type formatting utilities shared across LSP features.
  """

  fun extract_type(type_node: AST box, channel: Channel): String =>
    """
    Extract type information from a type node.
    """
    let node_id = type_node.id()
    match node_id
    | TokenIds.tk_nominal() =>
      // Nominal type
      try
        let num_children = type_node.num_children()
        if num_children > 1 then
          let type_id = type_node(1)?
          let base_name =
            if type_id.id() == TokenIds.tk_id() then
              type_id.token_value() as String
            else
              extract_type(type_id, channel)
            end

          // Check for type arguments
          let type_args_str =
            match \exhaustive\
              _find_child_by_type(
                type_node,
                TokenIds.tk_typeargs(),
                2)
            | let ta: AST box => _extract_typeargs(ta, channel)
            | None => ""
            end

          // Check for capability
          var capability_str: String = ""
          var idx: USize = 2
          while idx < num_children do
            try
              let child = type_node(idx)?
              let cap = extract_capability(child.id())
              if cap != "" then
                capability_str = cap
                break
              end
            end
            idx = idx + 1
          end

          let cap_str =
            if capability_str.size() > 0 then
              " " + capability_str
            else
              ""
            end
          base_name + type_args_str + cap_str
        else
          "?"
        end
      else
        "?"
      end
    | TokenIds.tk_typealiasref() =>
      // Type alias reference: children are (id, typeargs, cap, eph)
      try
        let num_children = type_node.num_children()
        let type_id = type_node(0)?
        let base_name =
          if type_id.id() == TokenIds.tk_id() then
            type_id.token_value() as String
          else
            extract_type(type_id, channel)
          end

        // Check for type arguments at child(1)
        let type_args_str =
          match \exhaustive\
            _find_child_by_type(
              type_node,
              TokenIds.tk_typeargs(),
              1)
          | let ta: AST box => _extract_typeargs(ta, channel)
          | None => ""
          end

        // Check for capability starting at child(2)
        var capability_str: String = ""
        var idx: USize = 2
        while idx < num_children do
          try
            let child = type_node(idx)?
            let cap = extract_capability(child.id())
            if cap != "" then
              capability_str = cap
              break
            end
          end
          idx = idx + 1
        end

        let cap_str =
          if capability_str.size() > 0 then
            " " + capability_str
          else
            ""
          end
        base_name + type_args_str + cap_str
      else
        "?"
      end
    | TokenIds.tk_id() =>
      // Direct ID node
      try
        type_node.token_value() as String
      else
        "?"
      end
    | TokenIds.tk_typeparamref() =>
      // Type parameter reference
      try
        let child = type_node(0)?
        if child.id() == TokenIds.tk_id() then
          child.token_value() as String
        else
          type_node.token_value() as String
        end
      else
        try
          type_node.token_value() as String
        else
          "?"
        end
      end
    | TokenIds.tk_uniontype() =>
      // Union type: (Type1 | Type2 | Type3)
      _extract_composite_type(type_node, " | ", channel)
    | TokenIds.tk_isecttype() =>
      // Intersection type
      _extract_composite_type(type_node, " & ", channel)
    | TokenIds.tk_tupletype() =>
      // Tuple type: (Type1, Type2, Type3)
      _extract_composite_type(type_node, ", ", channel)
    | TokenIds.tk_arrow() =>
      // Arrow type: left->right
      try
        let left_child = type_node(0)?
        let left_cap = extract_capability(left_child.id())
        let right = extract_type(type_node(1)?, channel)
        if left_cap.size() > 0 then
          left_cap + "->" + right
        else
          right
        end
      else
        "?"
      end
    else
      // For other type node types
      TokenIds.string(node_id)
    end

  fun extract_capability(node_id: I32): String =>
    """
    Extract capability string from a capability token ID.
    """
    match node_id
    | TokenIds.tk_iso() => "iso"
    | TokenIds.tk_trn() => "trn"
    | TokenIds.tk_ref() => "ref"
    | TokenIds.tk_val() => "val"
    | TokenIds.tk_box() => "box"
    | TokenIds.tk_tag() => "tag"
    else
      ""
    end

  fun extract_type_params(
    type_params: AST box,
    channel: Channel): String
  =>
    """
    Extract type parameters from typeparams node.
    """
    let type_param_strs = Array[String]
    for type_param in type_params.children() do
      try
        let tp_id = type_param(0)?
        if tp_id.id() == TokenIds.tk_id() then
          let tp_name = tp_id.token_value() as String
          let constraint =
            try
              let constraint_node = type_param(1)?
              ": " + extract_type(constraint_node, channel)
            else
              ""
            end
          type_param_strs.push(tp_name + constraint)
        end
      end
    end
    if type_param_strs.size() > 0 then
      "[" + ", ".join(type_param_strs.values()) + "]"
    else
      ""
    end

  fun _extract_typeargs(typeargs_node: AST box, channel: Channel): String =>
    """
    Extract type arguments from a tk_typeargs node.
    """
    let arg_strs = Array[String]
    for arg in typeargs_node.children() do
      arg_strs.push(extract_type(arg, channel))
    end
    if arg_strs.size() > 0 then
      "[" + ", ".join(arg_strs.values()) + "]"
    else
      ""
    end

  fun _extract_composite_type(
    type_node: AST box,
    separator: String,
    channel: Channel): String
  =>
    """
    Extract composite types (union, intersection, tuple) by recursively
    extracting children.
    """
    let type_strs = Array[String]
    for child in type_node.children() do
      type_strs.push(extract_type(child, channel))
    end
    if type_strs.size() > 0 then
      "(" + separator.join(type_strs.values()) + ")"
    else
      "?"
    end

  fun _find_child_by_type(
    ast: AST box,
    token_id: I32,
    start_index: USize = 0): (AST box | None)
  =>
    """
    Search for a child node with a specific token type.
    """
    var idx = start_index
    while idx < ast.num_children() do
      try
        let child = ast(idx)?
        if child.id() == token_id then
          return child
        end
      end
      idx = idx + 1
    end
    None
