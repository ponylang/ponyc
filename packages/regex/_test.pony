use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestApply)
    test(_TestGroups)
    test(_TestEq)
    test(_TestMatchIterator)
    test(_TestMatchIteratorEmpty)
    test(_TestSplit)
    test(_TestError)

class iso _TestApply is UnitTest
  """
  Tests basic compilation and matching.
  """
  fun name(): String => "regex/Regex.apply"

  fun apply(h: TestHelper) ? =>
    let r = Regex("\\d+")?
    let m = r("ab1234")?
    h.assert_eq[String](m(0)?, "1234")
    h.assert_eq[USize](USize(2), m.start_pos())
    h.assert_eq[USize](USize(5), m.end_pos())

class iso _TestGroups is UnitTest
  """
  Tests basic compilation and matching.
  """
  fun name(): String => "regex/Regex.groups"

  fun apply(h: TestHelper) ? =>
    let r = Regex("""(\d+)?\.(\d+)?""")?
    let m1 = r("123.456")?
    h.assert_eq[String](m1(0)?, "123.456")
    h.assert_eq[String](m1(1)?, "123")
    h.assert_eq[String](m1(2)?, "456")
    h.assert_array_eq[String](m1.groups(), ["123"; "456"])

    let m2 = r("123.")?
    h.assert_eq[String](m2(0)?, "123.")
    h.assert_eq[String](m2(1)?, "123")
    h.assert_error({() ? => m2(2)? })
    h.assert_array_eq[String](m2.groups(), ["123"; ""])

    let m3 = r(".456")?
    h.assert_eq[String](m3(0)?, ".456")
    h.assert_error({() ? => m3(1)? })
    h.assert_eq[String](m3(2)?, "456")
    h.assert_array_eq[String](m3.groups(), [""; "456"])

class iso _TestEq is UnitTest
  """
  Tests eq operator.
  """
  fun name(): String => "regex/Regex.eq"

  fun apply(h: TestHelper) ? =>
    let r = Regex("\\d+")?
    h.assert_true(r == "1234", """ \d+ =~ "1234" """)
    h.assert_true(r != "asdf", """ \d+ !~ "asdf" """)

class iso _TestMatchIterator is UnitTest
  """
  Tests the match iterator
  """
  fun name(): String => "regex/Regex.matches"

  fun apply(h: TestHelper) ? =>
    let r = Regex("([+-]?\\s*\\d+[dD]\\d+|[+-]?\\s*\\d+)")?
    let matches = r.matches("9d6+4d20+30-5d5+12-60")

    let m1 = matches.next()?
    h.assert_eq[String](m1(0)?, "9d6")
    let m2 = matches.next()?
    h.assert_eq[String](m2(0)?, "+4d20")
    let m3 = matches.next()?
    h.assert_eq[String](m3(0)?, "+30")
    let m4 = matches.next()?
    h.assert_eq[String](m4(0)?, "-5d5")
    let m5 = matches.next()?
    h.assert_eq[String](m5(0)?, "+12")
    let m6 = matches.next()?
    h.assert_eq[String](m6(0)?, "-60")
    h.assert_false(matches.has_next())

class iso _TestMatchIteratorEmpty is UnitTest
  """
  Tests the match iterator when the subject doesn't contain any matches
  for the regular expression
  """
  fun name(): String => "regex/Regex.matches/nomatch"

  fun apply(h: TestHelper) ? =>
    let r = Regex("([+-]?\\s*\\d+[dD]\\d+|[+-]?\\s*\\d+)")?
    let matches = r.matches("nevergonnaletyoudown")

    h.assert_false(matches.has_next())

class iso _TestSplit is UnitTest
  """
  Tests split.
  """
  fun name(): String => "regex/Regex.split"

  fun apply(h: TestHelper) ? =>
    h.assert_array_eq[String](["ab"; "cd"; "ef"],
      Regex("\\d+")?.split("ab12cd34ef")?)
    h.assert_array_eq[String](["abcdef"], Regex("\\d*")?.split("abcdef")?)
    h.assert_array_eq[String](["abc"; "def"], Regex("\\d*")?.split("abc1def")?)

class iso _TestError is UnitTest
  """
  Tests basic compilation failure.
  """
  fun name(): String => "regex/Regex.create/fails"

  fun apply(h: TestHelper) =>
    h.assert_error({() ? => Regex("(\\d+")? })
