use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestList)
    test(_TestRing)
    test(_TestListsFrom)
    test(_TestListsMap)
    test(_TestListsFlatMap)
    test(_TestListsFlatten)
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

class iso _TestListsFrom is UnitTest
  fun name(): String => "collections/Lists/from()"

  fun apply(h: TestHelper) ? =>
    let a = Lists.from[U32]([1, 3, 5, 7, 9])

    h.assert_eq[USize](a.size(), 5)
    h.assert_eq[U32](a(0), 1)
    h.assert_eq[U32](a(1), 3)
    h.assert_eq[U32](a(2), 5)
    h.assert_eq[U32](a(3), 7)
    h.assert_eq[U32](a(4), 9)

    let b = Lists.from[U32](Array[U32])
    h.assert_eq[USize](b.size(), 0)

    true

class iso _TestListsMap is UnitTest
  fun name(): String => "collections/Lists/map()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2)

    let f = lambda(a: U32): U32 => consume a * 2 end
    let c = Lists.map[U32,U32](a, f)

    h.assert_eq[USize](c.size(), 3)
    h.assert_eq[U32](c(0), 0)
    h.assert_eq[U32](c(1), 2)
    h.assert_eq[U32](c(2), 4)

    true

class iso _TestListsFlatMap is UnitTest
  fun name(): String => "collections/Lists/flat_map()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2)

    let f = lambda(a: U32): List[U32] => List[U32].push(consume a * 2) end
    let c = Lists.flat_map[U32,U32](a, f)

    h.assert_eq[USize](c.size(), 3)
    h.assert_eq[U32](c(0), 0)
    h.assert_eq[U32](c(1), 2)
    h.assert_eq[U32](c(2), 4)

    true

class iso _TestListsFlatten is UnitTest
  fun name(): String => "collections/Lists/flatten()"

  fun apply(h: TestHelper) ? =>
    let a = List[List[U32]]
    let l1 = List[U32].push(0).push(1)
    let l2 = List[U32].push(2).push(3)
    let l3 = List[U32].push(4)
    a.push(l1).push(l2).push(l3)

    let b: List[U32] = Lists.flatten[U32](a)

    h.assert_eq[USize](b.size(), 5)
    h.assert_eq[U32](b(0), 0)
    h.assert_eq[U32](b(1), 1)
    h.assert_eq[U32](b(2), 2)
    h.assert_eq[U32](b(3), 3)
    h.assert_eq[U32](b(4), 4)

    let c = List[List[U32]]
    let d = Lists.flatten[U32](c)
    h.assert_eq[USize](d.size(), 0)

    true

class iso _TestListsFilter is UnitTest
  fun name(): String => "collections/Lists/filter()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2).push(3)

    let f = lambda(a: U32): Bool => consume a > 1 end
    let b = Lists.filter[U32](a, f)

    h.assert_eq[USize](b.size(), 2)
    h.assert_eq[U32](b(0), 2)
    h.assert_eq[U32](b(1), 3)

    true

class iso _TestListsFold is UnitTest
  fun name(): String => "collections/Lists/fold()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2).push(3)

    let f = lambda(acc: U32, x: U32): U32 => acc + x end
    let value = Lists.fold[U32,U32](a, f, 0)

    h.assert_eq[U32](value, 6)

    let g = lambda(acc: List[U32], x: U32): List[U32] => acc.push(x * 2) end
    let resList = Lists.fold[U32,List[U32]](a, g, List[U32])

    h.assert_eq[USize](resList.size(), 4)
    h.assert_eq[U32](resList(0), 0)
    h.assert_eq[U32](resList(1), 2)
    h.assert_eq[U32](resList(2), 4)
    h.assert_eq[U32](resList(3), 6)

    true

class iso _TestListsEvery is UnitTest
  fun name(): String => "collections/Lists/every()"

  fun apply(h: TestHelper) =>
    let a = List[U32]
    a.push(0).push(1).push(2).push(3)

    let f = lambda(x: U32): Bool => x < 4 end
    let g = lambda(x: U32): Bool => x < 3 end
    let z = lambda(x: U32): Bool => x < 0 end
    let lessThan4 = Lists.every[U32](a, f)
    let lessThan3 = Lists.every[U32](a, g)
    let lessThan0 = Lists.every[U32](a, z)

    h.assert_eq[Bool](lessThan4, true)
    h.assert_eq[Bool](lessThan3, false)
    h.assert_eq[Bool](lessThan0, false)

    let b = List[U32]
    let empty = Lists.every[U32](b, f)
    h.assert_eq[Bool](empty, true)

    true

