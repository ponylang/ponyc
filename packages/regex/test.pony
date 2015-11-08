use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestApply)
    test(_TestEq)
    test(_TestSplit)
    test(_TestError)

class iso _TestApply is UnitTest
  """
  Tests basic compilation and matching.
  """
  fun name(): String => "regex/Regex.apply"

  fun apply(h: TestHelper): TestResult ? =>
    let r = Regex.create("\\d+")
    let m = r("ab1234")
    h.expect_eq[String](m(0), "1234")
    h.expect_eq[U64](U64(2), m.start_pos())
    h.expect_eq[U64](U64(5), m.end_pos())
    true

class iso _TestEq is UnitTest
  """
  Tests eq operator.
  """
  fun name(): String => "regex/Regex.eq"

  fun apply(h: TestHelper): TestResult ? =>
    let r = Regex.create("\\d+")
    h.expect_true(r == "1234", """ \d+ =~ "1234" """)
    h.expect_true(r != "asdf", """ \d+ !~ "asdf" """)
    true

class iso _TestSplit is UnitTest
  """
  Tests split.
  """
  fun name(): String => "regex/Regex.split"

  fun apply(h: TestHelper): TestResult ? =>
    let a: Array[String] = Regex("\\d+").split("ab12cd34ef")
    let exp: Array[String] = ["ab", "cd", "ef"]
    h.assert_array_eq[String](a, exp)
    true

class iso _TestError is UnitTest
  """
  Tests basic compilation failure.
  """
  fun name(): String => "regex/Regex.create/fails"

  fun apply(h: TestHelper): TestResult =>
    h.expect_error(lambda()? => Regex.create("(\\d+") end)
    true
