primitive ExplainHelpers
  """
  Formatting helpers for the `--explain` CLI option.

  Produces human-readable output describing a lint rule: its ID, default
  status, description, and a link to the full documentation on the website.
  """

  fun format(
    id: String val,
    description: String val,
    default_status: RuleStatus)
    : String val
  =>
    """
    Format a rule's explain output as a multi-line string.

    The output includes the rule ID, whether it is enabled or disabled by
    default, the description, and a URL to the rule reference documentation.
    """
    let status_text: String val =
      match default_status
      | RuleOn => "enabled by default"
      | RuleOff => "disabled by default"
      end

    recover val
      String
        .> append("\n")
        .> append(id)
        .> append(" (")
        .> append(status_text)
        .> append(")\n\n  ")
        .> append(description)
        .> append("\n\n  ")
        .> append(rule_url(id))
        .> append("\n")
    end

  fun rule_url(id: String val): String val =>
    """
    Build the full URL to this rule's documentation on the ponylang website.

    The anchor fragment is the rule ID with `/` characters stripped, matching
    the heading anchor format used by the website (e.g., `style/dot-spacing`
    becomes `#styledot-spacing`).
    """
    let anchor = recover val
      let s = String(id.size())
      for ch in id.values() do
        if ch != '/' then
          s.push(ch)
        end
      end
      s
    end

    recover val
      String
        .> append(
          "https://www.ponylang.io/use/linting/rule-reference/#")
        .> append(anchor)
    end
