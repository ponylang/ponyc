"""
# LSP Hover Support

This module implements the Language Server Protocol (LSP) `textDocument/hover` capability
for the Pony language server. Hover provides contextual information when users hover their
cursor over code elements in the editor.

## LSP Hover Specification

The hover capability is defined in the LSP specification:
https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_hover

The hover request returns information about the symbol at a given text document position,
typically displayed in a tooltip when hovering over code in the editor.

## Implementation Overview

This implementation provides hover information for:

### Supported Language Constructs
- **Entities**: class, actor, trait, interface, primitive, type, struct
- **Methods**: fun, be (behavior), new (constructor)
- **Fields**: let, var, embed
- **Local Variables**: let, var declarations
- **References**: Follows identifiers and type references to their definitions

### Hover Information Includes
- Declaration signatures with keywords and parameters
- Type annotations for fields and return types
- Type parameters with constraints
- Docstrings extracted from the AST

## Architecture

The module is organized into three layers:

### 1. Value Objects (Pure Data)
- `EntityInfo`: Information about entity declarations
- `MethodInfo`: Information about method declarations
- `FieldInfo`: Information about field declarations

### 2. Pure Formatting Functions (Testable)
- `format_entity()`: Format entity hover text
- `format_method()`: Format method hover text
- `format_field()`: Format field hover text

### 3. AST Extraction (Impure)
- `create_hover()`: Main entry point, dispatches based on AST node type
- `extract_*_info()`: Extract information from AST nodes
- `_format_*()`: Format specific node types
- Helper functions for extracting types, parameters, and docstrings

## Output Format

Hover text is formatted as Markdown with Pony syntax highlighting:

```
```pony
class MyClass[T: Any val]
```

This is a sample class that demonstrates hover functionality.
```

## Usage

```pony
let hover_text = HoverFormatter.create_hover(ast_node, channel)
match hover_text
| let text: String => // Display in hover tooltip
| None => // No hover info available
end
```
"""
use ".."
use "pony_compiler"

class val EntityInfo
  """
  Information about an entity declaration (class, actor, trait, etc.)
  """
  let keyword: String
  let name: String
  let type_params: String
  let docstring: String

  new val create(keyword': String, name': String, type_params': String = "", docstring': String = "") =>
    keyword = keyword'
    name = name'
    type_params = type_params'
    docstring = docstring'

