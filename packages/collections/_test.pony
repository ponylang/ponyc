use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestList)
    test(_TestMap)
    test(_TestMapRemove)
    test(_TestMapUpsert)
    test(_TestRing)
    test(_TestListsFrom)
    test(_TestListsMap)
    test(_TestListsFlatMap)
    test(_TestListsFilter)
    test(_TestListsFold)
    test(_TestListsEvery)
    test(_TestListsExists)
    test(_TestListsPartition)
    test(_TestListsDrop)
    test(_TestListsTake)
    test(_TestListsTakeWhile)
    test(_TestListsContains)
    test(_TestListsReverse)
    test(_TestHashSetContains)
    test(_TestSort)

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

class iso _TestListsFrom is UnitTest
  fun name(): String => "collections/Lists/from()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32].from([1, 3, 5, 7, 9])

    h.assert_eq[USize](a.size(), 5)
    h.assert_eq[U32](a(0), 1)
    h.assert_eq[U32](a(1), 3)
    h.assert_eq[U32](a(2), 5)
    h.assert_eq[U32](a(3), 7)
    h.assert_eq[U32](a(4), 9)

    let b = List[U32].from(Array[U32])
    h.assert_eq[USize](b.size(), 0)

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

    h.assert_true(a.contains("0"), "contains did not find expected element in HashMap")
    h.assert_false(a.contains("7"), "contains found unexpected element in HashMap")

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

class iso _TestMapRemove is UnitTest
  fun name(): String => "collections/Map.remove"

  fun apply(h: TestHelper) ? =>
    let x: Map[U32, String] = Map[U32,String]
    x(1) = "one"
    h.assert_eq[USize](x.size(), 1)
    x.remove(1)
    h.assert_eq[USize](x.size(), 0)

    x(2) = "two"
    h.assert_eq[USize](x.size(), 1)
    x.remove(2)
    h.assert_eq[USize](x.size(), 0)

    x(11) = "here"
    h.assert_eq[String](x(11), "here")

class iso _TestMapUpsert is UnitTest
  fun name(): String => "collections/Map.upsert"

  fun apply(h: TestHelper) ? =>
    let x: Map[U32, U64] = Map[U32,U64]
    let f = lambda(x: U64, y: U64): U64 => x + y end
    x.upsert(1, 5, f)
    h.assert_eq[U64](5, x(1))
    x.upsert(1, 3, f)
    h.assert_eq[U64](8, x(1))
    x.upsert(1, 4, f)
    h.assert_eq[U64](12, x(1))
    x.upsert(1, 2, f)
    h.assert_eq[U64](14, x(1))
    x.upsert(1, 1, f)
    h.assert_eq[U64](15, x(1))

    let x2: Map[U32, String] = Map[U32,String]
    let g = lambda(x: String, y: String): String => x + ", " + y end
    x2.upsert(1, "1", g)
    h.assert_eq[String]("1", x2(1))
    x2.upsert(1, "2", g)
    h.assert_eq[String]("1, 2", x2(1))
    x2.upsert(1, "3", g)
    h.assert_eq[String]("1, 2, 3", x2(1))
    x2.upsert(1, "abc", g)
    h.assert_eq[String]("1, 2, 3, abc", x2(1))
    x2.upsert(1, "def", g)
    h.assert_eq[String]("1, 2, 3, abc, def", x2(1))

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

class iso _TestListsMap is UnitTest
  fun name(): String => "collections/Lists/map()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2)

    let f = lambda(a: U32): U32 => a * 2 end
    let c = a.map[U32](f)

    h.assert_eq[USize](c.size(), 3)
    h.assert_eq[U32](c(0), 0)
    h.assert_eq[U32](c(1), 2)
    h.assert_eq[U32](c(2), 4)

class iso _TestListsFlatMap is UnitTest
  fun name(): String => "collections/Lists/flat_map()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2)

    let f = lambda(a: U32): List[U32] => List[U32].push(a).push(a * 2) end
    let c = a.flat_map[U32](f)

    h.assert_eq[USize](c.size(), 6)
    h.assert_eq[U32](c(0), 0)
    h.assert_eq[U32](c(1), 0)
    h.assert_eq[U32](c(2), 1)
    h.assert_eq[U32](c(3), 2)
    h.assert_eq[U32](c(4), 2)
    h.assert_eq[U32](c(5), 4)

class iso _TestListsFilter is UnitTest
  fun name(): String => "collections/Lists/filter()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2).push(3)

    let f = lambda(a: U32): Bool => a > 1 end
    let b = a.filter(f)

    h.assert_eq[USize](b.size(), 2)
    h.assert_eq[U32](b(0), 2)
    h.assert_eq[U32](b(1), 3)

