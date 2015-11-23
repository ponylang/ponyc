use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
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

    test(_TestFnMatch("a1c", "a[12]c", true))
    test(_TestFnMatch("a2c", "a[12]c", true))
    test(_TestFnMatch("a3c", "a[12]c", false))
    test(_TestFnMatch("a3c", "a[!12]c", true))
    test(_TestFnMatch("a2c", "a[!12]c", false))
    test(_TestFnMatch("a1c", "a[!12]c", false))
    test(_TestFnMatch("a11c", "a[12]c", false))
    test(_TestFnMatch("a12c", "a[12]c", false))

    test(_TestFnMatchCase)
    test(_TestFilter)
    test(_TestMkdtemp)
    test(_TestWalk)

class iso _TestFnMatchCase is UnitTest
  fun name(): String => "files/Glob.fnmatchcase"

  fun apply(h: TestHelper): TestResult =>
    h.expect_true(Glob.fnmatchcase("aBc", "aBc"))
    h.expect_false(Glob.fnmatchcase("aBc", "abc"))
    true

class iso _TestFnMatch is UnitTest
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

class iso _TestFilter is UnitTest
  fun name(): String => "files/Glob.filter"

  fun apply(h: TestHelper): TestResult? =>
    let m = Glob.filter([ "12/a/Bcd", "12/q/abd", "34/b/Befd"], "*/?/B*d")
    h.assert_eq[U64](2, m.size())

    h.expect_eq[String](m(0)._1, "12/a/Bcd")
    h.expect_array_eq[String](m(0)._2, ["12", "a", "c"])

    h.expect_eq[String](m(1)._1, "34/b/Befd")
    h.expect_array_eq[String](m(1)._2, ["34", "b", "ef"])
    true

class iso _TestMkdtemp  is UnitTest
  fun name(): String => "files/FilePath.mkdtemp"
  fun apply(h: TestHelper): TestResult? => error

  fun run(h: TestHelper, env: Env): TestResult? =>
    h.expect_error(lambda()(e = env)? => FilePath.mkdtemp(e.root, "tmp") end,
        "FilePath.mkdtemp should fail when path doesn't have a XXXXXX suffix")

    let tmp = FilePath.mkdtemp(env.root, "tmp.TestMkdtemp.XXXXXX")
    h.expect_true(FileInfo(tmp).directory)
    h.expect_true(tmp.remove())
    true

class iso _TestWalk  is UnitTest
  fun name(): String => "files/FilePath.walk"
  fun apply(h: TestHelper): TestResult? => error

  fun run(h: TestHelper, env: Env): TestResult? =>
    let top = Directory(FilePath.mkdtemp(env.root, "tmp.TestWalk.XXXXXX"))
    top.mkdir("a")
    top.create_file("b")
    top.mkdir("c")
    let a = top.open("a")
    a.create_file("1")
    a.create_file("2")
    let c = top.open("c")
    c.create_file("3")
    c.create_file("4")
    top.path.walk(
        lambda(dir: FilePath, entries: Array[String] ref)
              (p = top.path.path, h = h) =>
      try
        if dir.path == p then
          h.expect_array_eq[String](entries, ["a", "b", "c"])
          entries.pop()
        elseif dir.path.at("a", -1) then
          h.expect_array_eq[String](entries, ["1", "2"])
        else
          h.assert_failed("Unexpected dir: " + dir.path)
        end
      else
        h.assert_failed("Unexpected error in walk: " + dir.path)
      end
    end)
    h.expect_true(top.path.remove())
    true
