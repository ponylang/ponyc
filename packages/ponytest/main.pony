use "collections"

actor Main
  new create(env: Env) =>
    var test = PonyTest(env)

    test(recover _TestStringRunes end)

    test.complete()

class _TestStringRunes iso is UnitTest
  """
  Test iterating over the unicode codepoints in a string.
  """
  fun name(): String => "string.runes"

  fun apply(h: TestHelper): TestResult ? =>
    let s = "\u16ddx\ufb04"
    let expect = [as U32: 0x16dd, 'x', 0xfb04]
    let result = Array[U32]

    for c in s.runes() do
      result.push(c)
    end

    h.expect_eq[U64](expect.size(), result.size())

    for i in Range(0, expect.size()) do
      h.expect_eq[U32](expect(i), result(i))
    end

    true
