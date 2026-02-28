class val RuleRegistry
  """
  Registry of all lint rules, filtered by configuration.

  Construction takes all known text rules, AST rules, and a `LintConfig`. Each
  rule's effective status is determined by the config's precedence chain
  (CLI > file > default).
  """
  let _enabled: Array[TextRule val] val
  let _all: Array[TextRule val] val
  let _enabled_ast: Array[ASTRule val] val
  let _all_ast: Array[ASTRule val] val
  let _config: LintConfig

  new val create(
    rules: Array[TextRule val] val,
    ast_rules: Array[ASTRule val] val,
    config: LintConfig)
  =>
    _all = rules
    _all_ast = ast_rules
    _config = config
    _enabled =
      recover val
        let result = Array[TextRule val]
        for rule in rules.values() do
          match config.rule_status(
            rule.id(),
            rule.category(),
            rule.default_status())
          | RuleOn => result.push(rule)
          end
        end
        result
      end
    _enabled_ast =
      recover val
        let result = Array[ASTRule val]
        for rule in ast_rules.values() do
          match config.rule_status(
            rule.id(),
            rule.category(),
            rule.default_status())
          | RuleOn => result.push(rule)
          end
        end
        result
      end

  fun enabled_text_rules(): Array[TextRule val] val =>
    """
    Returns only the text rules that are enabled by configuration.
    """
    _enabled

  fun all_text_rules(): Array[TextRule val] val =>
    """
    Returns all registered text rules regardless of configuration.
    """
    _all

  fun enabled_ast_rules(): Array[ASTRule val] val =>
    """
    Returns only the AST rules that are enabled by configuration.
    """
    _enabled_ast

  fun all_ast_rules(): Array[ASTRule val] val =>
    """
    Returns all registered AST rules regardless of configuration.
    """
    _all_ast

  fun is_enabled(rule_id: String, category: String, default: RuleStatus)
    : Bool
  =>
    """
    Check if a rule is enabled by configuration. Used for module-level and
    package-level rules that are checked outside the per-node dispatch.
    """
    match _config.rule_status(rule_id, category, default)
    | RuleOn => true
    else false
    end
