use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestFnMatchCase)
    test(_TestFnMatch("abc", "abc", true))
    test(_TestFnMatch("abc", "a.c", false))

    // Make sure regular expression special characters don't screw things up.
    test(_TestFnMatch("a[].\\()c", "a[].\\()c", true))
    test(_TestFnMatch("a[].\\()d", "a[].\\()c", false))

    // Globs don't allow wild card escaping (it would mess up windows paths).
    test(_TestFnMatch("a*d", "a\\*c", false))
    test(_TestFnMatch("a\\bdc", "a\\*c", true))
    test(_TestFnMatch("a?c", "a\\?c", false))
    test(_TestFnMatch("a\\bc", "a\\?c", true))

    test(_TestFnMatch("abc", "a?c", true))
    test(_TestFnMatch("abbc", "a?c", false))
    test(_TestFnMatch("a/c", "a?c", false))

    test(_TestFnMatch("ac", "a*c", true))
    test(_TestFnMatch("abc", "a*c", true))
    test(_TestFnMatch("abbc", "a*c", true))
    test(_TestFnMatch("a/c", "a*c", false))
    test(_TestFnMatch("abc/def", "a*/*f", true))
    test(_TestFnMatch("a/ccc/d/f", "a/*/?/f", true))

    test(_TestFnMatch("ac", "a**c", true))
    test(_TestFnMatch("abc", "a**c", true))
    test(_TestFnMatch("abbc", "a**c", true))
    test(_TestFnMatch("a/c", "a**c", true))
    test(_TestFnMatch("ab/bc", "a**c", true))
    test(_TestFnMatch("a/b/c", "a**c", true))

class iso _TestFnMatchCase is UnitTest
  """
  Tests basic compilation and matching.
  """
  fun name(): String => "files/Glob.fnmatchcase"

  fun apply(h: TestHelper): TestResult =>
    h.expect_true(Glob.fnmatchcase("aBc", "aBc"))
    h.expect_false(Glob.fnmatchcase("aBc", "abc"))
    true

class iso _TestFnMatch is UnitTest
  """
  Tests basic compilation and matching.
  """
  let _glob: String
  let _path: String
  let _matches: Bool

  new iso create(glob: String, path: String, matches: Bool) =>
    _glob = glob
    _path = path
    _matches = matches

  fun name(): String =>
    "files/" + (if _matches then "" else "!" end) +
      "Glob.fnmatch(" + _glob + ", " + _path + ")"

  fun apply(h: TestHelper): TestResult =>
    _matches == Glob.fnmatch(_glob, _path)

