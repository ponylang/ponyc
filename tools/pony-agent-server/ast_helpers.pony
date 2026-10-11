use "files"
use "pony_compiler"

primitive ASTHelpers
  """
  Walk AST parent chains, look up type definitions by name, and
  determine entity default capabilities.
  """

  fun find_module(program: Program val, file: String val)
    : (Module val | None)
  =>
    """
    Find the module whose source path matches `file`. Normalizes
    the query path to an absolute path before comparing.
    """
    let abs_file = Path.abs(file)
    for pkg in program.packages() do
      for mod in pkg.modules() do
        if mod.file == abs_file then
          return mod
        end
      end
    end

  fun find_enclosing_recover(node: AST box): (AST box | None) =>
    """
    Walk up the parent chain to find the nearest TK_RECOVER. Stops
    at method or type boundaries.
    """
    var current: (AST box | None) = node.parent()
    repeat
      match \exhaustive\ current
      | let ast: AST box =>
        match ast.id()
        | TokenIds.tk_recover() => return ast
        | TokenIds.tk_fun()
        | TokenIds.tk_be()
        | TokenIds.tk_new()
        | TokenIds.tk_class()
        | TokenIds.tk_actor()
        | TokenIds.tk_primitive()
        | TokenIds.tk_struct()
        | TokenIds.tk_trait()
        | TokenIds.tk_interface() =>
          return None
        end
        current = ast.parent()
      | None =>
        return None
      end
    until current is None end
    None

  fun is_inside(node: AST box, ancestor: AST box): Bool =>
    """
    True when `node` is a descendant of `ancestor`.
    """
    var current: (AST box | None) = node.parent()
    repeat
      match \exhaustive\ current
      | let ast: AST box =>
        if ast == ancestor then return true end
        current = ast.parent()
      | None =>
        return false
      end
    until current is None end
    false

  fun find_enclosing_method(node: AST box): (AST box | None) =>
    """
    Walk up the parent chain to find the nearest TK_FUN, TK_BE, or TK_NEW.
    """
    var current: (AST box | None) = node.parent()
    repeat
      match \exhaustive\ current
      | let ast: AST box =>
        match ast.id()
        | TokenIds.tk_fun()
        | TokenIds.tk_be()
        | TokenIds.tk_new() =>
          return ast
        end
        current = ast.parent()
      | None =>
        return None
      end
    until current is None end
    None

  fun find_enclosing_type(node: AST box): (AST box | None) =>
    """
    Walk up the parent chain to find the nearest type definition
    (TK_CLASS, TK_ACTOR, TK_PRIMITIVE, TK_STRUCT, TK_TRAIT, or
    TK_INTERFACE).
    """
    var current: (AST box | None) = node.parent()
    repeat
      match \exhaustive\ current
      | let ast: AST box =>
        match ast.id()
        | TokenIds.tk_class()
        | TokenIds.tk_actor()
        | TokenIds.tk_primitive()
        | TokenIds.tk_struct()
        | TokenIds.tk_trait()
        | TokenIds.tk_interface() =>
          return ast
        end
        current = ast.parent()
      | None =>
        return None
      end
    until current is None end
    None

  fun in_recover_block(node: AST box): Bool =>
    """
    Walk up the parent chain looking for TK_RECOVER. Stops at method
    or type boundaries.
    """
    var current: (AST box | None) = node.parent()
    repeat
      match \exhaustive\ current
      | let ast: AST box =>
        match ast.id()
        | TokenIds.tk_recover() => return true
        | TokenIds.tk_fun()
        | TokenIds.tk_be()
        | TokenIds.tk_new()
        | TokenIds.tk_class()
        | TokenIds.tk_actor()
        | TokenIds.tk_primitive()
        | TokenIds.tk_struct()
        | TokenIds.tk_trait()
        | TokenIds.tk_interface() =>
          return false
        end
        current = ast.parent()
      | None =>
        return false
      end
    until current is None end
    false

  fun find_type_def(program: Program val, type_name: String val)
    : (AST box | String val | None)
  =>
    """
    Find an entity definition by unqualified name across all packages.
    Returns the AST node, an error string when the name is ambiguous,
    or None when nothing matches.
    """
    var found: (AST box | None) = None
    for pkg in program.packages() do
      for mod in pkg.modules() do
        var child: (AST box | None) = mod.ast.child()
        while true do
          match \exhaustive\ child
          | let node: AST box =>
            if TokenIds.is_entity(node.id()) then
              try
                let n = node(0)?.token_value() as String val
                if n == type_name then
                  match \exhaustive\ found
                  | let _: AST box =>
                    return "ambiguous type name: " + type_name +
                      " (found in multiple packages)"
                  | None =>
                    found = node
                  end
                end
              end
            end
            child = node.sibling()
          | None =>
            break
          end
        end
      end
    end
    found

  fun default_cap(def: AST box): TokenId =>
    """
    The default capability for an entity: tag for actors, val for
    primitives, ref for everything else.
    """
    match def.id()
    | TokenIds.tk_actor() => TokenIds.tk_tag()
    | TokenIds.tk_primitive() => TokenIds.tk_val()
    else TokenIds.tk_ref()
    end
