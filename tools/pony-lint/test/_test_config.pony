use "pony_test"
use "collections"
use "files"
use lint = ".."

class \nodoc\ _TestConfigDefaultAllEnabled is UnitTest
  """Default config returns default status for all rules."""
  fun name(): String => "Config: default returns rule defaults"

  fun apply(h: TestHelper) =>
    let config = lint.LintConfig.default()
    match \exhaustive\
      config.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOn => h.assert_true(true)
    | lint.RuleOff => h.fail("expected RuleOn")
    end

class \nodoc\ _TestConfigCLIDisableRule is UnitTest
  """CLI disable always produces RuleOff."""
  fun name(): String => "Config: CLI disable overrides everything"

  fun apply(h: TestHelper) =>
    let disabled =
      recover val Set[String] .> set("style/line-length") end
    let file_rules = recover val Map[String, lint.RuleStatus] end
    let config = lint.LintConfig(disabled, file_rules)
    match \exhaustive\
      config.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOff => h.assert_true(true)
    | lint.RuleOn => h.fail("expected RuleOff")
    end

class \nodoc\ _TestConfigCLIDisableCategory is UnitTest
  """CLI disable by category disables all rules in that category."""
  fun name(): String => "Config: CLI disable category"

  fun apply(h: TestHelper) =>
    let disabled =
      recover val Set[String] .> set("style") end
    let file_rules = recover val Map[String, lint.RuleStatus] end
    let config = lint.LintConfig(disabled, file_rules)
    match \exhaustive\
      config.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOff => h.assert_true(true)
    | lint.RuleOn => h.fail("expected RuleOff")
    end

class \nodoc\ _TestConfigFileRuleOverride is UnitTest
  """File config rule-specific override takes effect."""
  fun name(): String => "Config: file rule-specific override"

  fun apply(h: TestHelper) =>
    let disabled = recover val Set[String] end
    let file_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style/line-length") = lint.RuleOff
        m
      end
    let config = lint.LintConfig(disabled, file_rules)
    match \exhaustive\
      config.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOff => h.assert_true(true)
    | lint.RuleOn => h.fail("expected RuleOff")
    end

class \nodoc\ _TestConfigFileCategoryDefault is UnitTest
  """File config category setting applies to rules in that category."""
  fun name(): String => "Config: file category default"

  fun apply(h: TestHelper) =>
    let disabled = recover val Set[String] end
    let file_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style") = lint.RuleOff
        m
      end
    let config = lint.LintConfig(disabled, file_rules)
    match \exhaustive\
      config.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOff => h.assert_true(true)
    | lint.RuleOn => h.fail("expected RuleOff")
    end

class \nodoc\ _TestConfigCLIOverridesFile is UnitTest
  """CLI disable overrides file config that enables a rule."""
  fun name(): String => "Config: CLI overrides file config"

  fun apply(h: TestHelper) =>
    let disabled =
      recover val Set[String] .> set("style/line-length") end
    let file_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style/line-length") = lint.RuleOn
        m
      end
    let config = lint.LintConfig(disabled, file_rules)
    match \exhaustive\
      config.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOff => h.assert_true(true)
    | lint.RuleOn => h.fail("expected RuleOff")
    end

class \nodoc\ _TestConfigParseValidJSON is UnitTest
  """Valid JSON config parses correctly."""
  fun name(): String => "Config: parse valid JSON"

  fun apply(h: TestHelper) =>
    match \exhaustive\ lint.ConfigLoader.parse(
      """{"rules": {"style/line-length": "off", "style": "on"}}""")
    | let rules: Map[String, lint.RuleStatus] val =>
      h.assert_eq[USize](2, rules.size())
    | let err: lint.ConfigError =>
      h.fail("unexpected error: " + err.message)
    end

class \nodoc\ _TestConfigParseMalformedJSON is UnitTest
  """Malformed JSON produces ConfigError."""
  fun name(): String => "Config: malformed JSON -> error"

  fun apply(h: TestHelper) =>
    match \exhaustive\ lint.ConfigLoader.parse("{not valid json}")
    | let rules: Map[String, lint.RuleStatus] val =>
      h.fail("expected ConfigError")
    | let err: lint.ConfigError =>
      h.assert_true(err.message.contains("malformed JSON"))
    end

