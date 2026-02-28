use ast = "pony_compiler"

primitive PackageDocstring is ASTRule
  """
  Flags packages that lack a package-level docstring.

  A package docstring lives in a file named after the package (e.g.,
  `my_package.pony` for a package in the `my_package/` directory). The file
  should contain a module-level docstring — a triple-quoted string as the
  first token in the module AST.

  Directory names with hyphens are normalized to underscores when deriving the
  expected filename, since hyphens are not valid in Pony identifiers.

  This rule operates at the package level. The linter calls `check_package()`
  once per compiled package rather than per-node.
  """
  fun id(): String val => "style/package-docstring"
  fun category(): String val => "style"

  fun description(): String val =>
    "package should have a docstring in its <package>.pony file"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    // Not used for per-node dispatch — this rule uses check_package
    recover val Array[ast.TokenId] end

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    // Per-node check is a no-op; see check_package
    recover val Array[Diagnostic val] end

  fun check_package(
    pkg_name: String val,
    has_pkg_file: Bool,
    has_docstring: Bool,
    first_file_rel: String val)
    : Array[Diagnostic val] val
  =>
    """
    Check that a package has its `<name>.pony` file with a module-level
    docstring. `pkg_name` is the normalized package name (hyphens replaced
    with underscores), `has_pkg_file` indicates whether the expected file
    exists, `has_docstring` indicates whether that file has a module-level
    docstring, and `first_file_rel` is used for diagnostic location.
    """
    if not has_pkg_file then
      recover val
        [ Diagnostic(
          id(),
          "package has no '" + pkg_name + ".pony' file",
          first_file_rel,
          0,
          0)]
      end
    elseif not has_docstring then
      recover val
        [ Diagnostic(
          id(),
          "'" + pkg_name + ".pony' should have a package docstring",
          first_file_rel,
          0,
          0)]
      end
    else
      recover val Array[Diagnostic val] end
    end
