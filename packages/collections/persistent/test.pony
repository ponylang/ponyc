use "ponytest"
use mut = "collections"
use "random"

actor Main is TestList
  new create(env: Env) =>
    PonyTest(env, this)

  new make() => None

  fun tag tests(test: PonyTest) =>
    run_tests(test)

  fun tag run_tests(test: PonyTest) =>
    test(_TestPrepend)
    test(_TestFrom)
    test(_TestConcat)
    test(_TestMap)
    test(_TestFlatMap)
    test(_TestFilter)
    test(_TestFold)
    test(_TestEveryExists)
    test(_TestPartition)
    test(_TestDrop)
    test(_TestDropWhile)
    test(_TestTake)
    test(_TestTakeWhile)
    test(_TestBitOps)
    test(_TestHAMTMap)
    test(_TestMapVsMap)

class iso _TestPrepend is UnitTest
  fun name(): String => "collections/persistent/List/prepend()"

  fun apply(h: TestHelper) ? =>
    let a = Lists[U32].empty()
    let b = Lists[U32].cons(1, Lists[U32].empty())
    let c = Lists[U32].cons(2, b)
    let d = c.prepend(3)
    let e = a.prepend(10)

    h.assert_eq[U64](a.size(), 0)
    h.assert_eq[U64](b.size(), 1)
    h.assert_eq[U64](c.size(), 2)
    h.assert_eq[U64](d.size(), 3)
    h.assert_eq[U64](e.size(), 1)

    h.assert_eq[U32](b.head(), 1)
    h.assert_eq[U64](b.tail().size(), 0)
    h.assert_eq[U32](c.head(), 2)
    h.assert_eq[U64](c.tail().size(), 1)
    h.assert_eq[U32](d.head(), 3)
    h.assert_eq[U64](d.tail().size(), 2)
    h.assert_eq[U32](e.head(), 10)
    h.assert_eq[U64](e.tail().size(), 0)

    true

class iso _TestFrom is UnitTest
  fun name(): String => "collections/persistent/Lists/from()"

  fun apply(h: TestHelper) ? =>
    let l1 = Lists[U32]([1, 2, 3])
    h.assert_eq[U64](l1.size(), 3)
    h.assert_eq[U32](l1.head(), 1)

    true

class iso _TestConcat is UnitTest
  fun name(): String => "collections/persistent/List/concat()"

  fun apply(h: TestHelper) ? =>
    let l1 = Lists[U32]([1, 2, 3])
    let l2 = Lists[U32]([4, 5, 6])
    let l3 = l1.concat(l2)
    let l4 = l3.reverse()
    h.assert_eq[U64](l3.size(), 6)
    h.assert_true(Lists[U32].eq(l3, Lists[U32]([1,2,3,4,5,6])))
    h.assert_true(Lists[U32].eq(l4, Lists[U32]([6,5,4,3,2,1])))

    let l5 = Lists[U32].empty()
    let l6 = l5.reverse()
    let l7 = l6.concat(l1)
    h.assert_eq[U64](l6.size(), 0)
    h.assert_true(Lists[U32].eq(l7, Lists[U32]([1,2,3])))

    let l8 = Lists[U32]([1])
    let l9 = l8.reverse()
    h.assert_true(Lists[U32].eq(l9, Lists[U32]([1])))

    true

class iso _TestMap is UnitTest
  fun name(): String => "collections/persistent/Lists/map()"

  fun apply(h: TestHelper) ? =>
    let l5 = Lists[U32]([1, 2, 3]).map[U32](lambda(x: U32): U32 => x * 2 end)
    h.assert_true(Lists[U32].eq(l5, Lists[U32]([2,4,6])))

    true

class iso _TestFlatMap is UnitTest
  fun name(): String => "collections/persistent/Lists/flat_map()"

  fun apply(h: TestHelper) ? =>
    let f = lambda(x: U32): List[U32] => Lists[U32]([x - 1, x, x + 1]) end
    let l6 = Lists[U32]([2, 5, 8]).flat_map[U32](f)
    h.assert_true(Lists[U32].eq(l6, Lists[U32]([1,2,3,4,5,6,7,8,9])))

    true

