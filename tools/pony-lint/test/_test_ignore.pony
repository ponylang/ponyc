use "pony_test"
use "files"
use lint = ".."

// --- PatternParser tests ---

class \nodoc\ _TestPatternParserBlankLine is UnitTest
  """Blank lines produce None."""
  fun name(): String => "PatternParser: blank line"

  fun apply(h: TestHelper) =>
    h.assert_true(lint.PatternParser("") is None)
    h.assert_true(lint.PatternParser("   ") is None)
    h.assert_true(lint.PatternParser("\t") is None)

class \nodoc\ _TestPatternParserComment is UnitTest
  """Comment lines produce None."""
  fun name(): String => "PatternParser: comment"

  fun apply(h: TestHelper) =>
    h.assert_true(lint.PatternParser("# this is a comment") is None)
    h.assert_true(lint.PatternParser("#") is None)

class \nodoc\ _TestPatternParserNegation is UnitTest
  """Lines starting with ! are negated."""
  fun name(): String => "PatternParser: negation"

  fun apply(h: TestHelper) =>
    match lint.PatternParser("!foo")
    | let pat: lint.IgnorePattern val =>
      h.assert_true(pat.negated)
      h.assert_eq[String]("foo", pat.pattern)
    else
      h.fail("expected pattern")
    end

class \nodoc\ _TestPatternParserTrailingSlash is UnitTest
  """Trailing / sets dir_only and is stripped."""
  fun name(): String => "PatternParser: trailing slash"

  fun apply(h: TestHelper) =>
    match lint.PatternParser("build/")
    | let pat: lint.IgnorePattern val =>
      h.assert_true(pat.dir_only)
      h.assert_eq[String]("build", pat.pattern)
      h.assert_false(pat.anchored)
    else
      h.fail("expected pattern")
    end

class \nodoc\ _TestPatternParserLeadingSlash is UnitTest
  """Leading / makes the pattern anchored and is stripped."""
  fun name(): String => "PatternParser: leading slash"

  fun apply(h: TestHelper) =>
    match lint.PatternParser("/build")
    | let pat: lint.IgnorePattern val =>
      h.assert_true(pat.anchored)
      h.assert_eq[String]("build", pat.pattern)
    else
      h.fail("expected pattern")
    end

class \nodoc\ _TestPatternParserAnchoring is UnitTest
  """Patterns containing / (not just leading or trailing) are anchored."""
  fun name(): String => "PatternParser: anchoring by contained /"

  fun apply(h: TestHelper) =>
    // Contains / → anchored
    match lint.PatternParser("src/build")
    | let pat: lint.IgnorePattern val =>
      h.assert_true(pat.anchored)
      h.assert_eq[String]("src/build", pat.pattern)
    else
      h.fail("expected pattern")
    end
    // No / → unanchored
    match lint.PatternParser("build")
    | let pat: lint.IgnorePattern val =>
      h.assert_false(pat.anchored)
      h.assert_eq[String]("build", pat.pattern)
    else
      h.fail("expected pattern")
    end

class \nodoc\ _TestPatternParserEscapes is UnitTest
  """Backslash escapes are resolved."""
  fun name(): String => "PatternParser: backslash escapes"

  fun apply(h: TestHelper) =>
    // Escaped # at start → literal #
    match lint.PatternParser("\\#not-a-comment")
    | let pat: lint.IgnorePattern val =>
      h.assert_eq[String]("#not-a-comment", pat.pattern)
    else
      h.fail("expected pattern for \\#")
    end
    // Escaped ! at start → literal !, not negated
    match lint.PatternParser("\\!important")
    | let pat: lint.IgnorePattern val =>
      h.assert_false(pat.negated)
      h.assert_eq[String]("!important", pat.pattern)
    else
      h.fail("expected pattern for \\!")
    end
    // Escaped space preserves trailing space
    match lint.PatternParser("foo\\ ")
    | let pat: lint.IgnorePattern val =>
      h.assert_eq[String]("foo ", pat.pattern)
    else
      h.fail("expected pattern for escaped space")
    end

class \nodoc\ _TestPatternParserTrailingWhitespace is UnitTest
  """Unescaped trailing whitespace is stripped."""
  fun name(): String => "PatternParser: trailing whitespace stripped"

  fun apply(h: TestHelper) =>
    match lint.PatternParser("build   ")
    | let pat: lint.IgnorePattern val =>
      h.assert_eq[String]("build", pat.pattern)
    else
      h.fail("expected pattern")
    end

class \nodoc\ _TestPatternParserSimpleWildcard is UnitTest
  """Simple wildcard patterns preserve * characters."""
  fun name(): String => "PatternParser: wildcard patterns"

  fun apply(h: TestHelper) =>
    match lint.PatternParser("*.o")
    | let pat: lint.IgnorePattern val =>
      h.assert_eq[String]("*.o", pat.pattern)
      h.assert_false(pat.anchored)
      h.assert_false(pat.negated)
      h.assert_false(pat.dir_only)
    else
      h.fail("expected pattern")
    end