class \nodoc\ _TestConfigParseInvalidStatus is UnitTest
  """Invalid rule status value produces ConfigError."""
  fun name(): String => "Config: invalid status value -> error"

  fun apply(h: TestHelper) =>
    match \exhaustive\ lint.ConfigLoader.parse(
      """{"rules": {"style/line-length": "maybe"}}""")
    | let rules: Map[String, lint.RuleStatus] val =>
      h.fail("expected ConfigError")
    | let err: lint.ConfigError =>
      h.assert_true(err.message.contains("invalid rule status"))
    end

class \nodoc\ _TestConfigParseNoRules is UnitTest
  """JSON with no rules key returns empty map."""
  fun name(): String => "Config: no rules key -> empty"

  fun apply(h: TestHelper) =>
    match \exhaustive\ lint.ConfigLoader.parse("{}")
    | let rules: Map[String, lint.RuleStatus] val =>
      h.assert_eq[USize](0, rules.size())
    | let err: lint.ConfigError =>
      h.fail("unexpected error: " + err.message)
    end

class \nodoc\ _TestConfigMergeRuleOverride is UnitTest
  """Child rule-specific entry overrides parent."""
  fun name(): String => "Config: merge rule override"

  fun apply(h: TestHelper) =>
    let parent_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style/line-length") = lint.RuleOn
        m
      end
    let parent =
      lint.LintConfig(recover val Set[String] end, parent_rules)
    let child_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style/line-length") = lint.RuleOff
        m
      end
    let merged = lint.LintConfig.merge(parent, child_rules)
    match \exhaustive\
      merged.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOff => h.assert_true(true)
    | lint.RuleOn => h.fail("expected child override to win")
    end

class \nodoc\ _TestConfigMergeCategoryCleaning is UnitTest
  """Child category entry removes parent rule-specific entries."""
  fun name(): String => "Config: merge category cleaning"

  fun apply(h: TestHelper) =>
    let parent_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style/line-length") = lint.RuleOn
        m("style/hard-tabs") = lint.RuleOn
        m
      end
    let parent =
      lint.LintConfig(recover val Set[String] end, parent_rules)
    let child_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style") = lint.RuleOff
        m
      end
    let merged = lint.LintConfig.merge(parent, child_rules)
    // Both style rules should now be off
    match \exhaustive\
      merged.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOff => h.assert_true(true)
    | lint.RuleOn => h.fail("expected line-length off after category clean")
    end
    match \exhaustive\
      merged.rule_status("style/hard-tabs", "style", lint.RuleOn)
    | lint.RuleOff => h.assert_true(true)
    | lint.RuleOn => h.fail("expected hard-tabs off after category clean")
    end

class \nodoc\ _TestConfigMergeOmissionDefers is UnitTest
  """Omitted rule in child defers to parent."""
  fun name(): String => "Config: merge omission defers to parent"

  fun apply(h: TestHelper) =>
    let parent_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style/line-length") = lint.RuleOff
        m("style/hard-tabs") = lint.RuleOn
        m
      end
    let parent =
      lint.LintConfig(recover val Set[String] end, parent_rules)
    // Child only mentions hard-tabs
    let child_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style/hard-tabs") = lint.RuleOff
        m
      end
    let merged = lint.LintConfig.merge(parent, child_rules)
    // line-length should still be off (from parent)
    match \exhaustive\
      merged.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOff => h.assert_true(true)
    | lint.RuleOn => h.fail("expected parent setting to carry forward")
    end

class \nodoc\ _TestConfigMergeCLICarriesForward is UnitTest
  """CLI-disabled rules are unaffected by merge."""
  fun name(): String => "Config: merge CLI carries forward"

  fun apply(h: TestHelper) =>
    let parent =
      lint.LintConfig(
        recover val Set[String] .> set("style/line-length") end,
        recover val Map[String, lint.RuleStatus] end)
    let child_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style/line-length") = lint.RuleOn
        m
      end
    let merged = lint.LintConfig.merge(parent, child_rules)
    // CLI disable should win over child override
    match \exhaustive\
      merged.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOff => h.assert_true(true)
    | lint.RuleOn => h.fail("expected CLI disable to win")
    end

