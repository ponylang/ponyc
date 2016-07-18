use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestIniParse)

class iso _TestIniParse is UnitTest
  fun name(): String => "ini/IniParse"

  fun apply(h: TestHelper) ? =>
    let source =
      """
      key 1 = value 1 ; Not in a section
      [Section 1] # First section

      key 2 =     value 2

      [Section 2]

      [Section 3]
      key 2 = value 3
             key 1   =   value 1;Not a comment
      """

    let array: Array[String] = source.split("\r\n")
    let map = IniParse(array.values())

    h.assert_eq[Bool](map.contains("Section 2"), true)

    h.assert_eq[String](map("")("key 1"), "value 1")
    h.assert_eq[String](map("Section 1")("key 2"), "value 2")
    h.assert_eq[String](map("Section 3")("key 2"), "value 3")
    h.assert_eq[String](map("Section 3")("key 1"), "value 1;Not a comment")
