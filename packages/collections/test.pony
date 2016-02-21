use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestList)
    test(_TestRing)

class iso _TestList is UnitTest
  fun name(): String => "collections/List"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2)

    let b = a.clone()
    h.assert_eq[USize](b.size(), 3)
    h.assert_eq[U32](b(0), 0)
    h.assert_eq[U32](b(1), 1)
    h.assert_eq[U32](b(2), 2)

    b.remove(1)
    h.assert_eq[USize](b.size(), 2)
    h.assert_eq[U32](b(0), 0)
    h.assert_eq[U32](b(1), 2)

    b.append_list(a)
    h.assert_eq[USize](b.size(), 5)
    h.assert_eq[U32](b(0), 0)
    h.assert_eq[U32](b(1), 2)
    h.assert_eq[U32](b(2), 0)
    h.assert_eq[U32](b(3), 1)
    h.assert_eq[U32](b(4), 2)

class iso _TestRing is UnitTest
  fun name(): String => "collections/RingBuffer"

  fun apply(h: TestHelper) ? =>
    let a = RingBuffer[U64](4)
    a.push(0).push(1).push(2).push(3)

    h.assert_eq[U64](a(0), 0)
    h.assert_eq[U64](a(1), 1)
    h.assert_eq[U64](a(2), 2)
    h.assert_eq[U64](a(3), 3)

    a.push(4).push(5)

    h.assert_error(lambda()(a)? => a(0) end, "Read ring 0")
    h.assert_error(lambda()(a)? => a(1) end, "Read ring 1")

    h.assert_eq[U64](a(2), 2)
    h.assert_eq[U64](a(3), 3)
    h.assert_eq[U64](a(4), 4)
    h.assert_eq[U64](a(5), 5)
    
    h.assert_error(lambda()(a)? => a(6) end, "Read ring 6")