class \nodoc\ _TestConfigMergeRuleSpecificNoCleanCategory is UnitTest
  """Child rule-specific entry does NOT clean parent category entry."""
  fun name(): String =>
    "Config: merge rule-specific does not clean category"

  fun apply(h: TestHelper) =>
    let parent_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style") = lint.RuleOn
        m
      end
    let parent =
      lint.LintConfig(recover val Set[String] end, parent_rules)
    let child_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style/line-length") = lint.RuleOff
        m
      end
    let merged = lint.LintConfig.merge(parent, child_rules)
    // line-length overridden by child
    match \exhaustive\
      merged.rule_status("style/line-length", "style", lint.RuleOn)
    | lint.RuleOff => h.assert_true(true)
    | lint.RuleOn => h.fail("expected child rule to override")
    end
    // Other style rules still governed by parent category "on"
    match \exhaustive\
      merged.rule_status("style/hard-tabs", "style", lint.RuleOff)
    | lint.RuleOn => h.assert_true(true)
    | lint.RuleOff => h.fail("expected parent category to still apply")
    end

class \nodoc\ _TestConfigFromCLIExplicitConfigRootDir is UnitTest
  """Explicit --config returns the config file's parent as root dir."""
  fun name(): String => "Config: from_cli explicit config root dir"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?
      let sub_dir =
        FilePath(FileAuth(auth), Path.join(tmp.path, "sub"))
      sub_dir.mkdir()
      let config_fp =
        FilePath(
          FileAuth(auth), Path.join(sub_dir.path, ".pony-lint.json"))
      let f = File(config_fp)
      f.print("""{"rules": {"style/line-length": "off"}}""")
      f.dispose()

      let no_disabled = recover val Array[String] end
      match \exhaustive\
        lint.ConfigLoader.from_cli(
          no_disabled, config_fp.path, FileAuth(auth))
      | (let _: lint.LintConfig, let root_dir: String val) =>
        // Root dir should be the config file's parent directory
        h.assert_eq[String](sub_dir.path, root_dir)
      | let err: lint.ConfigError =>
        h.fail("unexpected error: " + err.message)
      end

      config_fp.remove()
      sub_dir.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestConfigFromCLIExplicitConfigNotFound is UnitTest
  """Explicit --config with non-existent file returns ConfigError."""
  fun name(): String => "Config: from_cli explicit config not found"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    let no_disabled = recover val Array[String] end
    match \exhaustive\
      lint.ConfigLoader.from_cli(
        no_disabled, "/no/such/config.json", FileAuth(auth))
    | (let _: lint.LintConfig, let _: String val) =>
      h.fail("expected ConfigError")
    | let err: lint.ConfigError =>
      h.assert_true(err.message.contains("not found"))
    end

class \nodoc\ _TestConfigFromCLIAutoDiscoverRootDir is UnitTest
  """Auto-discovery with no config returns CWD as root dir."""
  fun name(): String => "Config: from_cli auto-discover root dir"

  fun apply(h: TestHelper) =>
    // This test runs from a directory that (almost certainly) has no
    // .pony-lint.json in CWD. If the ponyc repo's root .pony-lint.json
    // is found via corral.json, the root dir will be the corral.json
    // directory. Either way, we verify the return type is a tuple with
    // a non-empty root dir string.
    let auth = h.env.root
    let no_disabled = recover val Array[String] end
    match \exhaustive\
      lint.ConfigLoader.from_cli(no_disabled, None, FileAuth(auth))
    | (let _: lint.LintConfig, let root_dir: String val) =>
      h.assert_true(root_dir.size() > 0)
    | let err: lint.ConfigError =>
      h.fail("unexpected error: " + err.message)
    end