class \nodoc\ _TestPatternParserDoubleStarAnchored is UnitTest
  """A pattern with **/ is anchored (contains /)."""
  fun name(): String => "PatternParser: **/ is anchored"

  fun apply(h: TestHelper) =>
    match lint.PatternParser("**/foo")
    | let pat: lint.IgnorePattern val =>
      h.assert_true(pat.anchored)
      h.assert_eq[String]("**/foo", pat.pattern)
    else
      h.fail("expected pattern")
    end

class \nodoc\ _TestIgnoreMatcherGitignore is UnitTest
  """A .gitignore pattern excludes matching entries."""
  fun name(): String => "IgnoreMatcher: .gitignore excludes"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "ignore-test")?
      // Create .git dir to simulate a git repo
      let git_dir = FilePath(FileAuth(auth),
        Path.join(tmp.path, ".git"))
      git_dir.mkdir()
      // Create .gitignore
      let gitignore = File(FilePath(FileAuth(auth),
        Path.join(tmp.path, ".gitignore")))
      gitignore.print("build/")
      gitignore.print("*.o")
      gitignore.dispose()

      let matcher = lint.IgnoreMatcher(FileAuth(auth), tmp.path)
      matcher.load_directory(tmp.path)

      // Directory "build" should be ignored (dir_only pattern)
      h.assert_true(matcher.is_ignored(
        Path.join(tmp.path, "build"), "build", true))
      // File "build" should NOT be ignored (dir_only pattern)
      h.assert_false(matcher.is_ignored(
        Path.join(tmp.path, "build"), "build", false))
      // .o file should be ignored
      h.assert_true(matcher.is_ignored(
        Path.join(tmp.path, "foo.o"), "foo.o", false))
      // .pony file should not be ignored
      h.assert_false(matcher.is_ignored(
        Path.join(tmp.path, "foo.pony"), "foo.pony", false))

      // Cleanup
      FilePath(FileAuth(auth),
        Path.join(tmp.path, ".gitignore")).remove()
      git_dir.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestIgnoreMatcherIgnoreFile is UnitTest
  """.ignore files are always loaded, even outside a git repo."""
  fun name(): String => "IgnoreMatcher: .ignore without git repo"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "ignore-test")?
      // No .git dir — not a git repo
      // Create .ignore
      let ignore = File(FilePath(FileAuth(auth),
        Path.join(tmp.path, ".ignore")))
      ignore.print("build/")
      ignore.dispose()

      // root = None means no git repo
      let matcher = lint.IgnoreMatcher(FileAuth(auth), None)
      matcher.load_directory(tmp.path)

      // Directory "build" should be ignored via .ignore
      h.assert_true(matcher.is_ignored(
        Path.join(tmp.path, "build"), "build", true))
      // File should not be ignored
      h.assert_false(matcher.is_ignored(
        Path.join(tmp.path, "foo.pony"), "foo.pony", false))

      FilePath(FileAuth(auth),
        Path.join(tmp.path, ".ignore")).remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestIgnoreMatcherIgnoreOverridesGitignore is UnitTest
  """.ignore rules override .gitignore (loaded after, higher precedence)."""
  fun name(): String => "IgnoreMatcher: .ignore overrides .gitignore"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "ignore-test")?
      let git_dir = FilePath(FileAuth(auth),
        Path.join(tmp.path, ".git"))
      git_dir.mkdir()
      // .gitignore ignores build/
      let gitignore = File(FilePath(FileAuth(auth),
        Path.join(tmp.path, ".gitignore")))
      gitignore.print("build/")
      gitignore.dispose()
      // .ignore negates build/
      let ignore = File(FilePath(FileAuth(auth),
        Path.join(tmp.path, ".ignore")))
      ignore.print("!build/")
      ignore.dispose()

      let matcher = lint.IgnoreMatcher(FileAuth(auth), tmp.path)
      matcher.load_directory(tmp.path)

      // build/ should NOT be ignored because .ignore negates it
      h.assert_false(matcher.is_ignored(
        Path.join(tmp.path, "build"), "build", true))

      FilePath(FileAuth(auth),
        Path.join(tmp.path, ".gitignore")).remove()
      FilePath(FileAuth(auth),
        Path.join(tmp.path, ".ignore")).remove()
      git_dir.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestIgnoreMatcherNegation is UnitTest
  """A negated pattern un-ignores a previously ignored entry."""
  fun name(): String => "IgnoreMatcher: negation un-ignores"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "ignore-test")?
      let git_dir = FilePath(FileAuth(auth),
        Path.join(tmp.path, ".git"))
      git_dir.mkdir()
      // .gitignore: ignore all .o, but keep important.o
      let gitignore = File(FilePath(FileAuth(auth),
        Path.join(tmp.path, ".gitignore")))
      gitignore.print("*.o")
      gitignore.print("!important.o")
      gitignore.dispose()

      let matcher = lint.IgnoreMatcher(FileAuth(auth), tmp.path)
      matcher.load_directory(tmp.path)

      // Regular .o file should be ignored
      h.assert_true(matcher.is_ignored(
        Path.join(tmp.path, "foo.o"), "foo.o", false))
      // important.o should NOT be ignored (negated)
      h.assert_false(matcher.is_ignored(
        Path.join(tmp.path, "important.o"), "important.o", false))

      FilePath(FileAuth(auth),
        Path.join(tmp.path, ".gitignore")).remove()
      git_dir.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestIgnoreMatcherNonGitIgnoresGitignore is UnitTest
  """Outside a git repo, .gitignore files are not loaded."""
  fun name(): String => "IgnoreMatcher: non-git skips .gitignore"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "ignore-test")?
      // No .git dir
      // Create .gitignore (should be ignored since not in git repo)
      let gitignore = File(FilePath(FileAuth(auth),
        Path.join(tmp.path, ".gitignore")))
      gitignore.print("*.o")
      gitignore.dispose()

      // root = None means no git repo
      let matcher = lint.IgnoreMatcher(FileAuth(auth), None)
      matcher.load_directory(tmp.path)

      // .o file should NOT be ignored (no git repo, .gitignore not loaded)
      h.assert_false(matcher.is_ignored(
        Path.join(tmp.path, "foo.o"), "foo.o", false))

      FilePath(FileAuth(auth),
        Path.join(tmp.path, ".gitignore")).remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestIgnoreMatcherHierarchical is UnitTest
  """Rules from subdirectory .gitignore override parent rules."""
  fun name(): String => "IgnoreMatcher: hierarchical .gitignore"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "ignore-test")?
      let git_dir = FilePath(FileAuth(auth),
        Path.join(tmp.path, ".git"))
      git_dir.mkdir()
      // Root .gitignore ignores all .o files
      let gitignore = File(FilePath(FileAuth(auth),
        Path.join(tmp.path, ".gitignore")))
      gitignore.print("*.o")
      gitignore.dispose()
      // Subdirectory .gitignore un-ignores .o files
      let sub_dir = FilePath(FileAuth(auth),
        Path.join(tmp.path, "src"))
      sub_dir.mkdir()
      let sub_gitignore = File(FilePath(FileAuth(auth),
        Path.join(sub_dir.path, ".gitignore")))
      sub_gitignore.print("!*.o")
      sub_gitignore.dispose()

      let matcher = lint.IgnoreMatcher(FileAuth(auth), tmp.path)
      matcher.load_directory(tmp.path)
      matcher.load_directory(sub_dir.path)

      // .o file at root should be ignored
      h.assert_true(matcher.is_ignored(
        Path.join(tmp.path, "foo.o"), "foo.o", false))
      // .o file in src/ should NOT be ignored (subdirectory negation)
      h.assert_false(matcher.is_ignored(
        Path.join(sub_dir.path, "foo.o"), "foo.o", false))

      FilePath(FileAuth(auth),
        Path.join(sub_dir.path, ".gitignore")).remove()
      sub_dir.remove()
      FilePath(FileAuth(auth),
        Path.join(tmp.path, ".gitignore")).remove()
      git_dir.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end

