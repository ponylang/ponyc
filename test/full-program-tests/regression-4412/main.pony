use "pony_test"

// As long as #4412 is fixed, this program will compile and
// _TestIssue4412 will pass. If a regression is introduced, this
// program will fail to compile.

type Foo is (I32 | (String, I32))

actor Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  fun tag tests(test: PonyTest) =>
    test(_TestIssue4412)

class iso _TestIssue4412 is UnitTest
  fun name(): String => "_TestIssue4412"

  fun apply(h: TestHelper) =>
    // This test double-checks the correct value is extracted from the match
    h.assert_eq[I32](123, test_match(("no-parens", 123)))
    h.assert_eq[I32](123, test_match(("parens", 123)))

  fun test_match(value: Foo): I32 =>
    match value
    | ("no-parens", let x: I32) => x

    // This is the line that triggered the crash due to the extra parens
    | ("parens", (let x: I32)) => x
    else
      0
    end
