use "pony_test"
use "random"
use "time"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    test(_TestFlags)
    test(_TestHashSetContains)
    test(_TestHashSetIntersect)
    test(_TestHeap)
    test(_TestList)
    test(_TestListsContains)
    test(_TestListsDrop)
    test(_TestListsEvery)
    test(_TestListsExists)
    test(_TestListsFilter)
    test(_TestListsFlatMap)
    test(_TestListsFold)
    test(_TestListsFrom)
    test(_TestListsMap)
    test(_TestListsPartition)
    test(_TestListsReverse)
    test(_TestListsTake)
    test(_TestListsTakeWhile)
    test(_TestMap)
    test(_TestMapHashFunc)
    test(_TestMapRemove)
    test(_TestMapUpsert)
    test(_TestRange)
    test(_TestRing)
    test(_TestSort)

class \nodoc\ iso _TestList is UnitTest
  fun name(): String => "collections/List"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a .> push(0) .> push(1) .> push(2)

    let b: List[U32] ref = a.clone()
    h.assert_eq[USize](b.size(), 3)
    h.assert_eq[U32](b(0)?, 0)
    h.assert_eq[U32](b(1)?, 1)
    h.assert_eq[U32](b(2)?, 2)

    b.remove(1)?
    h.assert_eq[USize](b.size(), 2)
    h.assert_eq[U32](b(0)?, 0)
    h.assert_eq[U32](b(1)?, 2)

    b.append_list(a)
    h.assert_eq[USize](b.size(), 5)
    h.assert_eq[U32](b(0)?, 0)
    h.assert_eq[U32](b(1)?, 2)
    h.assert_eq[U32](b(2)?, 0)
    h.assert_eq[U32](b(3)?, 1)
    h.assert_eq[U32](b(4)?, 2)

class \nodoc\ iso _TestListsFrom is UnitTest
  fun name(): String => "collections/Lists/from()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32].from([1; 3; 5; 7; 9])

    h.assert_eq[USize](a.size(), 5)
    h.assert_eq[U32](a(0)?, 1)
    h.assert_eq[U32](a(1)?, 3)
    h.assert_eq[U32](a(2)?, 5)
    h.assert_eq[U32](a(3)?, 7)
    h.assert_eq[U32](a(4)?, 9)

    let b = List[U32].from(Array[U32])
    h.assert_eq[USize](b.size(), 0)

class \nodoc\ iso _TestMap is UnitTest
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

    h.assert_true(
      a.contains("0"),
      "contains did not find expected element in HashMap")
    h.assert_false(
      a.contains("7"),
      "contains found unexpected element in HashMap")

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
      h.assert_eq[U32](b(k)?, v)
      pair_count = pair_count + 1
    end
    h.assert_eq[USize](7, pair_count)

    let a_removed_pair = a.remove("1")?
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

    let c = Map[String, U32]
    c.insert_if_absent("a", 1)
    h.assert_eq[U32](1, c("a")?)
    c.insert_if_absent("a", 2)
    h.assert_eq[U32](1, c("a")?)
    h.assert_eq[U32](0, c.insert_if_absent("0", 0))
    h.assert_eq[U32](0, c.insert_if_absent("0", 1))

    let d = c - "a"
    h.assert_error({() ? => d("a")? })

    let e = d + ("a", 1)
    h.assert_eq[U32](1, e("a")?)

class \nodoc\ iso _TestMapRemove is UnitTest
  fun name(): String => "collections/Map.remove"

  fun apply(h: TestHelper) ? =>
    let x: Map[U32, String] = Map[U32,String]
    x(1) = "one"
    h.assert_eq[USize](x.size(), 1)
    x.remove(1)?
    h.assert_eq[USize](x.size(), 0)

    x(2) = "two"
    h.assert_eq[USize](x.size(), 1)
    x.remove(2)?
    h.assert_eq[USize](x.size(), 0)

    x(11) = "here"
    h.assert_eq[String](x(11)?, "here")