class iso _TestListsFold is UnitTest
  fun name(): String => "collections/Lists/fold()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2).push(3)

    let f = lambda(acc: U32, x: U32): U32 => acc + x end
    let value = a.fold[U32](f, 0)

    h.assert_eq[U32](value, 6)

    let g = lambda(acc: List[U32], x: U32): List[U32] => acc.push(x * 2) end
    let resList = a.fold[List[U32]](g, List[U32])

    h.assert_eq[USize](resList.size(), 4)
    h.assert_eq[U32](resList(0), 0)
    h.assert_eq[U32](resList(1), 2)
    h.assert_eq[U32](resList(2), 4)
    h.assert_eq[U32](resList(3), 6)

class iso _TestListsEvery is UnitTest
  fun name(): String => "collections/Lists/every()"

  fun apply(h: TestHelper) =>
    let a = List[U32]
    a.push(0).push(1).push(2).push(3)

    let f = lambda(x: U32): Bool => x < 4 end
    let g = lambda(x: U32): Bool => x < 3 end
    let z = lambda(x: U32): Bool => x < 0 end
    let lessThan4 = a.every(f)
    let lessThan3 = a.every(g)
    let lessThan0 = a.every(z)

    h.assert_eq[Bool](lessThan4, true)
    h.assert_eq[Bool](lessThan3, false)
    h.assert_eq[Bool](lessThan0, false)

    let b = List[U32]
    let empty = b.every(f)
    h.assert_eq[Bool](empty, true)

class iso _TestListsExists is UnitTest
  fun name(): String => "collections/Lists/exists()"

  fun apply(h: TestHelper) =>
    let a = List[U32]
    a.push(0).push(1).push(2).push(3)

    let f = lambda(x: U32): Bool => x > 2 end
    let g = lambda(x: U32): Bool => x >= 0 end
    let z = lambda(x: U32): Bool => x < 0 end
    let gt2 = a.exists(f)
    let gte0 = a.exists(g)
    let lt0 = a.exists(z)

    h.assert_eq[Bool](gt2, true)
    h.assert_eq[Bool](gte0, true)
    h.assert_eq[Bool](lt0, false)

    let b = List[U32]
    let empty = b.exists(f)
    h.assert_eq[Bool](empty, false)

class iso _TestListsPartition is UnitTest
  fun name(): String => "collections/Lists/partition()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2).push(3)

    let isEven = lambda(x: U32): Bool => (x % 2) == 0 end
    (let evens, let odds) = a.partition(isEven)

    h.assert_eq[USize](evens.size(), 2)
    h.assert_eq[U32](evens(0), 0)
    h.assert_eq[U32](evens(1), 2)
    h.assert_eq[USize](odds.size(), 2)
    h.assert_eq[U32](odds(0), 1)
    h.assert_eq[U32](odds(1), 3)

    let b = List[U32]
    (let emptyEvens, let emptyOdds) = b.partition(isEven)

    h.assert_eq[USize](emptyEvens.size(), 0)
    h.assert_eq[USize](emptyOdds.size(), 0)

class iso _TestListsDrop is UnitTest
  fun name(): String => "collections/Lists/drop()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2).push(3).push(4)

    let b = a.drop(2)
    let c = a.drop(4)
    let d = a.drop(5)
    let e = a.drop(6)

    h.assert_eq[USize](b.size(), 3)
    h.assert_eq[U32](b(0), 2)
    h.assert_eq[U32](b(2), 4)
    h.assert_eq[USize](c.size(), 1)
    h.assert_eq[U32](c(0), 4)
    h.assert_eq[USize](d.size(), 0)
    h.assert_eq[USize](e.size(), 0)

    let empty = List[U32]
    let l = empty.drop(3)
    h.assert_eq[USize](l.size(), 0)

class iso _TestListsTake is UnitTest
  fun name(): String => "collections/Lists/take()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2).push(3).push(4)

    let b = a.take(2)
    let c = a.take(4)
    let d = a.take(5)
    let e = a.take(6)
    let m = a.take(0)

    h.assert_eq[USize](b.size(), 2)
    h.assert_eq[U32](b(0), 0)
    h.assert_eq[U32](b(1), 1)
    h.assert_eq[USize](c.size(), 4)
    h.assert_eq[U32](c(0), 0)
    h.assert_eq[U32](c(1), 1)
    h.assert_eq[U32](c(2), 2)
    h.assert_eq[U32](c(3), 3)
    h.assert_eq[USize](d.size(), 5)
    h.assert_eq[U32](d(0), 0)
    h.assert_eq[U32](d(1), 1)
    h.assert_eq[U32](d(2), 2)
    h.assert_eq[U32](d(3), 3)
    h.assert_eq[U32](d(4), 4)
    h.assert_eq[USize](e.size(), 5)
    h.assert_eq[U32](e(0), 0)
    h.assert_eq[U32](e(1), 1)
    h.assert_eq[U32](e(2), 2)
    h.assert_eq[U32](e(3), 3)
    h.assert_eq[U32](e(4), 4)
    h.assert_eq[USize](m.size(), 0)

    let empty = List[U32]
    let l = empty.take(3)
    h.assert_eq[USize](l.size(), 0)

