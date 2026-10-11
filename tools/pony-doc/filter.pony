use ast = "pony_compiler"

primitive Filter
  """
  Filtering predicates for modules, entities, and names.
  """

  fun is_test_module(name: String box): Bool =>
    """
    Returns true for test modules that should be excluded.
    """
    (name == "test") or (name == "builtin_test")

  fun is_private(name: String box): Bool =>
    """
    Returns true if the name has a leading underscore.
    """
    (name.size() > 0) and (try name(0)? == '_' else false end)

  fun is_internal(name: String box): Bool =>
    """
    Returns true for compiler-generated hygienic names
    (`$` prefix).
    """
    (name.size() > 0) and (try name(0)? == '$' else false end)

  fun has_nodoc(node: ast.AST box): Bool =>
    """
    Returns true if the AST node has a `\\nodoc\\`
    annotation.
    """
    node.has_annotation("nodoc")