class val MethodInfo
  """
  Information about a method declaration (fun, be, new)
  """
  let keyword: String
  let name: String
  let receiver_cap: String
  let type_params: String
  let params: String
  let return_type: String
  let docstring: String

  new val create(keyword': String, name': String, receiver_cap': String = "", type_params': String = "", params': String = "()", return_type': String = "", docstring': String = "") =>
    keyword = keyword'
    name = name'
    receiver_cap = receiver_cap'
    type_params = type_params'
    params = params'
    return_type = return_type'
    docstring = docstring'

class val FieldInfo
  """
  Information about a field declaration (let, var, embed)
  """
  let keyword: String
  let name: String
  let field_type: String

  new val create(keyword': String, name': String, field_type': String = "") =>
    keyword = keyword'
    name = name'
    field_type = field_type'

primitive HoverFormatter
  """
  Extracts hover information from AST nodes and formats as markdown.
  """

  // ========== Pure Formatting Functions (Testable) ==========

  fun format_entity(info: EntityInfo): String =>
    """
    Format entity declaration as markdown hover text.
    """
    let declaration = info.keyword + " " + info.name + info.type_params
    let code_block = _wrap_code_block(consume declaration)
    if info.docstring.size() > 0 then
      code_block + "\n\n" + info.docstring
    else
      code_block
    end

  fun format_method(info: MethodInfo): String =>
    """
    Format method information into markdown hover text.
    Returns a string with signature in a code block and optional docstring.
    """
    let cap_str = if info.receiver_cap.size() > 0 then " " + info.receiver_cap else "" end
    let signature = info.keyword + cap_str + " " + info.name + info.type_params + info.params + info.return_type
    let code_block = _wrap_code_block(consume signature)
    if info.docstring.size() > 0 then
      code_block + "\n\n" + info.docstring
    else
      code_block
    end

  fun format_field(info: FieldInfo): String =>
    """
    Format field declaration as markdown hover text.
    """
    let declaration = info.keyword + " " + info.name + info.field_type
    _wrap_code_block(consume declaration)

  // ========== AST Extraction and Dispatch ==========

  fun tag create_hover(ast: AST box, channel: Channel): (String | None) =>
    """
    Main entry point. Returns markdown string or None if no hover info available.
    Dispatches to specialized formatters based on AST node type.
    """
    match ast.id()
    // Entity types
    | TokenIds.tk_class() => _format_entity(ast, "class", channel)
    | TokenIds.tk_actor() => _format_entity(ast, "actor", channel)
    | TokenIds.tk_trait() => _format_entity(ast, "trait", channel)
    | TokenIds.tk_interface() => _format_entity(ast, "interface", channel)
    | TokenIds.tk_primitive() => _format_entity(ast, "primitive", channel)
    | TokenIds.tk_type() => _format_entity(ast, "type", channel)
    | TokenIds.tk_struct() => _format_entity(ast, "struct", channel)

    // Method types
    | TokenIds.tk_fun() => _format_method(ast, "fun", channel)
    | TokenIds.tk_be() => _format_method(ast, "be", channel)
    | TokenIds.tk_new() => _format_method(ast, "new", channel)

    // Field types
    | TokenIds.tk_flet() => _format_field(ast, "let", channel)
    | TokenIds.tk_fvar() => _format_field(ast, "var", channel)
    | TokenIds.tk_embed() => _format_field(ast, "embed", channel)

    // Local variable declarations
    | TokenIds.tk_let() => _format_local_var(ast, "let", channel)
    | TokenIds.tk_var() => _format_local_var(ast, "var", channel)

    // Parameter declarations
    | TokenIds.tk_param() => _format_param(ast, channel)

    // Type references - try to follow to definition
    | TokenIds.tk_reference() => _format_reference(ast, channel)
    | TokenIds.tk_typeref() => _format_reference(ast, channel)
    | TokenIds.tk_nominal() => _format_reference(ast, channel)

    // Function/method/constructor calls - follow to definition
    | TokenIds.tk_funref() => _format_reference(ast, channel)
    | TokenIds.tk_beref() => _format_reference(ast, channel)
    | TokenIds.tk_newref() => _format_reference(ast, channel)
    | TokenIds.tk_newberef() => _format_reference(ast, channel)
    | TokenIds.tk_funchain() => _format_reference(ast, channel)
    | TokenIds.tk_bechain() => _format_reference(ast, channel)

    // Field references - follow to definition
    | TokenIds.tk_fletref() => _format_reference(ast, channel)
    | TokenIds.tk_fvarref() => _format_reference(ast, channel)
    | TokenIds.tk_embedref() => _format_reference(ast, channel)

    // Local variable references - follow to definition
    | TokenIds.tk_letref() => _format_reference(ast, channel)
    | TokenIds.tk_varref() => _format_reference(ast, channel)

    // Parameter references - follow to definition
    | TokenIds.tk_paramref() => _format_reference(ast, channel)

    // Identifier - try to get info from parent or follow to definition
    | TokenIds.tk_id() => _format_id(ast, channel)

    else
      // For other node types, return None
      None
    end

  fun tag _format_entity(ast: AST box, keyword: String, channel: Channel): (String | None) =>
    """
    Format entity declarations (class, actor, trait, etc.)
    Entity structure: child(0) = id, child(1) = cap, child(2) = cap, child(3) = provides,
                      child(4) = members, child(5) = at, child(6) = docstring
    """
    match extract_entity_info(ast, keyword, channel)
    | let info: EntityInfo => format_entity(info)
    | None => None
    end

  fun tag extract_entity_info(ast: AST box, keyword: String, channel: Channel): (EntityInfo | None) =>
    """
    Extract entity information from AST node.
    """
    try
      let id = ast(0)?
      if id.id() == TokenIds.tk_id() then
        let name = id.token_value() as String

        // Extract type parameters
        let type_params_str = match _find_child_by_type(ast, TokenIds.tk_typeparams(), 1)
        | let tp: AST box => _extract_type_params(tp, channel)
        | None => ""
        end

        // Extract docstring from child 6
        let docstring: String = try
          let doc_node = ast(6)?
          if doc_node.id() == TokenIds.tk_string() then
            doc_node.token_value() as String
          else
            ""
          end
        else
          ""
        end

        EntityInfo(keyword, name, type_params_str, docstring)
      else
        None
      end
    else
      None
    end

  fun tag _format_method(ast: AST box, keyword: String, channel: Channel): (String | None) =>
    """
    Format method declarations (fun, be, new)
    Method structure: child(0) = cap, child(1) = id, child(2) = typeparams, child(3) = params,
                      child(4) = return_type, child(5) = error, child(6) = body, child(7) = docstring
    """
    match extract_method_info(ast, keyword, channel)
    | let info: MethodInfo => format_method(info)
    | None => None
    end

  fun tag extract_method_info(ast: AST box, keyword: String, channel: Channel): (MethodInfo | None) =>
    """
    Extract method information from AST node.
    """
    try
      // Extract receiver capability from child(0)
      let receiver_cap = try
        let cap = ast(0)?
        _extract_capability(cap.id())
      else
        ""
      end

      let id = ast(1)?
      if id.id() == TokenIds.tk_id() then
        let name = id.token_value() as String

        // Extract type parameters
        let type_params_str = try
          let type_params = ast(2)?
          if type_params.id() == TokenIds.tk_typeparams() then
            _extract_type_params(type_params, channel)
          else
            ""
          end
        else
          ""
        end

        // Extract parameters
        let params_str = try
          let params = ast(3)?
          _extract_params(params, channel)
        else
          "()"
        end

        // Extract return type (only for fun/be, not new)
        let return_type_str = if (keyword == "fun") or (keyword == "be") then
          try
            let return_type = ast(4)?
            ": " + _extract_type(return_type, channel)
          else
            ""
          end
        else
          ""
        end

        // Extract docstring from child 7
        let docstring: String = try
          let doc_node = ast(7)?
          if doc_node.id() == TokenIds.tk_string() then
            doc_node.token_value() as String
          else
            ""
          end
        else
          ""
        end

        MethodInfo(keyword, name, receiver_cap, type_params_str, params_str, return_type_str, docstring)
      else
        None
      end
    else
      None
    end

  fun tag _format_field(ast: AST box, keyword: String, channel: Channel): (String | None) =>
    """
    Format field declarations (let, var, embed)
    Field structure: child(0) = id, child(1) = type, child(2) = initializer
    Note: Fields don't support docstrings in Pony syntax
    """
    match _extract_field_info(ast, keyword, channel)
    | let info: FieldInfo => format_field(info)
    | None => None
    end

  fun tag _extract_field_info(ast: AST box, keyword: String, channel: Channel): (FieldInfo | None) =>
    """
    Extract field information from AST node.
    """
    try
      let id = ast(0)?
      if id.id() == TokenIds.tk_id() then
        let name = id.token_value() as String

        // Extract type annotation
        let type_str = try
          let field_type = ast(1)?
          ": " + _extract_type(field_type, channel)
        else
          ""
        end

        FieldInfo(keyword, name, type_str)
      else
        None
      end
    else
      None
    end

  fun tag _format_local_var(ast: AST box, keyword: String, channel: Channel): (String | None) =>
    """
    Format local variable declarations (let, var)
    Local var structure: child(0) = id, child(1) = type
    """
    match _extract_local_var_info(ast, keyword, channel)
    | let info: FieldInfo => format_field(info)  // Same format as fields
    | None => None
    end

  fun tag _extract_local_var_info(ast: AST box, keyword: String, channel: Channel): (FieldInfo | None) =>
    """
    Extract local variable information from AST node.
    """
    try
      let id = ast(0)?
      if id.id() == TokenIds.tk_id() then
        let name = id.token_value() as String

        // Extract type annotation if present
        let type_str = try
          let var_type = ast(1)?
          if var_type.id() != TokenIds.tk_none() then
            ": " + _extract_type(var_type, channel)
          else
            ""
          end
        else
          ""
        end

        FieldInfo(keyword, name, type_str)
      else
        None
      end
    else
      None
    end

  fun tag _format_param(ast: AST box, channel: Channel): (String | None) =>
    """
    Format parameter declarations
    Parameter structure: child(0) = id, child(1) = type
    """
    match _extract_param_info(ast, channel)
    | let info: FieldInfo => format_field(info)  // Same format as fields
    | None => None
    end

  fun tag _extract_param_info(ast: AST box, channel: Channel): (FieldInfo | None) =>
    """
    Extract parameter information from AST node.
    """
    try
      let id = ast(0)?
      if id.id() == TokenIds.tk_id() then
        let name = id.token_value() as String

        // Extract type annotation
        let type_str = try
          let param_type = ast(1)?
          if param_type.id() != TokenIds.tk_none() then
            ": " + _extract_type(param_type, channel)
          else
            ""
          end
        else
          ""
        end

        FieldInfo("param", name, type_str)
      else
        None
      end
    else
      None
    end

  fun tag _format_id(ast: AST box, channel: Channel): (String | None) =>
    """
    Format identifier nodes - look at parent to get full context, or follow to definition
    """
    try
      let name = ast.token_value() as String

      // First, try to get the parent node to determine if this is a declaration
      match ast.parent()
      | let parent: AST =>
        // Check what kind of declaration this ID belongs to
        match parent.id()
        | TokenIds.tk_class() => _format_entity(parent, "class", channel)
        | TokenIds.tk_actor() => _format_entity(parent, "actor", channel)
        | TokenIds.tk_trait() => _format_entity(parent, "trait", channel)
        | TokenIds.tk_interface() => _format_entity(parent, "interface", channel)
        | TokenIds.tk_primitive() => _format_entity(parent, "primitive", channel)
        | TokenIds.tk_type() => _format_entity(parent, "type", channel)
        | TokenIds.tk_struct() => _format_entity(parent, "struct", channel)
        | TokenIds.tk_fun() => _format_method(parent, "fun", channel)
        | TokenIds.tk_be() => _format_method(parent, "be", channel)
        | TokenIds.tk_new() => _format_method(parent, "new", channel)
        | TokenIds.tk_flet() => _format_field(parent, "let", channel)
        | TokenIds.tk_fvar() => _format_field(parent, "var", channel)
        | TokenIds.tk_embed() => _format_field(parent, "embed", channel)
        | TokenIds.tk_let() => _format_local_var(parent, "let", channel)
        | TokenIds.tk_var() => _format_local_var(parent, "var", channel)
        | TokenIds.tk_letref() => _format_reference(parent, channel)
        | TokenIds.tk_varref() => _format_reference(parent, channel)
        | TokenIds.tk_fletref() => _format_reference(parent, channel)
        | TokenIds.tk_fvarref() => _format_reference(parent, channel)
        | TokenIds.tk_embedref() => _format_reference(parent, channel)
        | TokenIds.tk_paramref() => _format_reference(parent, channel)
        else
          // Parent is not a declaration - try to follow to definition
          _format_from_definition(ast, channel)
        end
      else
        // No parent - try to follow to definition
        _format_from_definition(ast, channel)
      end
    else
      None
    end

  fun tag _format_reference(ast: AST box, channel: Channel): (String | None) =>
    """
    Format type reference nodes by following to their definition
    """
    _format_from_definition(ast, channel)

  fun tag _format_from_definition(ast: AST box, channel: Channel): (String | None) =>
    """
    Follow an identifier or reference to its definition and format that.
    """
    try
      // Use definitions() to find where this is defined
      let defs = ast.definitions()
      if defs.size() > 0 then
        // Get the first definition
        let definition = defs(0)?
        _format_from_found_definition(definition, channel)
      else
        None
      end
    else
      None
    end

  fun tag _format_from_found_definition(definition: AST box, channel: Channel): (String | None) =>
    """
    Format a definition AST node based on its type
    """
    match definition.id()
    | TokenIds.tk_class() => _format_entity(definition, "class", channel)
    | TokenIds.tk_actor() => _format_entity(definition, "actor", channel)
    | TokenIds.tk_trait() => _format_entity(definition, "trait", channel)
    | TokenIds.tk_interface() => _format_entity(definition, "interface", channel)
    | TokenIds.tk_primitive() => _format_entity(definition, "primitive", channel)
    | TokenIds.tk_type() => _format_entity(definition, "type", channel)
    | TokenIds.tk_struct() => _format_entity(definition, "struct", channel)
    | TokenIds.tk_fun() => _format_method(definition, "fun", channel)
    | TokenIds.tk_be() => _format_method(definition, "be", channel)
    | TokenIds.tk_new() => _format_method(definition, "new", channel)
    | TokenIds.tk_flet() => _format_field(definition, "let", channel)
    | TokenIds.tk_fvar() => _format_field(definition, "var", channel)
    | TokenIds.tk_embed() => _format_field(definition, "embed", channel)
    | TokenIds.tk_let() => _format_local_var(definition, "let", channel)
    | TokenIds.tk_var() => _format_local_var(definition, "var", channel)
    | TokenIds.tk_param() => _format_param(definition, channel)
    else
      // Unknown definition type
      None
    end

  fun tag _extract_params(params: AST box, channel: Channel): String =>
    """
    Extract parameter list from params node.
    Returns formatted parameter list like "(x: U32, y: String)"
    """
    if params.id() == TokenIds.tk_params() then
      let param_strs = Array[String]
      for param in params.children() do
        try
          // Each param has structure: child(0) = id, child(1) = type
          let param_id = param(0)?
          if param_id.id() == TokenIds.tk_id() then
            let param_name = param_id.token_value() as String
            let param_type = try
              let ptype = param(1)?
              ": " + _extract_type(ptype, channel)
            else
              ""
            end
            param_strs.push(param_name + param_type)
          end
        end
      end
      "(" + ", ".join(param_strs.values()) + ")"
    else
      "()"
    end

  fun tag _extract_type_params(type_params: AST box, channel: Channel): String =>
    """
    Extract type parameters from typeparams node.
    Returns formatted type params like "[T: Trait, U: Other]"
    """
    let type_param_strs = Array[String]
    for type_param in type_params.children() do
      try
        // Type parameter structure: child(0) = id, child(1) = constraint (optional)
        let tp_id = type_param(0)?
        if tp_id.id() == TokenIds.tk_id() then
          let tp_name = tp_id.token_value() as String
          let constraint = try
            let constraint_node = type_param(1)?
            ": " + _extract_type(constraint_node, channel)
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

  fun tag _extract_type(type_node: AST box, channel: Channel): String =>
    """
    Extract type information from a type node.
    Handles nominal types, union types, tuple types, intersection types, and arrow types
    """
    let node_id = type_node.id()
    match node_id
    | TokenIds.tk_nominal() =>
      // Nominal type - try to get the type name
      // TK_NOMINAL structure: child(0) = package id ($0 for builtin), child(1) = type name,
      // child(2+) = capability (optional), type args (optional)
      try
        let num_children = type_node.num_children()
        if num_children > 1 then
          let type_id = type_node(1)?
          let base_name = if type_id.id() == TokenIds.tk_id() then
            type_id.token_value() as String
          else
            // Try to recursively extract from child
            _extract_type(type_id, channel)
          end

          // Check for type arguments
          let type_args_str = match _find_child_by_type(type_node, TokenIds.tk_typeargs(), 2)
          | let ta: AST box => _extract_typeargs(ta, channel)
          | None => ""
          end

          // Check for capability
          var capability_str: String = ""
          var idx: USize = 2
          while idx < num_children do
            try
              let child = type_node(idx)?
              let cap = _extract_capability(child.id())
              if cap != "" then
                capability_str = cap
                break
              end
            end
            idx = idx + 1
          end

          let cap_str = if capability_str.size() > 0 then " " + capability_str else "" end
          base_name + type_args_str + cap_str
        else
          "?"
        end
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
      // Type parameter reference (like T, U in generic types)
      // Structure: may have child(0) = id or have token value
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
      // Intersection type: (Type1 & Type2 & Type3)
      _extract_composite_type(type_node, " & ", channel)
    | TokenIds.tk_tupletype() =>
      // Tuple type: (Type1, Type2, Type3)
      _extract_composite_type(type_node, ", ", channel)
    | TokenIds.tk_arrow() =>
      // Arrow type: left->right (viewpoint adaptation)
      try
        let left_child = type_node(0)?
        let left_cap = _extract_capability(left_child.id())
        let right = _extract_type(type_node(1)?, channel)
        if left_cap.size() > 0 then
          left_cap + "->" + right
        else
          right
        end
      else
        "?"
      end
    else
      // For other type node types, return a placeholder
      TokenIds.string(node_id)
    end

  fun tag _extract_capability(node_id: I32): String =>
    """
    Extract capability string from a capability token ID.
    Returns capability name like "val" or empty string if not a capability.
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

  fun tag _extract_typeargs(typeargs_node: AST box, channel: Channel): String =>
    """
    Extract type arguments from a tk_typeargs node.
    Returns formatted string like "[T1, T2]" or empty string if no args.
    """
    let arg_strs = Array[String]
    for arg in typeargs_node.children() do
      arg_strs.push(_extract_type(arg, channel))
    end
    if arg_strs.size() > 0 then
      "[" + ", ".join(arg_strs.values()) + "]"
    else
      ""
    end

  fun tag _extract_composite_type(type_node: AST box, separator: String, channel: Channel): String =>
    """
    Extract composite types (union, intersection, tuple) by recursively extracting children.
    """
    let type_strs = Array[String]
    for child in type_node.children() do
      type_strs.push(_extract_type(child, channel))
    end
    if type_strs.size() > 0 then
      "(" + separator.join(type_strs.values()) + ")"
    else
      "?"
    end

  fun tag _wrap_code_block(content: String): String =>
    """
    Wrap content in a Pony code block for markdown formatting.
    """
    "```pony\n" + content + "\n```"

  fun tag _find_child_by_type(ast: AST box, token_id: I32, start_index: USize = 0): (AST box | None) =>
    """
    Search for a child node with a specific token type.
    Returns the first matching child found, or None if not found.
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
