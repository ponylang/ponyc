use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestApply)
    test(_TestGroups)
    test(_TestEq)
    test(_TestSplit)
    test(_TestError)

class iso _TestApply is UnitTest
  """
  Tests basic compilation and matching.
  """
  fun name(): String => "regex/Regex.apply"

  fun apply(h: TestHelper) ? =>
    let r = Regex("\\d+")
    let m = r("ab1234")
    h.assert_eq[String](m(0), "1234")
    h.assert_eq[USize](USize(2), m.start_pos())
    h.assert_eq[USize](USize(5), m.end_pos())

class iso _TestGroups is UnitTest
  """
  Tests basic compilation and matching.
  """
  fun name(): String => "regex/Regex.groups"

  fun apply(h: TestHelper) ? =>
    let r = Regex("""(\d+)?\.(\d+)?""")
    let m1 = r("123.456")
    h.assert_eq[String](m1(0), "123.456")
    h.assert_eq[String](m1(1), "123")
    h.assert_eq[String](m1(2), "456")
    h.assert_array_eq[String](m1.groups(), ["123", "456"])

    let m2 = r("123.")
    h.assert_eq[String](m2(0), "123.")
    h.assert_eq[String](m2(1), "123")
    h.assert_error(lambda()(m2 = m2)? => m2(2) end)
    h.assert_array_eq[String](m2.groups(), ["123", ""])

    let m3 = r(".456")
    h.assert_eq[String](m3(0), ".456")
    h.assert_error(lambda()(m3 = m3)? => m3(1) end)
    h.assert_eq[String](m3(2), "456")
    h.assert_array_eq[String](m3.groups(), ["", "456"])

class iso _TestEq is UnitTest
  """
  Tests eq operator.
  """
  fun name(): String => "regex/Regex.eq"

  fun apply(h: TestHelper) ? =>
    let r = Regex("\\d+")
    h.assert_true(r == "1234", """ \d+ =~ "1234" """)
    h.assert_true(r != "asdf", """ \d+ !~ "asdf" """)

class iso _TestSplit is UnitTest
  """
  Tests split.
  """
  fun name(): String => "regex/Regex.split"

  fun apply(h: TestHelper) ? =>
    h.assert_array_eq[String](["ab", "cd", "ef"],
      Regex("\\d+").split("ab12cd34ef"))
    h.assert_array_eq[String](["abcdef"], Regex("\\d*").split("abcdef"))
    h.assert_array_eq[String](["abc", "def"], Regex("\\d*").split("abc1def"))

class iso _TestError is UnitTest
  """
  Tests basic compilation failure.
  """
  fun name(): String => "regex/Regex.create/fails"

  fun apply(h: TestHelper) =>
    h.assert_error(lambda()? => Regex("(\\d+") end)
