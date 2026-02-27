use "collections"

class val Suppressions
  """
  Tracks suppression directives parsed from `// pony-lint:` magic comments in a
  source file. Supports three directive forms:

  - `// pony-lint: off [rule-or-category]` — suppress all subsequent lines
  - `// pony-lint: on [rule-or-category]` — end a suppression region
  - `// pony-lint: allow [rule-or-category]` — suppress the next line only

  When a rule-or-category argument is omitted, the directive applies to all
  rules. Category-level directives (e.g., `style`) apply to all rules in that
  category. A rule-specific `on` can override a category-level `off`.

  `lint/*` diagnostics (suppression errors) cannot themselves be suppressed.
  """
  let _suppressed_lines: Map[USize, Set[String]] val
  let _overridden_rules: Map[USize, Set[String]] val
  let _magic_lines: Set[USize] val
  let _errors: Array[Diagnostic val] val

  new val create(source: SourceFile val) =>
    (let sl, let ov, let ml, let el) = _parse(source)
    _suppressed_lines = sl
    _overridden_rules = ov
    _magic_lines = ml
    _errors = el

  fun tag _parse(source: SourceFile val)
    : ( Map[USize, Set[String]] val
      , Map[USize, Set[String]] val
      , Set[USize] val
      , Array[Diagnostic val] val)
  =>
    recover val
      let suppressed = Map[USize, Set[String]]
      let overrides = Map[USize, Set[String]]
      let magic = Set[USize]
      let errs = Array[Diagnostic val]

      // Currently active off regions: target -> line where opened
      let active_offs = Map[String, USize]
      // Currently active rule-specific overrides within category off
      let active_overrides = Set[String]

      for (idx, line) in source.lines.pairs() do
        let line_num = idx + 1

        // Record active suppressions for this line
        for target in active_offs.keys() do
          if not suppressed.contains(line_num) then
            suppressed(line_num) = Set[String]
          end
          try
            suppressed(line_num)?.set(target)
          else
            _Unreachable()
          end
        end

        // Record active overrides for this line
        for target in active_overrides.values() do
          if not overrides.contains(line_num) then
            overrides(line_num) = Set[String]
          end
          try
            overrides(line_num)?.set(target)
          else
            _Unreachable()
          end
        end

        let trimmed = line.clone()
        trimmed.lstrip()
        if trimmed.at("//pony-lint:") then
          magic.set(line_num)
          errs.push(Diagnostic("lint/malformed-suppression",
            "missing space after '//'",
            source.rel_path, line_num, 1))
        elseif trimmed.at("// pony-lint:") then
          magic.set(line_num)
          let directive = trimmed.substring(
            "// pony-lint:".size().isize())
          let directive_str: String val = consume directive
          let directive_trimmed = directive_str.clone()
          directive_trimmed.strip()
          let parts = (consume directive_trimmed).split_by(" ")
          try
            let action = parts(0)?
            // Find first non-empty part after action (handles
            // consecutive spaces like "off  style")
            var target: String val = "*"
            var pi: USize = 1
            while pi < parts.size() do
              try
                let p = parts(pi)?
                if p.size() > 0 then
                  target = p
                  break
                end
              else
                _Unreachable()
              end
              pi = pi + 1
            end
            if action == "off" then
              // End any active override for this target
              if active_overrides.contains(target) then
                try
                  active_overrides.extract(target)?
                else
                  _Unreachable()
                end
              end
              if active_offs.contains(target) then
                errs.push(Diagnostic("lint/malformed-suppression",
                  "duplicate 'off' for '" + target + "'",
                  source.rel_path, line_num, 1))
              else
                active_offs(target) = line_num
              end
            elseif action == "on" then
              if target == "*" then
                // Wildcard on clears all active offs and overrides
                active_offs.clear()
                active_overrides.clear()
              elseif active_offs.contains(target) then
                try
                  active_offs.remove(target)?
                else
                  _Unreachable()
                end
              else
                // Rule-specific or category on overrides a broader off.
                active_overrides.set(target)
              end
            elseif action == "allow" then
              let next_line = line_num + 1
              if not suppressed.contains(next_line) then
                suppressed(next_line) = Set[String]
              end
              try
                suppressed(next_line)?.set(target)
              else
                _Unreachable()
              end
            else
              errs.push(Diagnostic("lint/malformed-suppression",
                "unknown directive '" + action + "'",
                source.rel_path, line_num, 1))
            end
          else
            errs.push(Diagnostic("lint/malformed-suppression",
              "empty directive",
              source.rel_path, line_num, 1))
          end
        end
      end

      // Report unclosed regions
      for (target, start_line) in active_offs.pairs() do
        errs.push(Diagnostic("lint/unclosed-suppression",
          "suppression for '" + target + "' opened at line "
            + start_line.string() + " was never closed",
          source.rel_path, start_line, 1))
      end

      (suppressed, overrides, magic, errs)
    end

  fun is_suppressed(line_num: USize, rule_id: String): Bool =>
    """
    Returns true if the given rule is suppressed at the given line number.
    `lint/*` rules are never suppressed.
    """
    if rule_id.at("lint/") then
      return false
    end
    // A per-line override means the rule was explicitly re-enabled here.
    // Check wildcard, category, and rule-specific overrides — mirroring
    // the same matching logic used for suppressions below.
    try
      let ov = _overridden_rules(line_num)?
      if ov.contains("*") then
        return false
      end
      if ov.contains(rule_id) then
        return false
      end
      try
        let slash_pos = rule_id.find("/")?
        let category = rule_id.substring(0, slash_pos)
        if ov.contains(consume category) then
          return false
        end
      end
    end
    try
      let targets = _suppressed_lines(line_num)?
      if targets.contains("*") then
        return true
      end
      if targets.contains(rule_id) then
        return true
      end
      // Check category match: rule_id "style/line-length" matches "style"
      try
        let slash_pos = rule_id.find("/")?
        let category = rule_id.substring(0, slash_pos)
        if targets.contains(consume category) then
          return true
        end
      end
      false
    else
      false
    end

  fun magic_comment_lines(): Set[USize] val =>
    """
    Returns the set of line numbers that contain pony-lint directives.
    """
    _magic_lines

  fun errors(): Array[Diagnostic val] val =>
    """
    Returns any lint/* diagnostics generated during suppression parsing
    (unclosed regions, malformed directives, etc.).
    """
    _errors