class iso _TestListsExists is UnitTest
  fun name(): String => "collections/Lists/exists()"

  fun apply(h: TestHelper) =>
    let a = List[U32]
    a.push(0).push(1).push(2).push(3)

    let f = lambda(x: U32): Bool => x > 2 end
    let g = lambda(x: U32): Bool => x >= 0 end
    let z = lambda(x: U32): Bool => x < 0 end
    let gt2 = Lists.exists[U32](a, f)
    let gte0 = Lists.exists[U32](a, g)
    let lt0 = Lists.exists[U32](a, z)

    h.assert_eq[Bool](gt2, true)
    h.assert_eq[Bool](gte0, true)
    h.assert_eq[Bool](lt0, false)

    let b = List[U32]
    let empty = Lists.exists[U32](b, f)
    h.assert_eq[Bool](empty, false)

    true

class iso _TestListsPartition is UnitTest
  fun name(): String => "collections/Lists/partition()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2).push(3)

    let isEven = lambda(x: U32): Bool => x % 2 == 0 end
    (let evens, let odds) = Lists.partition[U32](a, isEven)

    h.assert_eq[USize](evens.size(), 2)
    h.assert_eq[U32](evens(0), 0)
    h.assert_eq[U32](evens(1), 2)
    h.assert_eq[USize](odds.size(), 2)
    h.assert_eq[U32](odds(0), 1)
    h.assert_eq[U32](odds(1), 3)

    let b = List[U32]
    (let emptyEvens, let emptyOdds) = Lists.partition[U32](b, isEven)

    h.assert_eq[USize](emptyEvens.size(), 0)
    h.assert_eq[USize](emptyOdds.size(), 0)

    true

class iso _TestListsDrop is UnitTest
  fun name(): String => "collections/Lists/drop()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2).push(3).push(4)

    let b = Lists.drop[U32](a, 2)
    let c = Lists.drop[U32](a, 4)
    let d = Lists.drop[U32](a, 5)
    let e = Lists.drop[U32](a, 6)

    h.assert_eq[USize](b.size(), 3)
    h.assert_eq[U32](b(0), 2)
    h.assert_eq[U32](b(2), 4)
    h.assert_eq[USize](c.size(), 1)
    h.assert_eq[U32](c(0), 4)
    h.assert_eq[USize](d.size(), 0)
    h.assert_eq[USize](e.size(), 0)

    let empty = List[U32]
    let l = Lists.drop[U32](empty, 3)
    h.assert_eq[USize](l.size(), 0)

    true

class iso _TestListsTake is UnitTest
  fun name(): String => "collections/Lists/take()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2).push(3).push(4)

    let b = Lists.take[U32](a, 2)
    let c = Lists.take[U32](a, 4)
    let d = Lists.take[U32](a, 5)
    let e = Lists.take[U32](a, 6)
    let m = Lists.take[U32](a, 0)

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
    let l = Lists.take[U32](empty, 3)
    h.assert_eq[USize](l.size(), 0)

    true

class iso _TestListsTakeWhile is UnitTest
  fun name(): String => "collections/Lists/take_while()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2).push(3).push(4)

    let f = lambda(x: U32): Bool => x < 5 end
    let g = lambda(x: U32): Bool => x < 4 end
    let y = lambda(x: U32): Bool => x < 1 end
    let z = lambda(x: U32): Bool => x < 0 end
    let b = Lists.take_while[U32](a, f)
    let c = Lists.take_while[U32](a, g)
    let d = Lists.take_while[U32](a, y)
    let e = Lists.take_while[U32](a, z)

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
    let l = Lists.take_while[U32](empty, g)
    h.assert_eq[USize](l.size(), 0)

    true

class iso _TestListsContains is UnitTest
  fun name(): String => "collections/Lists/contains()"

  fun apply(h: TestHelper) =>
    let a = List[U32]
    a.push(0).push(1).push(2)

    h.assert_eq[Bool](Lists.contains[U32](a, 0), true)
    h.assert_eq[Bool](Lists.contains[U32](a, 3), false)

    true

class iso _TestListsReverse is UnitTest
  fun name(): String => "collections/Lists/reverse()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a.push(0).push(1).push(2)

    let b = Lists.reverse[U32](a)


    h.assert_eq[USize](a.size(), 3)
    h.assert_eq[U32](a(0), 0)
    h.assert_eq[U32](a(1), 1)
    h.assert_eq[U32](a(2), 2)

    h.assert_eq[USize](b.size(), 3)
    h.assert_eq[U32](b(0), 2)
    h.assert_eq[U32](b(1), 1)
    h.assert_eq[U32](b(2), 0)

    true
