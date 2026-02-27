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
      let pony_file = FilePath(FileAuth(auth),
        Path.join(tmp.path, "test.pony"))
      let file = File(pony_file)
      let long_line = recover val String .> append("a".mul(100)) end
      file.print(long_line)
      file.dispose()

      let rules: Array[lint.TextRule val] val = recover val
        [as lint.TextRule val: lint.LineLength]
      end
      let registry = lint.RuleRegistry(
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
      let pony_file = FilePath(FileAuth(auth),
        Path.join(tmp.path, "test.pony"))
      let file = File(pony_file)
      file.print("actor Main")
      file.print("  new create(env: Env) =>")
      file.print("    None")
      file.dispose()

      let rules: Array[lint.TextRule val] val = recover val
        Array[lint.TextRule val]
          .> push(lint.LineLength)
          .> push(lint.TrailingWhitespace)
          .> push(lint.HardTabs)
          .> push(lint.CommentSpacing)
      end
      let registry = lint.RuleRegistry(
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
      let corral_dir = FilePath(FileAuth(auth),
        Path.join(tmp.path, "_corral"))
      corral_dir.mkdir()
      let corral_file = FilePath(FileAuth(auth),
        Path.join(corral_dir.path, "dep.pony"))
      let file = File(corral_file)
      let long_line = recover val String .> append("a".mul(100)) end
      file.print(long_line)
      file.dispose()

      let rules: Array[lint.TextRule val] val = recover val
        [as lint.TextRule val: lint.LineLength]
      end
      let registry = lint.RuleRegistry(
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
      let pony_file = FilePath(FileAuth(auth),
        Path.join(tmp.path, "test.pony"))
      let file = File(pony_file)
      let long_line = recover val String .> append("a".mul(100)) end
      let content: String val =
        "// pony-lint: allow style/line-length\n" + long_line + "\n"
      file.write(content)
      file.dispose()

      let rules: Array[lint.TextRule val] val = recover val
        [as lint.TextRule val: lint.LineLength]
      end
      let registry = lint.RuleRegistry(
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
      let file_b = FilePath(FileAuth(auth),
        Path.join(tmp.path, "b.pony"))
      let fb = File(file_b)
      let long_line = recover val String .> append("a".mul(100)) end
      fb.print(long_line)
      fb.dispose()

      let file_a = FilePath(FileAuth(auth),
        Path.join(tmp.path, "a.pony"))
      let fa = File(file_a)
      fa.print(long_line)
      fa.dispose()

      let rules: Array[lint.TextRule val] val = recover val
        [as lint.TextRule val: lint.LineLength]
      end
      let registry = lint.RuleRegistry(
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
    let rules: Array[lint.TextRule val] val = recover val
      [as lint.TextRule val: lint.LineLength]
    end
    let registry = lint.RuleRegistry(
      rules, _NoASTRules(), lint.LintConfig.default())
    let cwd = Path.cwd()
    let linter = lint.Linter(registry, FileAuth(auth), cwd)
    let targets = recover val
      [as String val: "no_such_file_or_dir_xyzzy"]
    end
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