class iso _TestFilter is UnitTest
  fun name(): String => "collections/persistent/Lists/filter()"

  fun apply(h: TestHelper) ? =>
    let is_even = lambda(x: U32): Bool => x % 2 == 0 end
    let l7 = Lists[U32]([1,2,3,4,5,6,7,8]).filter(is_even)
    h.assert_true(Lists[U32].eq(l7, Lists[U32]([2,4,6,8])))

    true

class iso _TestFold is UnitTest
  fun name(): String => "collections/persistent/Lists/fold()"

  fun apply(h: TestHelper) ? =>
    let add = lambda(acc: U32, x: U32): U32 => acc + x end
    let value = Lists[U32]([1,2,3]).fold[U32](add, 0)
    h.assert_eq[U32](value, 6)

    let doubleAndPrepend =
      lambda(acc: List[U32], x: U32): List[U32] => acc.prepend(x * 2) end
    let l8 =
      Lists[U32]([1,2,3]).fold[List[U32]](doubleAndPrepend, Lists[U32].empty())
    h.assert_true(Lists[U32].eq(l8, Lists[U32]([6,4,2])))

    true

class iso _TestEveryExists is UnitTest
  fun name(): String => "collections/persistent/Lists/every()exists()"

  fun apply(h: TestHelper) =>
    let is_even = lambda(x: U32): Bool => x % 2 == 0 end
    let l9 = Lists[U32]([4,2,10])
    let l10 = Lists[U32]([1,1,3])
    let l11 = Lists[U32]([1,1,2])
    let l12 = Lists[U32]([2,2,3])
    let l13 = Lists[U32].empty()
    h.assert_eq[Bool](l9.every(is_even), true)
    h.assert_eq[Bool](l10.every(is_even), false)
    h.assert_eq[Bool](l11.every(is_even), false)
    h.assert_eq[Bool](l12.every(is_even), false)
    h.assert_eq[Bool](l13.every(is_even), true)
    h.assert_eq[Bool](l9.exists(is_even), true)
    h.assert_eq[Bool](l10.exists(is_even), false)
    h.assert_eq[Bool](l11.exists(is_even), true)
    h.assert_eq[Bool](l12.exists(is_even), true)
    h.assert_eq[Bool](l13.exists(is_even), false)

    true

class iso _TestPartition is UnitTest
  fun name(): String => "collections/persistent/Lists/partition()"

  fun apply(h: TestHelper) ? =>
    let is_even = lambda(x: U32): Bool => x % 2 == 0 end
    let l = Lists[U32]([1,2,3,4,5,6])
    (let hits, let misses) = l.partition(is_even)
    h.assert_true(Lists[U32].eq(hits, Lists[U32]([2,4,6])))
    h.assert_true(Lists[U32].eq(misses, Lists[U32]([1,3,5])))

    true

class iso _TestDrop is UnitTest
  fun name(): String => "collections/persistent/List/drop()"

  fun apply(h: TestHelper) ? =>
    let l = Lists[String](["a","b","c","d","e"])
    let l2 = Lists[U32]([1,2])
    let empty = Lists[String].empty()
    h.assert_true(Lists[String].eq(l.drop(3), Lists[String](["d","e"])))
    h.assert_true(Lists[U32].eq(l2.drop(3), Lists[U32].empty()))
    h.assert_true(Lists[String].eq(empty.drop(3), Lists[String].empty()))
    true

class iso _TestDropWhile is UnitTest
  fun name(): String => "collections/persistent/List/drop_while()"

  fun apply(h: TestHelper) ? =>
    let is_even = lambda(x: U32): Bool => x % 2 == 0 end
    let l = Lists[U32]([4,2,6,1,3,4,6])
    let empty = Lists[U32].empty()
    h.assert_true(Lists[U32].eq(l.drop_while(is_even), Lists[U32]([1,3,4,6])))
    h.assert_true(Lists[U32].eq(empty.drop_while(is_even), Lists[U32].empty()))
    true

