use ast = "pony_compiler"

primitive ModuleDocstring is ASTRule
  """
  Flags modules that lack a module-level docstring.

  A module docstring lives in a file named after the module (e.g.,
  `my_module.pony` for a module in the `my_module/` directory). The file
  should contain a file-level docstring — a triple-quoted string as the
  first token in the file AST.

  Directory names with hyphens are normalized to underscores when deriving the
  expected filename, since hyphens are not valid in Pony identifiers.

  This rule operates at the module level. The linter calls `check_module()`
  once per compiled module rather than per-node.
  """
  fun id(): String val => "style/module-docstring"
  fun category(): String val => "style"

  fun description(): String val =>
    "module should have a docstring in its <module>.pony file"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    // Not used for per-node dispatch — this rule uses check_module
    recover val Array[ast.TokenId] end

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    // Per-node check is a no-op; see check_module
    recover val Array[Diagnostic val] end

  fun check_module(
    pkg_name: String val,
    has_pkg_file: Bool,
    has_docstring: Bool,
    first_file_rel: String val)
    : Array[Diagnostic val] val
  =>
    """
    Check that a module has its `<name>.pony` file with a file-level
    docstring. `pkg_name` is the normalized module name (hyphens replaced
    with underscores), `has_pkg_file` indicates whether the expected file
    exists, `has_docstring` indicates whether that file has a file-level
    docstring, and `first_file_rel` is used for diagnostic location.
    """
    if not has_pkg_file then
      recover val
        [ Diagnostic(
          id(),
          "module has no '" + pkg_name + ".pony' file",
          first_file_rel,
          0,
          0)]
      end
    elseif not has_docstring then
      recover val
        [ Diagnostic(
          id(),
          "'" + pkg_name + ".pony' should have a module docstring",
          first_file_rel,
          0,
          0)]
      end
    else
      recover val Array[Diagnostic val] end
    end
