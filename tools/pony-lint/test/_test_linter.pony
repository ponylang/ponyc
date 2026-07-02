use "pony_test"
use "collections"
use "files"
use lint = ".."

primitive \nodoc\ _NoASTRules
  """Empty AST rules array for text-only linter tests."""
  fun apply(): Array[lint.ASTRule val] val =>
    recover val Array[lint.ASTRule val] end

class \nodoc\ _TestLinterSingleFile is UnitTest
  """Linting a single file with a violation produces diagnostics."""
  fun name(): String => "Linter: single file with violation"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?
      let pony_file =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "test.pony"))
      let file = File(pony_file)
      let long_line = recover val String .> append("a".mul(100)) end
      file.print(long_line)
      file.dispose()

      let rules: Array[lint.TextRule val] val =
        recover val [as lint.TextRule val: lint.LineLength] end
      let registry =
        lint.RuleRegistry(
          rules, _NoASTRules(), lint.LintConfig.default())
      let linter = lint.Linter(registry, FileAuth(auth), tmp.path)
      let targets = recover val [as String val: tmp.path] end
      (let diags, let exit_code) = linter.run(targets)
      h.assert_true(diags.size() > 0)
      match exit_code
      | lint.ExitViolations => h.assert_true(true)
      else
        h.fail("expected ExitViolations")
      end

      // Cleanup
      pony_file.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestLinterCleanFile is UnitTest
  """Linting a clean file produces no diagnostics."""
  fun name(): String => "Linter: clean file -> no diagnostics"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?
      let pony_file =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "test.pony"))
      let file = File(pony_file)
      file.print("actor Main")
      file.print("  new create(env: Env) =>")
      file.print("    None")
      file.dispose()

      let rules: Array[lint.TextRule val] val =
        recover val
          Array[lint.TextRule val]
            .> push(lint.LineLength)
            .> push(lint.TrailingWhitespace)
            .> push(lint.HardTabs)
            .> push(lint.CommentSpacing)
        end
      let registry =
        lint.RuleRegistry(
          rules, _NoASTRules(), lint.LintConfig.default())
      let linter = lint.Linter(registry, FileAuth(auth), tmp.path)
      let targets = recover val [as String val: tmp.path] end
      (let diags, let exit_code) = linter.run(targets)
      h.assert_eq[USize](0, diags.size())
      match exit_code
      | lint.ExitSuccess => h.assert_true(true)
      else
        h.fail("expected ExitSuccess")
      end

      pony_file.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestLinterSkipsCorral is UnitTest
  """Files in _corral/ are not discovered."""
  fun name(): String => "Linter: skips _corral directory"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?
      // Create _corral subdirectory with a .pony file
      let corral_dir =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "_corral"))
      corral_dir.mkdir()
      let corral_file =
        FilePath(
          FileAuth(auth), Path.join(corral_dir.path, "dep.pony"))
      let file = File(corral_file)
      let long_line = recover val String .> append("a".mul(100)) end
      file.print(long_line)
      file.dispose()

      let rules: Array[lint.TextRule val] val =
        recover val [as lint.TextRule val: lint.LineLength] end
      let registry =
        lint.RuleRegistry(
          rules, _NoASTRules(), lint.LintConfig.default())
      let linter = lint.Linter(registry, FileAuth(auth), tmp.path)
      let targets = recover val [as String val: tmp.path] end
      (let diags, _) = linter.run(targets)
      h.assert_eq[USize](0, diags.size())

      corral_file.remove()
      corral_dir.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestLinterSuppressedDiagnosticsFiltered is UnitTest
  """Suppressed diagnostics are filtered out."""
  fun name(): String => "Linter: suppressed diagnostics filtered"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?
      let pony_file =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "test.pony"))
      let file = File(pony_file)
      let long_line = recover val String .> append("a".mul(100)) end
      let content: String val =
        "// pony-lint: allow style/line-length\n" + long_line + "\n"
      file.write(content)
      file.dispose()

      let rules: Array[lint.TextRule val] val =
        recover val [as lint.TextRule val: lint.LineLength] end
      let registry =
        lint.RuleRegistry(
          rules, _NoASTRules(), lint.LintConfig.default())
      let linter = lint.Linter(registry, FileAuth(auth), tmp.path)
      let targets = recover val [as String val: tmp.path] end
      (let diags, _) = linter.run(targets)
      h.assert_eq[USize](0, diags.size())

      pony_file.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestLinterDiagnosticsSorted is UnitTest
  """Diagnostics from multiple files are sorted."""
  fun name(): String => "Linter: diagnostics sorted across files"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?
      // Create two files with violations — sorted order by filename
      let file_b =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "b.pony"))
      let fb = File(file_b)
      let long_line = recover val String .> append("a".mul(100)) end
      fb.print(long_line)
      fb.dispose()

      let file_a =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "a.pony"))
      let fa = File(file_a)
      fa.print(long_line)
      fa.dispose()

      let rules: Array[lint.TextRule val] val =
        recover val [as lint.TextRule val: lint.LineLength] end
      let registry =
        lint.RuleRegistry(
          rules, _NoASTRules(), lint.LintConfig.default())
      let linter = lint.Linter(registry, FileAuth(auth), tmp.path)
      let targets = recover val [as String val: tmp.path] end
      (let diags, _) = linter.run(targets)
      h.assert_eq[USize](2, diags.size())
      // Verify sorted by file
      try
        h.assert_true(diags(0)?.file < diags(1)?.file)
      else
        h.fail("could not access diagnostics")
      end

      file_a.remove()
      file_b.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestLinterNonExistentTarget is UnitTest
  """Non-existent target produces lint/io-error and ExitError."""
  fun name(): String => "Linter: non-existent target"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    let rules: Array[lint.TextRule val] val =
      recover val [as lint.TextRule val: lint.LineLength] end
    let registry =
      lint.RuleRegistry(
        rules, _NoASTRules(), lint.LintConfig.default())
    let cwd = Path.cwd()
    let linter = lint.Linter(registry, FileAuth(auth), cwd)
    let targets =
      recover val [as String val: "no_such_file_or_dir_xyzzy"] end
    (let diags, let exit_code) = linter.run(targets)
    h.assert_true(diags.size() > 0)
    try
      h.assert_eq[String]("lint/io-error", diags(0)?.rule_id)
      h.assert_true(diags(0)?.message.contains("not found"))
    else
      h.fail("could not access diagnostic")
    end
    match exit_code
    | lint.ExitError => h.assert_true(true)
    else
      h.fail("expected ExitError")
    end