class iso _TestTake is UnitTest
  fun name(): String => "collections/persistent/List/take()"

  fun apply(h: TestHelper) ? =>
    let l = Lists[String](["a","b","c","d","e"])
    let l2 = Lists[U32]([1,2])
    let empty = Lists[String].empty()
    h.assert_true(Lists[String].eq(l.take(3), Lists[String](["a","b","c"])))
    h.assert_true(Lists[U32].eq(l2.take(3), Lists[U32]([1,2])))
    h.assert_true(Lists[String].eq(empty.take(3), Lists[String].empty()))
    true

class iso _TestTakeWhile is UnitTest
  fun name(): String => "collections/persistent/List/take_while()"

  fun apply(h: TestHelper) ? =>
    let is_even = lambda(x: U32): Bool => x % 2 == 0 end
    let l = Lists[U32]([4,2,6,1,3,4,6])
    let empty = Lists[U32].empty()
    h.assert_true(Lists[U32].eq(l.take_while(is_even), Lists[U32]([4,2,6])))
    h.assert_true(Lists[U32].eq(empty.take_while(is_even), Lists[U32].empty()))

    true

class iso _TestBitOps is UnitTest
  fun name(): String => "collections/persistent/_BitOps"

  fun apply(h: TestHelper) =>
    let a = _BitOps.mask_low(845)
    let b = _BitOps.mask_low(968)
    let c = _BitOps.mask_low(875)
    let d = _BitOps.mask_low(559)
    let e = _BitOps.mask_low(618)
    h.assert_eq[U32](a, 13)
    h.assert_eq[U32](b, 8)
    h.assert_eq[U32](c, 11)
    h.assert_eq[U32](d, 15)
    h.assert_eq[U32](e, 10)

    //1100 00011 11101 01001 10111 or 12711223
    let b0 = _BitOps.bitmap_idx_for(12711223, 0)
    let b1 = _BitOps.bitmap_idx_for(12711223, 1)
    let b2 = _BitOps.bitmap_idx_for(12711223, 2)
    let b3 = _BitOps.bitmap_idx_for(12711223, 3)
    let b4 = _BitOps.bitmap_idx_for(12711223, 4)
    h.assert_eq[U32](b0, 23)
    h.assert_eq[U32](b1, 9)
    h.assert_eq[U32](b2, 29)
    h.assert_eq[U32](b3, 3)
    h.assert_eq[U32](b4, 12)

    let c0 = _BitOps.check_idx_bit(13, 0)
    let c1 = _BitOps.check_idx_bit(13, 1)
    let c2 = _BitOps.check_idx_bit(13, 2)
    let c3 = _BitOps.check_idx_bit(13, 3)
    let c4 = _BitOps.check_idx_bit(13, 4)
    let c5 = _BitOps.check_idx_bit(26, 0)
    let c6 = _BitOps.check_idx_bit(26, 1)
    let c7 = _BitOps.check_idx_bit(26, 2)
    let c8 = _BitOps.check_idx_bit(26, 3)
    let c9 = _BitOps.check_idx_bit(26, 4)
    h.assert_eq[Bool](c0, true)
    h.assert_eq[Bool](c1, false)
    h.assert_eq[Bool](c2, true)
    h.assert_eq[Bool](c3, true)
    h.assert_eq[Bool](c4, false)
    h.assert_eq[Bool](c5, false)
    h.assert_eq[Bool](c6, true)
    h.assert_eq[Bool](c7, false)
    h.assert_eq[Bool](c8, true)
    h.assert_eq[Bool](c9, true)

    let d0 = _BitOps.flip_indexed_bit_on(8, 0)
    let d1 = _BitOps.flip_indexed_bit_on(8, 1)
    let d2 = _BitOps.flip_indexed_bit_on(8, 2)
    let d3 = _BitOps.flip_indexed_bit_on(8, 3)
    let d4 = _BitOps.flip_indexed_bit_on(8, 4)
    h.assert_eq[U32](d0, 9)
    h.assert_eq[U32](d1, 10)
    h.assert_eq[U32](d2, 12)
    h.assert_eq[U32](d3, 8)
    h.assert_eq[U32](d4, 24)

    //1100 00011 11101 01001 10111 or 12711223
    let f0 = _BitOps.array_idx_for(12711223, 0)
    let f1 = _BitOps.array_idx_for(12711223, 5)
    let f2 = _BitOps.array_idx_for(12711223, 10)
    let f3 = _BitOps.array_idx_for(12711223, 25)
    h.assert_eq[USize](f0, 0)
    h.assert_eq[USize](f1, 4)
    h.assert_eq[USize](f2, 6)
    h.assert_eq[USize](f3, 14)

    true

