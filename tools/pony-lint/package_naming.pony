use ast = "pony_compiler"

primitive PackageNaming is ASTRule
  """
  Flags package directories whose name does not follow snake_case conventions.

  The check extracts the directory basename from the package path and verifies
  it contains only lowercase letters, digits, and underscores. This rule
  operates at the package level — the linter calls `check_package()` once per
  compiled package rather than per-node.

  Disabled by default: there is no reliable way to distinguish library packages
  (where snake_case is a meaningful convention) from application directories
  (which often use hyphens to match CLI naming). See Discussion #4846.
  """
  fun id(): String val => "style/package-naming"
  fun category(): String val => "style"

  fun description(): String val =>
    "package directory name should be snake_case"

  fun default_status(): RuleStatus => RuleOff

  fun node_filter(): Array[ast.TokenId] val =>
    // Not used for per-node dispatch — this rule uses check_package
    recover val Array[ast.TokenId] end

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    // Per-node check is a no-op; see check_package
    recover val Array[Diagnostic val] end

  fun check_package(dir_name: String val, first_file_rel: String val)
    : Array[Diagnostic val] val
  =>
    """
    Check that the package directory name follows snake_case conventions.
    `dir_name` is the directory basename, `first_file_rel` is the relative
    path of the first file in the package (used for diagnostic location).
    """
    if not NamingHelpers.is_snake_case(dir_name) then
      recover val
        [ Diagnostic(
          id(),
          "package directory '" + dir_name + "' should be snake_case",
          first_file_rel,
          0,
          0)]
      end
    else
      recover val Array[Diagnostic val] end
    end