class \nodoc\ _TestIgnoreMatcherAnchoredPattern is UnitTest
  """Anchored patterns match against the relative path from base_dir."""
  fun name(): String => "IgnoreMatcher: anchored pattern"

  fun apply(h: TestHelper) =>
    let auth = h.env.root
    try
      let tmp = FilePath.mkdtemp(FileAuth(auth), "ignore-test")?
      let git_dir = FilePath(FileAuth(auth),
        Path.join(tmp.path, ".git"))
      git_dir.mkdir()
      // Anchored pattern: src/build (contains /)
      let gitignore = File(FilePath(FileAuth(auth),
        Path.join(tmp.path, ".gitignore")))
      gitignore.print("src/build/")
      gitignore.dispose()
      // Create directory structure
      let src_dir = FilePath(FileAuth(auth),
        Path.join(tmp.path, "src"))
      src_dir.mkdir()

      let matcher = lint.IgnoreMatcher(FileAuth(auth), tmp.path)
      matcher.load_directory(tmp.path)

      // src/build directory should be ignored (anchored match)
      let build_path = Path.join(Path.join(tmp.path, "src"), "build")
      h.assert_true(matcher.is_ignored(build_path, "build", true))
      // other/build should NOT be ignored (anchored to root)
      let other_build = Path.join(Path.join(tmp.path, "other"), "build")
      h.assert_false(matcher.is_ignored(other_build, "build", true))

      FilePath(FileAuth(auth),
        Path.join(tmp.path, ".gitignore")).remove()
      src_dir.remove()
      git_dir.remove()
      tmp.remove()
    else
      h.fail("could not create temp directory")
    end
