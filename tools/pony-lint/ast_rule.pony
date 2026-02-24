use ast = "ast"

trait val ASTRule
  """
  An AST-based lint rule that checks parsed syntax tree nodes.

  Unlike `TextRule` which examines raw source text, AST rules operate on the
  parsed syntax tree produced by pony-ast. Each rule declares which token types
  it is interested in via `node_filter()`, and the dispatcher only calls
  `check()` for matching nodes.

  Rules are stateless primitives. Each receives a single AST node and the
  corresponding `SourceFile`, returning diagnostics without side effects.
  """
  fun id(): String val
    """Unique rule identifier in `category/rule-name` form."""

  fun category(): String val
    """Category prefix of `id()` (e.g., `style`)."""

  fun description(): String val
    """Short human-readable description of what the rule checks."""

  fun default_status(): RuleStatus
    """Whether the rule is enabled or disabled when no config overrides it."""

  fun node_filter(): Array[ast.TokenId] val
    """
    Token types this rule wants to inspect. The dispatcher only calls `check()`
    for nodes whose `id()` appears in this list.
    """

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
    """
    Examine an AST node and return diagnostics for any violations found.
    Returns an empty array when the node is clean.
    """
