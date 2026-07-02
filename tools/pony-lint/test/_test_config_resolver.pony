use "pony_test"
use "collections"
use "files"
use lint = ".."

class \nodoc\ _TestConfigResolverNoOverrides is UnitTest
  """Returns root config unchanged when no overrides exist."""
  fun name(): String => "ConfigResolver: no overrides returns root config"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?
      let sub = Path.join(tmp.path, "sub")

      let config = lint.LintConfig.default()
      let resolver = lint.ConfigResolver(config, tmp.path)
      let result = resolver.config_for(sub)
      h.assert_true(result is config)

      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestConfigResolverSingleOverride is UnitTest
  """Subdirectory override applies to that directory."""
  fun name(): String => "ConfigResolver: single override applies"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?
      let examples = Path.join(tmp.path, "examples")

      let root_rules =
        recover val
          let m = Map[String, lint.RuleStatus]
          m("style/line-length") = lint.RuleOn
          m
        end
      let root_config =
        lint.LintConfig(recover val Set[String] end, root_rules)
      let resolver = lint.ConfigResolver(root_config, tmp.path)

      let child_rules =
        recover val
          let m = Map[String, lint.RuleStatus]
          m("style/line-length") = lint.RuleOff
          m
        end
      resolver.add_directory(examples, child_rules)

      let result = resolver.config_for(examples)
      // Should NOT be the same object as root
      h.assert_false(result is root_config)
      match \exhaustive\
        result.rule_status("style/line-length", "style", lint.RuleOn)
      | lint.RuleOff => h.assert_true(true)
      | lint.RuleOn => h.fail("expected RuleOff in subdirectory")
      end

      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestConfigResolverNestedOverrides is UnitTest
  """Nearest ancestor override wins."""
  fun name(): String => "ConfigResolver: nearest ancestor wins"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?
      let examples = Path.join(tmp.path, "examples")
      let sub = Path.join(examples, "sub")

      let root_config = lint.LintConfig.default()
      let resolver = lint.ConfigResolver(root_config, tmp.path)

      // Parent turns off style
      let parent_rules =
        recover val
          let m = Map[String, lint.RuleStatus]
          m("style") = lint.RuleOff
          m
        end
      resolver.add_directory(examples, parent_rules)

      // Child turns line-length back on
      let child_rules =
        recover val
          let m = Map[String, lint.RuleStatus]
          m("style/line-length") = lint.RuleOn
          m
        end
      resolver.add_directory(sub, child_rules)

      let result = resolver.config_for(sub)
      // line-length should be on (child override)
      match \exhaustive\
        result.rule_status("style/line-length", "style", lint.RuleOn)
      | lint.RuleOn => h.assert_true(true)
      | lint.RuleOff => h.fail("expected RuleOn from child override")
      end
      // Other style rules should be off (parent category override)
      match \exhaustive\
        result.rule_status(
          "style/trailing-whitespace", "style", lint.RuleOn)
      | lint.RuleOff => h.assert_true(true)
      | lint.RuleOn => h.fail("expected RuleOff from parent category")
      end

      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestConfigResolverCaching is UnitTest
  """Second call returns same object (identity check)."""
  fun name(): String => "ConfigResolver: caching returns same object"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?
      let sub = Path.join(tmp.path, "sub")

      let config = lint.LintConfig.default()
      let resolver = lint.ConfigResolver(config, tmp.path)

      let child_rules =
        recover val
          let m = Map[String, lint.RuleStatus]
          m("style") = lint.RuleOff
          m
        end
      resolver.add_directory(sub, child_rules)

      let first = resolver.config_for(sub)
      let second = resolver.config_for(sub)
      h.assert_true(first is second)

      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestConfigResolverRootSkipped is UnitTest
  """add_directory for root dir is a no-op."""
  fun name(): String => "ConfigResolver: root directory skipped"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?

      let config = lint.LintConfig.default()
      let resolver = lint.ConfigResolver(config, tmp.path)

      // Try to add an override for the root — should be ignored
      let root_override =
        recover val
          let m = Map[String, lint.RuleStatus]
          m("style") = lint.RuleOff
          m
        end
      resolver.add_directory(tmp.path, root_override)

      let result = resolver.config_for(tmp.path)
      // Should still be the original root config
      h.assert_true(result is config)

      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestConfigResolverDuplicateSkipped is UnitTest
  """add_directory for same dir twice is idempotent."""
  fun name(): String => "ConfigResolver: duplicate add is idempotent"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?
      let sub = Path.join(tmp.path, "sub")

      let root_config = lint.LintConfig.default()
      let resolver = lint.ConfigResolver(root_config, tmp.path)

      let first_rules =
        recover val
          let m = Map[String, lint.RuleStatus]
          m("style/line-length") = lint.RuleOff
          m
        end
      resolver.add_directory(sub, first_rules)

      // Second add with different data — should be ignored
      let second_rules =
        recover val
          let m = Map[String, lint.RuleStatus]
          m("style/line-length") = lint.RuleOn
          m
        end
      resolver.add_directory(sub, second_rules)

      let result = resolver.config_for(sub)
      // Should use first_rules (line-length off)
      match \exhaustive\
        result.rule_status("style/line-length", "style", lint.RuleOn)
      | lint.RuleOff => h.assert_true(true)
      | lint.RuleOn => h.fail("expected first add to win")
      end

      tmp.remove()
    else
      h.fail("could not create temp directory")
    end
