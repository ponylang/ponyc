use "pony_test"
use "files"
use lint = ".."

class \nodoc\ _TestExprNodeScannerFindsMatch is UnitTest
  """
  A non-exhaustive match triggers ExhaustiveMatch when linted through
  `Linter.run()`. This exercises the `_ExprNodeScanner` path: the scanner
  must find `tk_match` in the parse-level AST so `PassExpr` runs and the
  rule can fire. If the scanner were broken, PassExpr would be skipped
  and no diagnostic would appear.
  """
  fun name(): String => "ExprNodeScanner: match found -> ExhaustiveMatch fires"
  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?
      let pony_file =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "test.pony"))
      let file = File(pony_file)
      // Non-exhaustive match — covers all cases but lacks \exhaustive\
      file.print("type Color is (Red | Green | Blue)")
      file.print("primitive Red")
      file.print("primitive Green")
      file.print("primitive Blue")
      file.print("")
      file.print("primitive Foo")
      file.print("  fun apply(c: Color): String =>")
      file.print("    match c")
      file.print("    | Red => \"red\"")
      file.print("    | Green => \"green\"")
      file.print("    | Blue => \"blue\"")
      file.print("    end")
      file.dispose()

      let ast_rules: Array[lint.ASTRule val] val =
        recover val [as lint.ASTRule val: lint.ExhaustiveMatch] end
      let registry =
        lint.RuleRegistry(
          recover val Array[lint.TextRule val] end,
          ast_rules,
          lint.LintConfig.default())
      let package_paths = _ponypath(h.env.vars)
      let linter =
        lint.Linter(
          registry, FileAuth(auth), tmp.path, package_paths)
      let targets = recover val [as String val: tmp.path] end
      (let diags, _) = linter.run(targets)

      var found_exhaustive = false
      for d in diags.values() do
        if d.rule_id == "safety/exhaustive-match" then
          found_exhaustive = true
          break
        end
      end
      h.assert_true(
        found_exhaustive,
        "expected ExhaustiveMatch diagnostic")

      pony_file.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory or compile")
    end

  fun _ponypath(
    vars: (Array[String val] val | None))
    : Array[String val] val
  =>
    match vars
    | let env_vars: Array[String val] val =>
      for pair in env_vars.values() do
        if pair.at("PONYPATH=") then
          let paths = Path.split_list(pair.substring(ISize(9)))
          return consume paths
        end
      end
    end
    recover val Array[String val] end

class \nodoc\ _TestExprNodeScannerSkipsPassExpr is UnitTest
  """
  Source without a match expression produces no ExhaustiveMatch
  diagnostic. The scanner should find no matching tokens and skip
  PassExpr entirely.
  """
  fun name(): String =>
    "ExprNodeScanner: no match -> no ExhaustiveMatch diagnostic"

  fun exclusion_group(): String => "ast-compile"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "pony-lint-test")?
      let pony_file =
        FilePath(
          FileAuth(auth), Path.join(tmp.path, "test.pony"))
      let file = File(pony_file)
      file.print("primitive Foo")
      file.print("  fun apply(): String => \"hello\"")
      file.dispose()

      let ast_rules: Array[lint.ASTRule val] val =
        recover val [as lint.ASTRule val: lint.ExhaustiveMatch] end
      let registry =
        lint.RuleRegistry(
          recover val Array[lint.TextRule val] end,
          ast_rules,
          lint.LintConfig.default())
      let package_paths = _ponypath(h.env.vars)
      let linter =
        lint.Linter(
          registry, FileAuth(auth), tmp.path, package_paths)
      let targets = recover val [as String val: tmp.path] end
      (let diags, _) = linter.run(targets)

      for d in diags.values() do
        if d.rule_id == "safety/exhaustive-match" then
          h.fail("unexpected ExhaustiveMatch diagnostic")
          return
        end
      end

      pony_file.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory or compile")
    end

  fun _ponypath(
    vars: (Array[String val] val | None))
    : Array[String val] val
  =>
    match vars
    | let env_vars: Array[String val] val =>
      for pair in env_vars.values() do
        if pair.at("PONYPATH=") then
          let paths = Path.split_list(pair.substring(ISize(9)))
          return consume paths
        end
      end
    end
    recover val Array[String val] end
