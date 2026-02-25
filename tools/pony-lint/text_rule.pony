trait val TextRule
  """
  A text-based lint rule that checks source file content line by line.

  Rules are stateless primitives that examine a `SourceFile` and produce
  zero or more `Diagnostic` values. Each rule has a unique `id()` in the
  form `category/rule-name` (e.g., `style/line-length`).
  """
  fun id(): String val
    """Unique rule identifier in `category/rule-name` form."""

  fun category(): String val
    """Category prefix of `id()` (e.g., `style`)."""

  fun description(): String val
    """Short human-readable description of what the rule checks."""

  fun default_status(): RuleStatus
    """Whether the rule is enabled or disabled when no config overrides it."""

  fun check(source: SourceFile val): Array[Diagnostic val] val
    """
    Examine a source file and return diagnostics for any violations found.
    Returns an empty array when the file is clean.
    """
