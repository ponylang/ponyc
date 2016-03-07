use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestList)
    test(_TestMap)
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

class iso _TestMap is UnitTest
  fun name(): String => "collections/Map"

  fun apply(h: TestHelper) ? =>
    let a = Map[String, U32]
    h.assert_eq[USize](0, a.size())
    h.assert_eq[USize](8, a.space())
    (a("0") = 0) as None
    (a("1") = 1) as None
    (a("2") = 2) as None
    (a("3") = 3) as None
    (a("4") = 4) as None
    (a("5") = 5) as None
    (a("6") = 6) as None

    h.assert_eq[U32](1, (a("1") = 11) as U32)
    h.assert_eq[U32](11, (a("1") = 1) as U32)

    let b = Map[String, U32]
    h.assert_eq[U32](0, b.insert("0", 0))
    h.assert_eq[U32](1, b.insert("1", 1))
    h.assert_eq[U32](2, b.insert("2", 2))
    h.assert_eq[U32](3, b.insert("3", 3))
    h.assert_eq[U32](4, b.insert("4", 4))
    h.assert_eq[U32](5, b.insert("5", 5))
    h.assert_eq[U32](6, b.insert("6", 6))

    h.assert_eq[USize](7, a.size())
    h.assert_eq[USize](16, a.space())
    h.assert_eq[USize](7, b.size())
    h.assert_eq[USize](16, b.space())

    var pair_count: USize = 0
    for (k, v) in a.pairs() do
      h.assert_eq[U32](b(k), v)
      pair_count = pair_count + 1
    end
    h.assert_eq[USize](7, pair_count)

    let a_removed_pair = a.remove("1")
    h.assert_eq[String]("1", a_removed_pair._1)
    h.assert_eq[U32](1, a_removed_pair._2)
    h.assert_eq[USize](6, a.size())
    h.assert_eq[USize](16, a.space())

    a.compact()
    h.assert_eq[USize](6, a.size())
    h.assert_eq[USize](8, a.space())

    b.clear()
    h.assert_eq[USize](0, b.size())
    h.assert_eq[USize](8, b.space())

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
