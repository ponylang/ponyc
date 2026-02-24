class val RuleRegistry
  """
  Registry of all lint rules, filtered by configuration.

  Construction takes all known rules and a `LintConfig`. Each rule's effective
  status is determined by the config's precedence chain (CLI > file > default).
  """
  let _enabled: Array[TextRule val] val
  let _all: Array[TextRule val] val

  new val create(rules: Array[TextRule val] val, config: LintConfig) =>
    _all = rules
    _enabled = recover val
      let result = Array[TextRule val]
      for rule in rules.values() do
        match config.rule_status(rule.id(), rule.category(),
          rule.default_status())
        | RuleOn => result.push(rule)
        end
      end
      result
    end

  fun enabled_text_rules(): Array[TextRule val] val =>
    """Returns only the rules that are enabled by the current configuration."""
    _enabled

  fun all_text_rules(): Array[TextRule val] val =>
    """Returns all registered rules regardless of configuration."""
    _all