class \nodoc\ iso _TestMapUpsert is UnitTest
  fun name(): String => "collections/Map.upsert"

  fun apply(h: TestHelper) ? =>
    let x: Map[U32, U64] = Map[U32,U64]
    let f = {(x: U64, y: U64): U64 => x + y }
    x.upsert(1, 5, f)
    h.assert_eq[U64](5, x(1)?)
    x.upsert(1, 3, f)
    h.assert_eq[U64](8, x(1)?)
    x.upsert(1, 4, f)
    h.assert_eq[U64](12, x(1)?)
    x.upsert(1, 2, f)
    h.assert_eq[U64](14, x(1)?)
    x.upsert(1, 1, f)
    h.assert_eq[U64](15, x(1)?)

    let x2: Map[U32, String] = Map[U32,String]
    let g = {(x: String, y: String): String => x + ", " + y }
    x2.upsert(1, "1", g)
    h.assert_eq[String]("1", x2(1)?)
    x2.upsert(1, "2", g)
    h.assert_eq[String]("1, 2", x2(1)?)
    x2.upsert(1, "3", g)
    h.assert_eq[String]("1, 2, 3", x2(1)?)
    x2.upsert(1, "abc", g)
    h.assert_eq[String]("1, 2, 3, abc", x2(1)?)
    x2.upsert(1, "def", g)
    h.assert_eq[String]("1, 2, 3, abc, def", x2(1)?)

    // verify correct results when we trigger a resize
    let prealloc: USize = 6
    let expected_initial_size: USize = (prealloc * 4) / 3
    let x3: Map[U32, U64] = Map[U32,U64](prealloc)
    let f' = {(x: U64, y: U64): U64 => x + y }
    h.assert_eq[USize](expected_initial_size, x3.space())
    h.assert_eq[U64](1, x3.upsert(1, 1, f'))
    h.assert_eq[U64](1, x3.upsert(2, 1, f'))
    h.assert_eq[U64](1, x3.upsert(3, 1, f'))
    h.assert_eq[U64](1, x3.upsert(4, 1, f'))
    h.assert_eq[U64](1, x3.upsert(5, 1, f'))
    h.assert_eq[U64](1, x3.upsert(6, 1, f'))
    h.assert_eq[U64](1, x3.upsert(7, 1, f'))
    h.assert_eq[U64](2, x3.upsert(5, 1, f'))
    h.assert_ne[USize](expected_initial_size, x3.space())

    let x4: Map[I32, I64] = Map[I32,I64]
    let i = {(x: I64, y: I64): I64 => x - y }
    x4.upsert(2, 4, i)
    h.assert_eq[I64](4, x4(2)?)
    x4.upsert(2, 4, i)
    h.assert_eq[I64](0, x4(2)?)
    x4.upsert(2, 4, i)
    h.assert_eq[I64](-4, x4(2)?)

class \nodoc\ iso _TestMapHashFunc is UnitTest
  fun name(): String => "collections/Map.hashfunc"

  fun apply(h: TestHelper) =>
    let a1: Array[U8] = [65; 77; 83; 0; 0; 39; 15; 0; 6; 200; 28; 0; 0; 0; 107]
    let a2: Array[U8] = [65; 77; 83; 0; 0; 39; 15; 0; 6; 200; 28; 0; 0; 0; 173]
    let h1: USize = @ponyint_hash_block(a1.cpointer(), a1.size())
    let h2: USize = @ponyint_hash_block(a2.cpointer(), a2.size())

    ifdef ilp32 then
      // I cannot find agreement on easy-to-find reference implementations
      // of half siphash-2-4.  These are the numbers to expect after
      // fixing GitHub bug 2979.  See comments in fun.c for details.
      h.assert_eq[USize](h1, 2478478751)
      h.assert_eq[USize](h2, 2780506380)
    else
      // The expected values on the right are generated by two reference
      // implementations:
      // C via NetBSD's src/external/mit/expat/dist/lib/siphash.h
      // C++ via
      // https://github.com/bloomberg/bde/blob/master/groups/bsl/bslh/bslh_siphashalgorithm.cpp
      h.assert_eq[USize](h1, 13698363685738975395)
      h.assert_eq[USize](h2, 17028932028986796913)
    end

class \nodoc\ iso _TestRing is UnitTest
  fun name(): String => "collections/RingBuffer"

  fun apply(h: TestHelper) ? =>
    let a = RingBuffer[U64](4)
    a .> push(0) .> push(1) .> push(2) .> push(3)

    h.assert_eq[U64](a(0)?, 0)
    h.assert_eq[U64](a(1)?, 1)
    h.assert_eq[U64](a(2)?, 2)
    h.assert_eq[U64](a(3)?, 3)

    a .> push(4) .> push(5)

    h.assert_error({() ? => a(0)? }, "Read ring 0")
    h.assert_error({() ? => a(1)? }, "Read ring 1")

    h.assert_eq[U64](a(2)?, 2)
    h.assert_eq[U64](a(3)?, 3)
    h.assert_eq[U64](a(4)?, 4)
    h.assert_eq[U64](a(5)?, 5)

    h.assert_error({() ? => a(6)? }, "Read ring 6")

class \nodoc\ iso _TestListsMap is UnitTest
  fun name(): String => "collections/Lists/map()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a .> push(0) .> push(1) .> push(2)

    let f = {(a: U32): U32 => a * 2 }
    let c = a.map[U32](f)

    h.assert_eq[USize](c.size(), 3)
    h.assert_eq[U32](c(0)?, 0)
    h.assert_eq[U32](c(1)?, 2)
    h.assert_eq[U32](c(2)?, 4)

class \nodoc\ iso _TestListsFlatMap is UnitTest
  fun name(): String => "collections/Lists/flat_map()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a .> push(0) .> push(1) .> push(2)

    let f = {(a: U32): List[U32] => List[U32] .> push(a) .> push(a * 2) }
    let c = a.flat_map[U32](f)

    h.assert_eq[USize](c.size(), 6)
    h.assert_eq[U32](c(0)?, 0)
    h.assert_eq[U32](c(1)?, 0)
    h.assert_eq[U32](c(2)?, 1)
    h.assert_eq[U32](c(3)?, 2)
    h.assert_eq[U32](c(4)?, 2)
    h.assert_eq[U32](c(5)?, 4)

class \nodoc\ iso _TestListsFilter is UnitTest
  fun name(): String => "collections/Lists/filter()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a .> push(0) .> push(1) .> push(2) .> push(3)

    let f = {(a: U32): Bool => a > 1 }
    let b = a.filter(f)

    h.assert_eq[USize](b.size(), 2)
    h.assert_eq[U32](b(0)?, 2)
    h.assert_eq[U32](b(1)?, 3)

class \nodoc\ iso _TestListsFold is UnitTest
  fun name(): String => "collections/Lists/fold()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a .> push(0) .> push(1) .> push(2) .> push(3)

    let f = {(acc: U32, x: U32): U32 => acc + x }
    let value = a.fold[U32](f, 0)

    h.assert_eq[U32](value, 6)

    let g = {(acc: List[U32], x: U32): List[U32] => acc .> push(x * 2) }
    let res_list = a.fold[List[U32]](g, List[U32])

    h.assert_eq[USize](res_list.size(), 4)
    h.assert_eq[U32](res_list(0)?, 0)
    h.assert_eq[U32](res_list(1)?, 2)
    h.assert_eq[U32](res_list(2)?, 4)
    h.assert_eq[U32](res_list(3)?, 6)

class \nodoc\ iso _TestListsEvery is UnitTest
  fun name(): String => "collections/Lists/every()"

  fun apply(h: TestHelper) =>
    let a = List[U32]
    a .> push(0) .> push(1) .> push(2) .> push(3)

    let f = {(x: U32): Bool => x < 4 }
    let g = {(x: U32): Bool => x < 3 }
    let z = {(x: U32): Bool => x < 0 }
    let less_than_4 = a.every(f)
    let less_than_3 = a.every(g)
    let less_than_0 = a.every(z)

    h.assert_eq[Bool](less_than_4, true)
    h.assert_eq[Bool](less_than_3, false)
    h.assert_eq[Bool](less_than_0, false)

    let b = List[U32]
    let empty = b.every(f)
    h.assert_eq[Bool](empty, true)

class \nodoc\ iso _TestListsExists is UnitTest
  fun name(): String => "collections/Lists/exists()"

  fun apply(h: TestHelper) =>
    let a = List[U32]
    a .> push(0) .> push(1) .> push(2) .> push(3)

    let f = {(x: U32): Bool => x > 2 }
    let g = {(x: U32): Bool => x >= 0 }
    let z = {(x: U32): Bool => x < 0 }
    let gt2 = a.exists(f)
    let gte0 = a.exists(g)
    let lt0 = a.exists(z)

    h.assert_eq[Bool](gt2, true)
    h.assert_eq[Bool](gte0, true)
    h.assert_eq[Bool](lt0, false)

    let b = List[U32]
    let empty = b.exists(f)
    h.assert_eq[Bool](empty, false)

class \nodoc\ iso _TestListsPartition is UnitTest
  fun name(): String => "collections/Lists/partition()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a .> push(0) .> push(1) .> push(2) .> push(3)

    let is_even = {(x: U32): Bool => (x % 2) == 0 }
    (let evens, let odds) = a.partition(is_even)

    h.assert_eq[USize](evens.size(), 2)
    h.assert_eq[U32](evens(0)?, 0)
    h.assert_eq[U32](evens(1)?, 2)
    h.assert_eq[USize](odds.size(), 2)
    h.assert_eq[U32](odds(0)?, 1)
    h.assert_eq[U32](odds(1)?, 3)

    let b = List[U32]
    (let empty_evens, let empty_odds) = b.partition(is_even)

    h.assert_eq[USize](empty_evens.size(), 0)
    h.assert_eq[USize](empty_odds.size(), 0)

class \nodoc\ iso _TestListsDrop is UnitTest
  fun name(): String => "collections/Lists/drop()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a .> push(0) .> push(1) .> push(2) .> push(3) .> push(4)

    let b = a.drop(2)
    let c = a.drop(4)
    let d = a.drop(5)
    let e = a.drop(6)

    h.assert_eq[USize](b.size(), 3)
    h.assert_eq[U32](b(0)?, 2)
    h.assert_eq[U32](b(2)?, 4)
    h.assert_eq[USize](c.size(), 1)
    h.assert_eq[U32](c(0)?, 4)
    h.assert_eq[USize](d.size(), 0)
    h.assert_eq[USize](e.size(), 0)

    let empty = List[U32]
    let l = empty.drop(3)
    h.assert_eq[USize](l.size(), 0)

class \nodoc\ iso _TestListsTake is UnitTest
  fun name(): String => "collections/Lists/take()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a .> push(0) .> push(1) .> push(2) .> push(3) .> push(4)

    let b = a.take(2)
    let c = a.take(4)
    let d = a.take(5)
    let e = a.take(6)
    let m = a.take(0)

    h.assert_eq[USize](b.size(), 2)
    h.assert_eq[U32](b(0)?, 0)
    h.assert_eq[U32](b(1)?, 1)
    h.assert_eq[USize](c.size(), 4)
    h.assert_eq[U32](c(0)?, 0)
    h.assert_eq[U32](c(1)?, 1)
    h.assert_eq[U32](c(2)?, 2)
    h.assert_eq[U32](c(3)?, 3)
    h.assert_eq[USize](d.size(), 5)
    h.assert_eq[U32](d(0)?, 0)
    h.assert_eq[U32](d(1)?, 1)
    h.assert_eq[U32](d(2)?, 2)
    h.assert_eq[U32](d(3)?, 3)
    h.assert_eq[U32](d(4)?, 4)
    h.assert_eq[USize](e.size(), 5)
    h.assert_eq[U32](e(0)?, 0)
    h.assert_eq[U32](e(1)?, 1)
    h.assert_eq[U32](e(2)?, 2)
    h.assert_eq[U32](e(3)?, 3)
    h.assert_eq[U32](e(4)?, 4)
    h.assert_eq[USize](m.size(), 0)

    let empty = List[U32]
    let l = empty.take(3)
    h.assert_eq[USize](l.size(), 0)

class \nodoc\ iso _TestListsTakeWhile is UnitTest
  fun name(): String => "collections/Lists/take_while()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a .> push(0) .> push(1) .> push(2) .> push(3) .> push(4)

    let f = {(x: U32): Bool => x < 5 }
    let g = {(x: U32): Bool => x < 4 }
    let y = {(x: U32): Bool => x < 1 }
    let z = {(x: U32): Bool => x < 0 }
    let b = a.take_while(f)
    let c = a.take_while(g)
    let d = a.take_while(y)
    let e = a.take_while(z)

    h.assert_eq[USize](b.size(), 5)
    h.assert_eq[U32](b(0)?, 0)
    h.assert_eq[U32](b(1)?, 1)
    h.assert_eq[U32](b(2)?, 2)
    h.assert_eq[U32](b(3)?, 3)
    h.assert_eq[U32](b(4)?, 4)
    h.assert_eq[USize](c.size(), 4)
    h.assert_eq[U32](c(0)?, 0)
    h.assert_eq[U32](c(1)?, 1)
    h.assert_eq[U32](c(2)?, 2)
    h.assert_eq[U32](c(3)?, 3)
    h.assert_eq[USize](d.size(), 1)
    h.assert_eq[U32](d(0)?, 0)
    h.assert_eq[USize](e.size(), 0)

    let empty = List[U32]
    let l = empty.take_while(g)
    h.assert_eq[USize](l.size(), 0)

class \nodoc\ iso _TestListsContains is UnitTest
  fun name(): String => "collections/Lists/contains()"

  fun apply(h: TestHelper) =>
    let a = List[U32]
    a .> push(0) .> push(1) .> push(2)

    h.assert_eq[Bool](a.contains(0), true)
    h.assert_eq[Bool](a.contains(1), true)
    h.assert_eq[Bool](a.contains(2), true)
    h.assert_eq[Bool](a.contains(3), false)

class \nodoc\ iso _TestListsReverse is UnitTest
  fun name(): String => "collections/Lists/reverse()"

  fun apply(h: TestHelper) ? =>
    let a = List[U32]
    a .> push(0) .> push(1) .> push(2)

    let b = a.reverse()

    h.assert_eq[USize](a.size(), 3)
    h.assert_eq[U32](a(0)?, 0)
    h.assert_eq[U32](a(1)?, 1)
    h.assert_eq[U32](a(2)?, 2)

    h.assert_eq[USize](b.size(), 3)
    h.assert_eq[U32](b(0)?, 2)
    h.assert_eq[U32](b(1)?, 1)
    h.assert_eq[U32](b(2)?, 0)

class \nodoc\ iso _TestHashSetContains is UnitTest
  fun name(): String => "collections/HashSet/contains()"

  fun apply(h: TestHelper) =>
    let a = Set[U32]
    a .> set(0) .> set(1)

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

class \nodoc\ iso _TestHashSetIntersect is UnitTest
  fun name(): String => "collections/HashSet/intersect()"

  fun apply(h: TestHelper) =>
    let tests =
      [ as (Array[U32], Array[U32], Array[U32]):
        ([1; 2; 3; 4; 5; 6; 10; 60], [1; 7], [1])
        ([1; 2; 3], [3; 2], [2; 3])
        ([1; 2; 3], [], [])
        ([], [1; 2], [])
        ([], [], [])
      ]

    for test in tests.values() do
      let sa = Set[U32] .> union(test._1.values())
      let sb = Set[U32] .> union(test._2.values())
      let intersect = sa .> intersect(sb)
      h.assert_eq[USize](intersect.size(), test._3.size())
      for v in test._3.values() do
        h.assert_true(intersect.contains(v))
      end
    end

class \nodoc\ iso _TestSort is UnitTest
  fun name(): String => "collections/Sort"

  fun apply(h: TestHelper) =>
    test_sort[USize](
      h,
      [23; 26; 31; 41; 53; 58; 59; 84; 93; 97],
      [31; 41; 59; 26; 53; 58; 97; 93; 23; 84])
    test_sort[I64](
      h,
      [-5467984; -784; 0; 0; 42; 59; 74; 238; 905; 959; 7586; 7586; 9845],
      [74; 59; 238; -784; 9845; 959; 905; 0; 0; 42; 7586; -5467984; 7586])
    test_sort[F64](
      h,
      [-959.7485; -784.0; 2.3; 7.8; 7.8; 59.0; 74.3; 238.2; 905; 9845.768],
      [74.3; 59.0; 238.2; -784.0; 2.3; 9845.768; -959.7485; 905; 7.8; 7.8])
    test_sort[String](
      h,
      [""; "%*&^*&^&"; "***"; "Hello"; "bar"; "f00"; "foo"; "foo"],
      [""; "Hello"; "foo"; "bar"; "foo"; "f00"; "%*&^*&^&"; "***"])

  fun test_sort[A: (Comparable[A] val & Stringable val)](
    h: TestHelper,
    sorted: Array[A],
    unsorted: Array[A])
  =>
    h.assert_array_eq[A](sorted, Sort[Array[A], A](unsorted))

class \nodoc\ iso _TestRange is UnitTest
  fun name(): String => "collections/Range"

  fun apply(h: TestHelper) ? =>
    // till — ascending
    _expect[USize](h, Range.till(0, 5), [0; 1; 2; 3; 4])
    _expect[U8](h, Range[U8].till(0, 3), [0; 1; 2])
    _expect[I8](h, Range[I8].till(-2, 2), [-2; -1; 0; 1])
    _expect[ISize](h, Range[ISize].till(-10, -7), [-10; -9; -8])

    // till — descending
    _expect[USize](h, Range.till(5, 0), [5; 4; 3; 2; 1])
    _expect[I8](h, Range[I8].till(2, -2), [2; 1; 0; -1])
    _expect[ISize](h, Range[ISize].till(1, -1), [1; 0])

    // through — ascending
    _expect[USize](h, Range.through(0, 5), [0; 1; 2; 3; 4; 5])
    _expect[U8](h, Range[U8].through(0, 3), [0; 1; 2; 3])
    _expect[I8](h, Range[I8].through(-2, 2), [-2; -1; 0; 1; 2])

    // through — descending
    _expect[USize](h, Range.through(5, 0), [5; 4; 3; 2; 1; 0])
    _expect[I32](h, Range[I32].through(2, -2), [2; 1; 0; -1; -2])
    _expect[ISize](h, Range[ISize].through(1, -1), [1; 0; -1])

    // step > 1
    _expect[USize](h, Range.till(0, 10, 3), [0; 3; 6; 9])
    _expect[USize](h, Range.through(0, 9, 3), [0; 3; 6; 9])
    _expect[I32](h, Range[I32].till(10, 0, 3), [10; 7; 4; 1])
    _expect[I32](h, Range[I32].through(9, 0, 3), [9; 6; 3; 0])
    _expect[USize](h, Range.till(0, 10, 2), [0; 2; 4; 6; 8])
    _expect[USize](h, Range.through(0, 10, 2), [0; 2; 4; 6; 8; 10])
    _expect[USize](h, Range.till(0, 1, 2), [0])
    _expect[USize](h, Range.through(0, 1, 2), [0])

    // single element via through(n, n)
    _expect[USize](h, Range.through(5, 5), [5])
    _expect[I8](h, Range[I8].through(0, 0), [0])
    _expect[F64](h, Range[F64].through(1.0, 1.0), [1.0])

    // empty via till(n, n)
    _expect[USize](h, Range.till(5, 5), Array[USize])
    _expect[I8](h, Range[I8].till(0, 0), Array[I8])

    // overflow safety — unsigned descending past zero
    _expect[U8](h, Range[U8].through(2, 0), [2; 1; 0])
    _expect[USize](h, Range.through(2, 0), [2; 1; 0])

    // overflow safety — unsigned ascending near max
    _expect[U8](h, Range[U8].through(250, 255), [250; 251; 252; 253; 254; 255])

    // overflow safety — full range (must terminate, not loop forever)
    let full_asc = Array[U8] .> concat(Range[U8].through(0, 255))
    h.assert_eq[USize](full_asc.size(), 256)
    h.assert_eq[U8](full_asc(0)?, 0)
    h.assert_eq[U8](full_asc(255)?, 255)

    let full_desc = Array[U8] .> concat(Range[U8].through(255, 0))
    h.assert_eq[USize](full_desc.size(), 256)
    h.assert_eq[U8](full_desc(0)?, 255)
    h.assert_eq[U8](full_desc(255)?, 0)

    let full_i8 = Array[I8] .> concat(Range[I8].through(-128, 127))
    h.assert_eq[USize](full_i8.size(), 256)
    h.assert_eq[I8](full_i8(0)?, -128)
    h.assert_eq[I8](full_i8(255)?, 127)

    let full_i8_desc = Array[I8] .> concat(Range[I8].through(127, -128))
    h.assert_eq[USize](full_i8_desc.size(), 256)
    h.assert_eq[I8](full_i8_desc(0)?, 127)
    h.assert_eq[I8](full_i8_desc(255)?, -128)

    // overflow safety — step-induced wraparound
    let step_wrap = Array[U8] .> concat(Range[U8].till(0, 255, 2))
    h.assert_eq[USize](step_wrap.size(), 128)
    h.assert_eq[U8](step_wrap(0)?, 0)
    h.assert_eq[U8](step_wrap(127)?, 254)
    _expect[U8](h, Range[U8].through(253, 255, 5), [253])

    // step > 1 — unsigned descending
    _expect[USize](h, Range.till(10, 0, 3), [10; 7; 4; 1])
    _expect[USize](h, Range.through(9, 0, 3), [9; 6; 3; 0])

    // float
    _expect[F32](h, Range[F32].till(-0.1, 0.2, 0.1), [-0.1; 0.0; 0.1])
    _expect[F64](h, Range[F64].till(-0.1, 0.2, 0.1), [-0.1; 0.0; 0.1])
    _expect[F64](h, Range[F64].through(0.2, -0.1, 0.1), [0.2; 0.1; 0.0; -0.1])
    _expect[F32](h, Range[F32].through(0.2, -0.1, 0.1), [0.2; 0.1; 0.0; -0.1])
    _expect[F32](h, Range[F32].till(0.2, -0.1, 0.1), [0.2; 0.1; 0.0])
    _expect[F32](h, Range[F32].till(0.0, F32.epsilon(), F32.epsilon()), [0.0])
    _expect[F64](h, Range[F64].till(0.0, F64.epsilon(), F64.epsilon()), [0.0])

    // empty range on step <= 0
    _expect[USize](h, Range.till(0, 10, 0), Array[USize])
    _expect[USize](h, Range.through(0, 10, 0), Array[USize])
    _expect[I32](h, Range[I32].till(0, 10, -1), Array[I32])
    _expect[I32](h, Range[I32].through(0, 10, -1), Array[I32])

    // empty range on non-finite floats
    let p_inf = F64.max_value() + F64.max_value()
    let n_inf = -p_inf
    let nan = F64(0) / F64(0)
    _expect[F64](h, Range[F64].till(nan, 10, 1), Array[F64])
    _expect[F64](h, Range[F64].till(0, nan, 1), Array[F64])
    _expect[F64](h, Range[F64].till(0, 10, nan), Array[F64])
    _expect[F64](h, Range[F64].till(p_inf, 10, 1), Array[F64])
    _expect[F64](h, Range[F64].till(0, p_inf, 1), Array[F64])
    _expect[F64](h, Range[F64].till(0, 10, p_inf), Array[F64])
    _expect[F64](h, Range[F64].till(n_inf, 10, 1), Array[F64])
    _expect[F64](h, Range[F64].till(0, n_inf, 1), Array[F64])
    _expect[F64](h, Range[F64].till(0, 10, n_inf), Array[F64])
    _expect[F64](h, Range[F64].through(nan, 10, 1), Array[F64])
    _expect[F64](h, Range[F64].through(0, nan, 1), Array[F64])
    _expect[F64](h, Range[F64].through(0, 10, nan), Array[F64])
    _expect[F64](h, Range[F64].through(p_inf, 10, 1), Array[F64])
    _expect[F64](h, Range[F64].through(0, p_inf, 1), Array[F64])
    _expect[F64](h, Range[F64].through(0, 10, p_inf), Array[F64])
    _expect[F64](h, Range[F64].through(n_inf, 10, 1), Array[F64])
    _expect[F64](h, Range[F64].through(0, n_inf, 1), Array[F64])
    _expect[F64](h, Range[F64].through(0, 10, n_inf), Array[F64])

    // empty range on non-finite F32
    let f32_nan = F32(0) / F32(0)
    let f32_inf = F32.max_value() + F32.max_value()
    _expect[F32](h, Range[F32].till(f32_nan, 10, 1), Array[F32])
    _expect[F32](h, Range[F32].till(0, f32_inf, 1), Array[F32])
    _expect[F32](h, Range[F32].through(f32_nan, 10, 1), Array[F32])
    _expect[F32](h, Range[F32].through(0, f32_inf, 1), Array[F32])

    // next() past end
    let r = Range.till(0, 2)
    r.next()?
    r.next()?
    try
      r.next()?
      h.fail("next() should error when range is exhausted")
    end

    // rewind
    let r2 = Range.till(0, 3)
    r2.next()?
    r2.next()?
    r2.rewind()
    h.assert_eq[USize](r2.next()?, 0)

    // rewind on through
    let r3 = Range.through(0, 2)
    while r3.has_next() do r3.next()? end
    r3.rewind()
    h.assert_eq[USize](r3.next()?, 0)

    // rewind after overflow-induced _done
    let r4 = Range[U8].through(253, 255)
    while r4.has_next() do r4.next()? end
    r4.rewind()
    _expect[U8](h, r4, [253; 254; 255])

    // rewind after descending unsigned overflow
    let r6 = Range[U8].through(2, 0)
    while r6.has_next() do r6.next()? end
    r6.rewind()
    _expect[U8](h, r6, [2; 1; 0])

    // through(n, n) at unsigned type boundary
    _expect[U8](h, Range[U8].through(255, 255), [255])
    _expect[U8](h, Range[U8].through(0, 0), [0])

    // through ascending with ISize
    _expect[ISize](h, Range[ISize].through(-3, 3), [-3; -2; -1; 0; 1; 2; 3])
    _expect[ISize](h, Range[ISize].through(3, -3), [3; 2; 1; 0; -1; -2; -3])

    // rewind does not resurrect an empty range from bad inputs
    let r5 = Range.till(0, 10, 0)
    h.assert_false(r5.has_next())
    r5.rewind()
    h.assert_false(r5.has_next())

  fun _expect[A: (Real[A] val & Number)](
    h: TestHelper,
    range: Range[A],
    expected: Array[A] val
  ) =>
    let actual = Array[A] .> concat(range)
    h.assert_array_eq[A](expected, actual)

class \nodoc\ iso _TestHeap is UnitTest
  fun name(): String => "collections/BinaryHeap"

  fun apply(t: TestHelper) ? =>
    _gen_test(t, Time.millis())?

  fun _gen_test(t: TestHelper, seed: U64) ? =>
    let rand = Rand(seed)
    let len = rand.int[USize](100)

    let ns = Array[USize](len)
    for _ in Range.till(0, len) do
      ns.push(rand.int[USize](100))
    end

    _test_push[MinHeapPriority[USize]](t, ns)?
    _test_push[MaxHeapPriority[USize]](t, ns)?
    _test_append[MinHeapPriority[USize]](t, ns)?
    _test_append[MaxHeapPriority[USize]](t, ns)?
    _test_pop[MinHeapPriority[USize]](t, ns)?
    _test_pop[MaxHeapPriority[USize]](t, ns)?

  fun _test_push[P: BinaryHeapPriority[USize]](
    t: TestHelper, ns: Array[USize]) ?
  =>
    let h = BinaryHeap[USize, P](ns.size())
    for n in ns.values() do
      h.push(n)
      _verify[P](t, h)?
    end
    t.assert_eq[USize](h.size(), ns.size())

  fun _test_append[P: BinaryHeapPriority[USize]](
    t: TestHelper, ns: Array[USize]) ?
  =>
    let h = BinaryHeap[USize, P](ns.size())
    h.append(ns)
    t.assert_eq[USize](h.size(), ns.size())
    _verify[P](t, h)?

  fun _test_pop[P: BinaryHeapPriority[USize]](
    t: TestHelper, ns: Array[USize]) ?
  =>
    let h = BinaryHeap[USize, P](ns.size())
    h.append(ns)

    if ns.size() == 0 then return end

    var prev = h.pop()?
    _verify[P](t, h)?

    for _ in Range.till(1, ns.size()) do
      let n = h.pop()?
      t.assert_true((prev == n) or P(prev, n))
      prev = n
      _verify[P](t, h)?
    end
    t.assert_eq[USize](h.size(), 0)

  fun _verify[P: BinaryHeapPriority[USize]]
    (t: TestHelper, h: BinaryHeap[USize, P], i: USize = 0) ?
  =>
    let a = (2 * i) + 1
    let b = a + 1

    if a < h.size() then
      t.assert_false(P(h._apply(a)?, h._apply(i)?))
      _verify[P](t, h, a)?
    end
    if b < h.size() then
      t.assert_false(P(h._apply(b)?, h._apply(i)?))
      _verify[P](t, h, b)?
    end

class \nodoc\ iso _TestFlags is UnitTest
  fun name(): String => "collections/Flags"

  fun apply(h: TestHelper) =>
    // t is for temporary.
    var t: _TestFlagsFlags = _TestFlagsFlags

    // As we want to test all binary combinations, we need four
    // flags to test the following combinations: 00 01 10 11
    //
    // Keeping these as let/val to ensure we don't accidentally
    // overwrite or mutate them.
    let set0011: _TestFlagsFlags =
      _TestFlagsFlags
        .> set(_TestFlagB)
        .> set(_TestFlagA)
    h.assert_eq[U8](set0011.value(), 0b0011)


    let set0101: _TestFlagsFlags =
      _TestFlagsFlags
        .> set(_TestFlagC)
        .> set(_TestFlagA)
    h.assert_eq[U8](set0101.value(), 0b0101)

    // Immutable Bitwise Operators: op_or, op_and, op_xor
    // 0b0011 or 0b0101
    h.assert_eq[U8](set0011.op_or(set0101).value(), 0b0111)

    // 0b0011 and 0b0101
    h.assert_eq[U8](set0011.op_and(set0101).value(), 0b0001)

    // 0b0011 xor 0b0101
    h.assert_eq[U8](set0011.op_xor(set0101).value(), 0b0110)

    // Immutable Bitwise Operators: add, sub
    // 0b0011 add 0b1000
    h.assert_eq[U8]((set0011 + _TestFlagD).value(), 0b1011)

    // 0b0011 sub 0b0001
    h.assert_eq[U8]((set0011 - _TestFlagA).value(), 0b0010)

    // Mutable Bitwise Operator: without
    // 0b0011 without 0b0101
    h.assert_eq[U8](set0011.without(set0101).value(), 0b0010)
    // 0b0101 without 0b0011
    h.assert_eq[U8](set0101.without(set0011).value(), 0b0100)

    // Mutable Operator: union
    t = set0011.clone()      // 0b0011
    t.union(set0101)         // 0b0101
    h.assert_eq[U8](t.value(), 0b0111)

    // a.union(b) == b.union(a)
    t = set0101.clone()      // 0b0101
    t.union(set0011)         // 0b0011
    h.assert_eq[U8](t.value(), 0b0111)

    // Mutable Operator: intersect
    t = set0011.clone()      // 0b0011
    t.intersect(set0101)     // 0b0101
    h.assert_eq[U8](t.value(), 0b0001)

    // a.intersect(b) == b.intersect(a)
    t = set0101.clone()      // 0b0101
    t.intersect(set0011)     // 0b0011
    h.assert_eq[U8](t.value(), 0b0001)

    // Mutable Operator: difference
    t = set0011.clone()              // 0b0011
    t.difference(set0101)            // 0b0101
    h.assert_eq[U8](t.value(), 0b0110)

    // a.difference(b) == b.difference(a) **symmetric difference**
    t = set0101.clone()      // 0b0101
    t.difference(set0011)    // 0b0011
    h.assert_eq[U8](t.value(), 0b0110)

    // Mutable Operator: remove
    t = set0011.clone()      // 0b0011
    t.remove(set0101)        // 0b0101
    h.assert_eq[U8](t.value(), 0b0010)

    // a.remove(b) != b.remove(a)
    t = set0101.clone()      // 0b0101
    t.remove(set0011)        // 0b0011
    h.assert_eq[U8](t.value(), 0b0100)

    // Mutable Operator: flip
    t = set0011.clone()
    h.assert_true(set0011 == t)                   // t = 0b0011
    t.flip(_TestFlagD)                            // flip 0b1000
                                                  // ------
    h.assert_true(set0011 != t)                   // t = 0b1011

                                                  // t = 0b1011
    t.flip(_TestFlagD)                            // flip 0b1000
                                                  // ------
    h.assert_true(set0011 == t)                   // t = 0b0011


    // Mutable Operator: all
    t.all()
    h.assert_true(t(_TestFlagA))
    h.assert_true(t(_TestFlagB))
    h.assert_true(t(_TestFlagC))
    h.assert_true(t(_TestFlagD))

    // Mutable Operator: clear
    t.clear()
    h.assert_false(t(_TestFlagA))
    h.assert_false(t(_TestFlagB))
    h.assert_false(t(_TestFlagC))
    h.assert_false(t(_TestFlagD))

    // Mutable Operator: set
    t .> set(_TestFlagA) .> set(_TestFlagD)
    h.assert_true(t(_TestFlagA))
    h.assert_false(t(_TestFlagB))
    h.assert_false(t(_TestFlagC))
    h.assert_true(t(_TestFlagD))

    // Check set for idempotency
    t .> set(_TestFlagA) .> set(_TestFlagD)
    h.assert_true(t(_TestFlagA))
    h.assert_false(t(_TestFlagB))
    h.assert_false(t(_TestFlagC))
    h.assert_true(t(_TestFlagD))

    // Mutable Operator: unset
    t .> unset(_TestFlagD)
    h.assert_true(t(_TestFlagA))
    h.assert_false(t(_TestFlagB))
    h.assert_false(t(_TestFlagC))
    h.assert_false(t(_TestFlagD))

    // Check unset for idempotency
    t .> unset(_TestFlagD)
    h.assert_true(t(_TestFlagA))
    h.assert_false(t(_TestFlagB))
    h.assert_false(t(_TestFlagC))
    h.assert_false(t(_TestFlagD))

    // Check instance creation default
    t = _TestFlagsFlags
    h.assert_false(t(_TestFlagA))
    h.assert_false(t(_TestFlagB))
    h.assert_false(t(_TestFlagC))
    h.assert_false(t(_TestFlagD))

    // Check instance creation with specified value
    t = _TestFlagsFlags(0b1001)
    h.assert_true(t(_TestFlagA))
    h.assert_false(t(_TestFlagB))
    h.assert_false(t(_TestFlagC))
    h.assert_true(t(_TestFlagD))

type _TestFlagsFlags is Flags[(_TestFlagA|_TestFlagB|_TestFlagC|_TestFlagD), U8]
primitive \nodoc\ _TestFlagA fun value(): U8 => 1
primitive \nodoc\ _TestFlagB fun value(): U8 => 2
primitive \nodoc\ _TestFlagC fun value(): U8 => 4
primitive \nodoc\ _TestFlagD fun value(): U8 => 8