class iso _TestListsTakeWhile is UnitTest
  fun name(): String => "collections/Lists/take_while()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2).push(3).push(4)

    let f = lambda(x: U32): Bool => x < 5 end
    let g = lambda(x: U32): Bool => x < 4 end
    let y = lambda(x: U32): Bool => x < 1 end
    let z = lambda(x: U32): Bool => x < 0 end
    let b = a.take_while(f)
    let c = a.take_while(g)
    let d = a.take_while(y)
    let e = a.take_while(z)

    h.assert_eq[USize](b.size(), 5)
    h.assert_eq[U32](b(0), 0)
    h.assert_eq[U32](b(1), 1)
    h.assert_eq[U32](b(2), 2)
    h.assert_eq[U32](b(3), 3)
    h.assert_eq[U32](b(4), 4)
    h.assert_eq[USize](c.size(), 4)
    h.assert_eq[U32](c(0), 0)
    h.assert_eq[U32](c(1), 1)
    h.assert_eq[U32](c(2), 2)
    h.assert_eq[U32](c(3), 3)
    h.assert_eq[USize](d.size(), 1)
    h.assert_eq[U32](d(0), 0)
    h.assert_eq[USize](e.size(), 0)

    let empty = List[U32]
    let l = empty.take_while(g)
    h.assert_eq[USize](l.size(), 0)

class iso _TestListsContains is UnitTest
  fun name(): String => "collections/Lists/contains()"

  fun apply(h: TestHelper) =>
    let a = List[U32]
    a.push(0).push(1).push(2)

    h.assert_eq[Bool](a.contains(0), true)
    h.assert_eq[Bool](a.contains(1), true)
    h.assert_eq[Bool](a.contains(2), true)
    h.assert_eq[Bool](a.contains(3), false)

class iso _TestListsReverse is UnitTest
  fun name(): String => "collections/Lists/reverse()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2)

    let b = a.reverse()

    h.assert_eq[USize](a.size(), 3)
    h.assert_eq[U32](a(0), 0)
    h.assert_eq[U32](a(1), 1)
    h.assert_eq[U32](a(2), 2)

    h.assert_eq[USize](b.size(), 3)
    h.assert_eq[U32](b(0), 2)
    h.assert_eq[U32](b(1), 1)
    h.assert_eq[U32](b(2), 0)

class iso _TestHashSetContains is UnitTest
  fun name(): String => "collections/HashSet/contains()"

  fun apply(h: TestHelper) =>
    let a = Set[U32]
    a.set(0).set(1)

    let not_found_fail = "contains did not find expected element in HashSet"
    let found_fail = "contains found unexpected element in HashSet"

    // Elements in set should be found
    h.assert_true(a.contains(0), not_found_fail)
    h.assert_true(a.contains(1), not_found_fail)

    // Elements not in set should not be found
    h.assert_false(a.contains(2), found_fail)

    // Unsetting an element should cause it to not be found
    a.unset(0)
    h.assert_false(a.contains(0), found_fail)

    // And resetting an element should cause it to be found again
    a.set(0)
    h.assert_true(a.contains(0), not_found_fail)

class iso _TestSort is UnitTest
  fun name(): String => "collections/Sort"

  fun apply(h: TestHelper) ? =>
    test_sort[USize](h,
      [23, 26, 31, 41, 53, 58, 59, 84, 93, 97],
      [31, 41, 59, 26, 53, 58, 97, 93, 23, 84])
    test_sort[I64](h,
      [-5467984, -784, 0, 0, 42, 59, 74, 238, 905, 959, 7586, 7586, 9845],
      [74, 59, 238, -784, 9845, 959, 905, 0, 0, 42, 7586, -5467984, 7586])
    test_sort[F64](h,
      [-959.7485, -784.0, 2.3, 7.8, 7.8, 59.0, 74.3, 238.2, 905, 9845.768],
      [74.3, 59.0, 238.2, -784.0, 2.3, 9845.768, -959.7485, 905, 7.8, 7.8])
    test_sort[String](h,
      ["", "%*&^*&^&", "***", "Hello", "bar", "f00", "foo", "foo"],
      ["", "Hello", "foo", "bar", "foo", "f00", "%*&^*&^&", "***"])

  fun test_sort[A: (Comparable[A] val & Stringable)](
    h: TestHelper,
    sorted: Array[A],
    unsorted: Array[A]
  ) ? =>
    h.assert_array_eq[A](sorted, Sort[A](unsorted))
