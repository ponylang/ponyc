use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestStringsCommonPrefix)

class iso _TestStringsCommonPrefix is UnitTest
  """
  Test strings/CommonPrefix
  """
  fun name(): String => "strings/CommonPrefix"

  fun apply(h: TestHelper) =>
    h.assert_eq[String]("", CommonPrefix(Array[String]))
    h.assert_eq[String]("", CommonPrefix([""]))
    h.assert_eq[String]("", CommonPrefix(["", "asdf"]))
    h.assert_eq[String]("", CommonPrefix(["qwer", "asdf"]))
    h.assert_eq[String]("", CommonPrefix(["asdf", "asdf", "qwer"]))
    h.assert_eq[String]("asdf", CommonPrefix(["asdf", "asdf"]))
    h.assert_eq[String]("as", CommonPrefix(["asdf", "asdf", "aser"]))
    h.assert_eq[String]("a", CommonPrefix(["a", "asdf", "asdf", "aser"]))
    h.assert_eq[String]("12", CommonPrefix([as Stringable: U32(1234), U32(12)]))
