use "ponytest"

actor Main
  new create(env: Env) =>
    let test = PonyTest(env)
    test(_TestList)
    test.complete()

class _TestList iso is UnitTest
  new iso create() => None
  fun name(): String => "collections/List"

  fun apply(h: TestHelper): TestResult ? =>
    let a = List[U32]
    a.push(0).push(1).push(2)

    let b = a.clone()
    h.expect_eq[U64](b.size(), 3)
    h.expect_eq[U32](b(0), 0)
    h.expect_eq[U32](b(1), 1)
    h.expect_eq[U32](b(2), 2)

    b.remove(1)
    h.expect_eq[U64](b.size(), 2)
    h.expect_eq[U32](b(1), 2)

    true