class iso _TestHAMTMap is UnitTest
  fun name(): String => "collections/persistent/Map"

  fun apply(h: TestHelper) =>
    let m1: Map[String,U32] = Maps.empty[String,U32]()
    h.assert_error(lambda()(m1)? => m1("a") end)
    let s1 = m1.size()
    h.assert_eq[U64](s1, 0)

    try
      let m2 = m1.update("a", 5)
      let m3 = m2.update("b", 10)
      let m4 = m3.update("a", 4)
      let m5 = m4.update("c", 0)
      h.assert_eq[U32](m2("a"), 5)
      h.assert_eq[U32](m3("b"), 10)
      h.assert_eq[U32](m4("a"), 4)
      h.assert_eq[U32](m5("c"), 0)
    else
      h.complete(false)
    end

    try
      let m6 = Maps.from[String,U32]([("a", 2), ("b", 3), ("d", 4), ("e", 5)])
      let m7 = m6.update("a", 10)
      h.assert_eq[U32](m6("a"), 2)
      h.assert_eq[U32](m6("b"), 3)
      h.assert_eq[U32](m6("d"), 4)
      h.assert_eq[U32](m6("e"), 5)
      h.assert_eq[U32](m7("a"), 10)
      h.assert_eq[U32](m7("b"), 3)
      h.assert_eq[U32](m7("a"), 10)
      let m8 = m7.remove("a")
      h.assert_error(lambda()(m8 = m8)? => m8("a") end)
      h.assert_eq[U32](m8("b"), 3)
      h.assert_eq[U32](m8("d"), 4)
      h.assert_eq[U32](m8("e"), 5)
      let m9 = m7.remove("e")
      h.assert_error(lambda()(m9 = m9)? => m9("e") end)
      h.assert_eq[U32](m9("b"), 3)
      h.assert_eq[U32](m9("d"), 4)
      let m10 = m9.remove("b").remove("d")
      h.assert_error(lambda()(m10 = m10)? => m10("b") end)
      h.assert_error(lambda()(m10 = m10)? => m10("d") end)
    else
      h.complete(false)
    end

    true

class iso _TestMapVsMap is UnitTest
  fun name(): String => "collections/persistent/Map vs. collections Map"

  fun apply(h: TestHelper) ? =>
    var p_map: Map[String,U64] = Maps.empty[String,U64]()
    let m_map: mut.Map[String,U64] = mut.Map[String,U64]()
    let kvs = Array[(String,U64)]()
    let dice = Dice(MT)
    var count: USize = 0
    let iterations: USize = 100000
    let keys: U64 = 10000

    while(count < iterations) do
      let k0 = dice(1,keys).string()
      let k: String val = consume k0
      let v = dice(1,100000)
      kvs.push((k, v))
      count = count + 1
    end

    count = 0
    while(count < iterations) do
      let k = kvs(count)._1
      let v = kvs(count)._2
      p_map = p_map.update(k, v)
      m_map.update(k, v)
      count = count + 1
    end
    count = 0
    while(count < iterations) do
      let pmv = p_map(kvs(count)._1)
      let mmv = m_map(kvs(count)._1)
      h.assert_eq[Bool](_H.equal_map_u64_values(pmv, mmv), true)
      count = count + 1
    end

    true

primitive _H
  fun equal_map_u64_values(a: (U64 | None), b: (U64 | None)): Bool =>
    match (a,b)
    | (None,None) => true
    | (None,_) =>
      false
    | (_,None) =>
      false
    | (let x: U64, let y: U64) =>
      x == y
    else
      false
    end