class \nodoc\ _TestConfigValidateKnownKeys is UnitTest
  """Validate passes when all config keys are known rule IDs or categories."""
  fun name(): String => "Config: validate passes for known keys"

  fun apply(h: TestHelper) =>
    let disabled = recover val Set[String] end
    let file_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style/line-length") = lint.RuleOff
        m("style") = lint.RuleOn
        m
      end
    let config = lint.LintConfig(disabled, file_rules)
    let known =
      recover val
        Set[String]
          .> set("style/line-length")
          .> set("style/trailing-whitespace")
          .> set("style")
      end
    match config.validate(known)
    | let err: lint.ConfigError =>
      h.fail("unexpected error: " + err.message)
    end

class \nodoc\ _TestConfigValidateUnknownRule is UnitTest
  """Validate fails when config has an unrecognized rule ID."""
  fun name(): String => "Config: validate fails for unknown rule"

  fun apply(h: TestHelper) =>
    let disabled = recover val Set[String] end
    let file_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("stlye/line-length") = lint.RuleOff
        m
      end
    let config = lint.LintConfig(disabled, file_rules)
    let known =
      recover val
        Set[String]
          .> set("style/line-length")
          .> set("style")
      end
    match \exhaustive\ config.validate(known)
    | None => h.fail("expected ConfigError")
    | let err: lint.ConfigError =>
      h.assert_true(err.message.contains("stlye/line-length"))
    end

class \nodoc\ _TestConfigValidateUnknownCategory is UnitTest
  """Validate fails when config has an unrecognized category."""
  fun name(): String => "Config: validate fails for unknown category"

  fun apply(h: TestHelper) =>
    let disabled = recover val Set[String] end
    let file_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("stlye") = lint.RuleOff
        m
      end
    let config = lint.LintConfig(disabled, file_rules)
    let known =
      recover val
        Set[String]
          .> set("style/line-length")
          .> set("style")
      end
    match \exhaustive\ config.validate(known)
    | None => h.fail("expected ConfigError")
    | let err: lint.ConfigError =>
      h.assert_true(err.message.contains("stlye"))
    end

class \nodoc\ _TestConfigValidateEmptyConfig is UnitTest
  """Validate passes when config has no file rules."""
  fun name(): String => "Config: validate passes for empty config"

  fun apply(h: TestHelper) =>
    let config = lint.LintConfig.default()
    let known =
      recover val Set[String] .> set("style/line-length") end
    match config.validate(known)
    | let err: lint.ConfigError =>
      h.fail("unexpected error: " + err.message)
    end

class \nodoc\ _TestConfigValidateMultipleUnknown is UnitTest
  """Validate reports all unrecognized keys, not just the first."""
  fun name(): String => "Config: validate reports multiple unknown keys"

  fun apply(h: TestHelper) =>
    let disabled = recover val Set[String] end
    let file_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("bogus/one") = lint.RuleOff
        m("bogus/two") = lint.RuleOff
        m
      end
    let config = lint.LintConfig(disabled, file_rules)
    let known =
      recover val
        Set[String]
          .> set("style/line-length")
          .> set("style")
      end
    match \exhaustive\ config.validate(known)
    | None => h.fail("expected ConfigError")
    | let err: lint.ConfigError =>
      h.assert_true(err.message.contains("bogus/one"))
      h.assert_true(err.message.contains("bogus/two"))
    end

class \nodoc\ _TestConfigValidateMixedKnownUnknown is UnitTest
  """Validate only reports unknown keys, not known ones."""
  fun name(): String => "Config: validate reports only unknown keys"

  fun apply(h: TestHelper) =>
    let disabled = recover val Set[String] end
    let file_rules =
      recover val
        let m = Map[String, lint.RuleStatus]
        m("style/line-length") = lint.RuleOff
        m("bogus/rule") = lint.RuleOn
        m
      end
    let config = lint.LintConfig(disabled, file_rules)
    let known =
      recover val
        Set[String]
          .> set("style/line-length")
          .> set("style")
      end
    match \exhaustive\ config.validate(known)
    | None => h.fail("expected ConfigError")
    | let err: lint.ConfigError =>
      h.assert_true(err.message.contains("bogus/rule"))
      h.assert_false(err.message.contains("style/line-length"))
    end