class \nodoc\ _TestLinterRespectsGitignore is UnitTest
  """Files matching .gitignore patterns are excluded from linting."""
  fun name(): String => "Linter: respects .gitignore"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?
      // Create .git dir to simulate a git repo
      let git_dir =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, ".git"))
      git_dir.mkdir()
      // Create .gitignore that ignores the "generated" directory
      let gitignore =
        File(
          FilePath(
            FileAuth(auth), Path.join(tmp.path, ".gitignore")))
      gitignore.print("generated/")
      gitignore.dispose()
      // Create a "generated" directory with a .pony file containing a
      // violation
      let gen_dir =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "generated"))
      gen_dir.mkdir()
      let gen_file =
        FilePath(
          FileAuth(auth), Path.join(gen_dir.path, "gen.pony"))
      let gf = File(gen_file)
      let long_line = recover val String .> append("a".mul(100)) end
      gf.print(long_line)
      gf.dispose()
      // Create a non-ignored .pony file that is clean
      let clean_file =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "clean.pony"))
      let cf = File(clean_file)
      cf.print("actor Main")
      cf.dispose()

      let rules: Array[lint.TextRule val] val =
        recover val [as lint.TextRule val: lint.LineLength] end
      let registry =
        lint.RuleRegistry(
          rules, _NoASTRules(), lint.LintConfig.default())
      let linter = lint.Linter(registry, FileAuth(auth), tmp.path)
      let targets = recover val [as String val: tmp.path] end
      (let diags, let exit_code) = linter.run(targets)
      // The generated/ file should be ignored, only clean.pony is linted
      h.assert_eq[USize](0, diags.size())
      match exit_code
      | lint.ExitSuccess => h.assert_true(true)
      else
        h.fail("expected ExitSuccess")
      end

      // Cleanup
      gen_file.remove()
      gen_dir.remove()
      clean_file.remove()
      FilePath(
        FileAuth(auth), Path.join(tmp.path, ".gitignore")).remove()
      git_dir.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestLinterExplicitFileBypassesIgnore is UnitTest
  """Explicit file targets are linted even when matching .gitignore."""
  fun name(): String =>
    "Linter: explicit file target bypasses .gitignore"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?
      // Create .git dir to simulate a git repo
      let git_dir =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, ".git"))
      git_dir.mkdir()
      // Create .gitignore that ignores *.pony
      let gitignore =
        File(
          FilePath(
            FileAuth(auth), Path.join(tmp.path, ".gitignore")))
      gitignore.print("*.pony")
      gitignore.dispose()
      // Create a .pony file with a violation
      let pony_file =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "test.pony"))
      let f = File(pony_file)
      let long_line = recover val String .> append("a".mul(100)) end
      f.print(long_line)
      f.dispose()

      let rules: Array[lint.TextRule val] val =
        recover val [as lint.TextRule val: lint.LineLength] end
      let registry =
        lint.RuleRegistry(
          rules, _NoASTRules(), lint.LintConfig.default())
      let linter = lint.Linter(registry, FileAuth(auth), tmp.path)
      // Target the file directly, not the directory
      let targets =
        recover val
          [as String val: Path.join(tmp.path, "test.pony")]
        end
      (let diags, _) = linter.run(targets)
      // File should be linted despite matching .gitignore
      h.assert_true(diags.size() > 0)

      // Cleanup
      pony_file.remove()
      FilePath(
        FileAuth(auth), Path.join(tmp.path, ".gitignore")).remove()
      git_dir.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestLinterRespectsGitignoreFromParent is UnitTest
  """Parent .gitignore applies when the target is a subdirectory."""
  fun name(): String =>
    "Linter: parent .gitignore applies to subdirectory target"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?
      // Create .git dir at root
      let git_dir =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, ".git"))
      git_dir.mkdir()
      // .gitignore at root ignores "generated/" directory
      let gitignore =
        File(
          FilePath(
            FileAuth(auth), Path.join(tmp.path, ".gitignore")))
      gitignore.print("generated/")
      gitignore.dispose()
      // Create src/ subdirectory as the lint target
      let src_dir =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "src"))
      src_dir.mkdir()
      // Create generated/ under src/ with a violation
      let gen_dir =
        FilePath(
          FileAuth(auth), Path.join(src_dir.path, "generated"))
      gen_dir.mkdir()
      let gen_file =
        FilePath(
          FileAuth(auth), Path.join(gen_dir.path, "gen.pony"))
      let gf = File(gen_file)
      let long_line = recover val String .> append("a".mul(100)) end
      gf.print(long_line)
      gf.dispose()
      // Create a clean file in src/
      let clean_file =
        FilePath(
          FileAuth(auth), Path.join(src_dir.path, "clean.pony"))
      let cf = File(clean_file)
      cf.print("actor Main")
      cf.dispose()

      let rules: Array[lint.TextRule val] val =
        recover val [as lint.TextRule val: lint.LineLength] end
      let registry =
        lint.RuleRegistry(
          rules, _NoASTRules(), lint.LintConfig.default())
      // Target is src/, not the repo root — tests that _GitRepoFinder
      // walks up to find .git and that _load_intermediate_ignores loads
      // root's .gitignore for the subdirectory target
      let linter = lint.Linter(registry, FileAuth(auth), tmp.path)
      let targets = recover val [as String val: src_dir.path] end
      (let diags, let exit_code) = linter.run(targets)
      // generated/ should be ignored via parent .gitignore
      h.assert_eq[USize](0, diags.size())
      match exit_code
      | lint.ExitSuccess => h.assert_true(true)
      else
        h.fail("expected ExitSuccess")
      end

      // Cleanup
      gen_file.remove()
      gen_dir.remove()
      clean_file.remove()
      src_dir.remove()
      FilePath(
        FileAuth(auth), Path.join(tmp.path, ".gitignore")).remove()
      git_dir.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestLinterSubdirConfigDisablesRule is UnitTest
  """Subdirectory .pony-lint.json disables a rule for that subtree."""
  fun name(): String =>
    "Linter: subdir config disables rule"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?

      // Root file with a long line (violation)
      let root_file =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "root.pony"))
      let rf = File(root_file)
      let long_line = recover val String .> append("a".mul(100)) end
      rf.print(long_line)
      rf.dispose()

      // Create examples/ subdirectory
      let examples_dir =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "examples"))
      examples_dir.mkdir()

      // examples/.pony-lint.json disables line-length
      let config_file =
        File(
          FilePath(
            FileAuth(auth),
            Path.join(examples_dir.path, ".pony-lint.json")))
      config_file.print(
        """{"rules": {"style/line-length": "off"}}""")
      config_file.dispose()

      // examples/ file with a long line (should NOT be flagged)
      let ex_file =
        FilePath(
          FileAuth(auth), Path.join(examples_dir.path, "ex.pony"))
      let ef = File(ex_file)
      ef.print(long_line)
      ef.dispose()

      let rules: Array[lint.TextRule val] val =
        recover val [as lint.TextRule val: lint.LineLength] end
      let registry =
        lint.RuleRegistry(
          rules, _NoASTRules(), lint.LintConfig.default())
      let linter =
        lint.Linter(
          registry, FileAuth(auth), tmp.path
          where root_dir = tmp.path)
      let targets = recover val [as String val: tmp.path] end
      (let diags, _) = linter.run(targets)

      // Only root.pony should have a violation
      h.assert_eq[USize](1, diags.size())
      try
        h.assert_true(diags(0)?.file.contains("root.pony"))
      else
        h.fail("could not access diagnostic")
      end

      // Cleanup
      ex_file.remove()
      FilePath(
        FileAuth(auth),
        Path.join(examples_dir.path, ".pony-lint.json")).remove()
      examples_dir.remove()
      root_file.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestLinterSubdirConfigEnablesRule is UnitTest
  """Subdirectory config enables a rule disabled by root config."""
  fun name(): String =>
    "Linter: subdir config enables rule"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?

      // Root .pony-lint.json disables line-length
      let root_config =
        File(
          FilePath(
            FileAuth(auth),
            Path.join(tmp.path, ".pony-lint.json")))
      root_config.print(
        """{"rules": {"style/line-length": "off"}}""")
      root_config.dispose()

      // Root file with a long line (should NOT be flagged)
      let root_file =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "root.pony"))
      let rf = File(root_file)
      let long_line = recover val String .> append("a".mul(100)) end
      rf.print(long_line)
      rf.dispose()

      // Create strict/ subdirectory
      let strict_dir =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "strict"))
      strict_dir.mkdir()

      // strict/.pony-lint.json re-enables line-length
      let strict_config =
        File(
          FilePath(
            FileAuth(auth),
            Path.join(strict_dir.path, ".pony-lint.json")))
      strict_config.print(
        """{"rules": {"style/line-length": "on"}}""")
      strict_config.dispose()

      // strict/ file with a long line (SHOULD be flagged)
      let strict_file =
        FilePath(
          FileAuth(auth), Path.join(strict_dir.path, "strict.pony"))
      let sf = File(strict_file)
      sf.print(long_line)
      sf.dispose()

      // Load root config manually since test creates its own
      let file_auth = FileAuth(auth)
      let root_rules =
        recover val
          let m = Map[String, lint.RuleStatus]
          m("style/line-length") = lint.RuleOff
          m
        end
      let config =
        lint.LintConfig(recover val Set[String] end, root_rules)
      let rules: Array[lint.TextRule val] val =
        recover val [as lint.TextRule val: lint.LineLength] end
      let registry = lint.RuleRegistry(rules, _NoASTRules(), config)
      let linter =
        lint.Linter(
          registry, file_auth, tmp.path
          where root_dir = tmp.path)
      let targets = recover val [as String val: tmp.path] end
      (let diags, _) = linter.run(targets)

      // Only strict.pony should have a violation
      h.assert_eq[USize](1, diags.size())
      try
        h.assert_true(diags(0)?.file.contains("strict.pony"))
      else
        h.fail("could not access diagnostic")
      end

      // Cleanup
      strict_file.remove()
      FilePath(
        FileAuth(auth),
        Path.join(strict_dir.path, ".pony-lint.json")).remove()
      strict_dir.remove()
      root_file.remove()
      FilePath(
        FileAuth(auth),
        Path.join(tmp.path, ".pony-lint.json")).remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestLinterSubdirConfigError is UnitTest
  """Malformed subdirectory config produces lint/config-error diagnostic."""
  fun name(): String =>
    "Linter: subdir config error"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?

      // Create sub/ directory with malformed config
      let sub_dir =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "sub"))
      sub_dir.mkdir()

      let bad_config =
        File(
          FilePath(
            FileAuth(auth),
            Path.join(sub_dir.path, ".pony-lint.json")))
      bad_config.print("{not valid json}")
      bad_config.dispose()

      // A clean file in sub/
      let sub_file =
        FilePath(
          FileAuth(auth), Path.join(sub_dir.path, "test.pony"))
      let sf = File(sub_file)
      sf.print("actor Main")
      sf.dispose()

      let rules: Array[lint.TextRule val] val =
        recover val [as lint.TextRule val: lint.LineLength] end
      let registry =
        lint.RuleRegistry(
          rules, _NoASTRules(), lint.LintConfig.default())
      let linter =
        lint.Linter(
          registry, FileAuth(auth), tmp.path
          where root_dir = tmp.path)
      let targets = recover val [as String val: tmp.path] end
      (let diags, let exit_code) = linter.run(targets)

      // Should have a config-error diagnostic
      var found_config_error = false
      for d in diags.values() do
        if d.rule_id == "lint/config-error" then
          found_config_error = true
          h.assert_true(d.message.contains("malformed JSON"))
        end
      end
      h.assert_true(found_config_error)
      match exit_code
      | lint.ExitError => h.assert_true(true)
      else
        h.fail("expected ExitError")
      end

      // Cleanup
      sub_file.remove()
      FilePath(
        FileAuth(auth),
        Path.join(sub_dir.path, ".pony-lint.json")).remove()
      sub_dir.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestLinterSubdirConfigCategoryCleaning is UnitTest
  """Child category off overrides parent rule-specific on."""
  fun name(): String =>
    "Linter: subdir config category cleaning"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?

      // Root config enables line-length explicitly
      let root_rules =
        recover val
          let m = Map[String, lint.RuleStatus]
          m("style/line-length") = lint.RuleOn
          m
        end
      let config =
        lint.LintConfig(recover val Set[String] end, root_rules)

      // Create examples/ with category-off config
      let examples_dir =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "examples"))
      examples_dir.mkdir()

      let ex_config =
        File(
          FilePath(
            FileAuth(auth),
            Path.join(examples_dir.path, ".pony-lint.json")))
      ex_config.print("""{"rules": {"style": "off"}}""")
      ex_config.dispose()

      // Long line in examples/ (should NOT be flagged)
      let ex_file =
        FilePath(
          FileAuth(auth), Path.join(examples_dir.path, "ex.pony"))
      let ef = File(ex_file)
      let long_line = recover val String .> append("a".mul(100)) end
      ef.print(long_line)
      ef.dispose()

      let rules: Array[lint.TextRule val] val =
        recover val [as lint.TextRule val: lint.LineLength] end
      let registry = lint.RuleRegistry(rules, _NoASTRules(), config)
      let linter =
        lint.Linter(
          registry, FileAuth(auth), tmp.path
          where root_dir = tmp.path)
      let targets =
        recover val [as String val: examples_dir.path] end
      (let diags, _) = linter.run(targets)

      // No violations — category cleaning removed rule-specific "on"
      h.assert_eq[USize](0, diags.size())

      // Cleanup
      ex_file.remove()
      FilePath(
        FileAuth(auth),
        Path.join(examples_dir.path, ".pony-lint.json")).remove()
      examples_dir.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestLinterExplicitFileSubdirConfig is UnitTest
  """Explicit file target respects config in its directory."""
  fun name(): String =>
    "Linter: explicit file target uses subdir config"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?

      // Create examples/ with config disabling line-length
      let examples_dir =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "examples"))
      examples_dir.mkdir()

      let ex_config =
        File(
          FilePath(
            FileAuth(auth),
            Path.join(examples_dir.path, ".pony-lint.json")))
      ex_config.print(
        """{"rules": {"style/line-length": "off"}}""")
      ex_config.dispose()

      // Long line in examples/ — should NOT be flagged
      let ex_file =
        FilePath(
          FileAuth(auth), Path.join(examples_dir.path, "ex.pony"))
      let ef = File(ex_file)
      let long_line = recover val String .> append("a".mul(100)) end
      ef.print(long_line)
      ef.dispose()

      let rules: Array[lint.TextRule val] val =
        recover val [as lint.TextRule val: lint.LineLength] end
      let registry =
        lint.RuleRegistry(
          rules, _NoASTRules(), lint.LintConfig.default())
      let linter =
        lint.Linter(
          registry, FileAuth(auth), tmp.path
          where root_dir = tmp.path)
      // Target the file directly, not the directory
      let targets =
        recover val
          [ as String val:
            Path.join(examples_dir.path, "ex.pony")
          ]
        end
      (let diags, _) = linter.run(targets)

      // No violations — subdir config disables line-length
      h.assert_eq[USize](0, diags.size())

      // Cleanup
      ex_file.remove()
      FilePath(
        FileAuth(auth),
        Path.join(examples_dir.path, ".pony-lint.json")).remove()
      examples_dir.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestLinterIntermediateConfigLoading is UnitTest
  """Intermediate config between root and target is loaded."""
  fun name(): String =>
    "Linter: intermediate config loading"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?

      // Create mid/ with config disabling line-length
      let mid_dir =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "mid"))
      mid_dir.mkdir()

      let mid_config =
        File(
          FilePath(
            FileAuth(auth),
            Path.join(mid_dir.path, ".pony-lint.json")))
      mid_config.print(
        """{"rules": {"style/line-length": "off"}}""")
      mid_config.dispose()

      // Create mid/sub/ as the lint target
      let sub_dir =
        FilePath(
          FileAuth(auth), Path.join(mid_dir.path, "sub"))
      sub_dir.mkdir()

      // Long line in sub/ — should NOT be flagged (inherits mid/ config)
      let sub_file =
        FilePath(
          FileAuth(auth), Path.join(sub_dir.path, "test.pony"))
      let sf = File(sub_file)
      let long_line = recover val String .> append("a".mul(100)) end
      sf.print(long_line)
      sf.dispose()

      let rules: Array[lint.TextRule val] val =
        recover val [as lint.TextRule val: lint.LineLength] end
      let registry =
        lint.RuleRegistry(
          rules, _NoASTRules(), lint.LintConfig.default())
      // Target is sub/, so mid/ is an intermediate directory
      let linter =
        lint.Linter(
          registry, FileAuth(auth), tmp.path
          where root_dir = tmp.path)
      let targets = recover val [as String val: sub_dir.path] end
      (let diags, _) = linter.run(targets)

      // No violations — mid/ config disables line-length
      h.assert_eq[USize](0, diags.size())

      // Cleanup
      sub_file.remove()
      sub_dir.remove()
      FilePath(
        FileAuth(auth),
        Path.join(mid_dir.path, ".pony-lint.json")).remove()
      mid_dir.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end
