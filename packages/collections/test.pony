use "ponytest"

actor Main
  new create(env: Env) =>
    let test = PonyTest(env)
    test(_TestList)
    test(_TestRing)
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

class _TestRing iso is UnitTest
  new iso create() => None
  fun name(): String => "collections/Ring"

  fun apply(h: TestHelper): TestResult ? =>
    let a = Ring[U64](4)
    a.push(0).push(1).push(2).push(3)

    h.expect_eq[U64](a(0), 0)
    h.expect_eq[U64](a(1), 1)
    h.expect_eq[U64](a(2), 2)
    h.expect_eq[U64](a(3), 3)

    a.push(4).push(5)

    try
      a(0)
      h.assert_failed("Read ring 0")
    end

    try
      a(1)
      h.assert_failed("Read ring 1")
    end

    h.expect_eq[U64](a(2), 2)
    h.expect_eq[U64](a(3), 3)
    h.expect_eq[U64](a(4), 4)
    h.expect_eq[U64](a(5), 5)

    try
      a(6)
      h.assert_failed("Read ring 6")
    end

    true
