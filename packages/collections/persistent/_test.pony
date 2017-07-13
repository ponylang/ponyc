use "ponytest"
use mut = "collections"
use "random"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    run_tests(test)

  fun tag run_tests(test: PonyTest) =>
    test(_TestListPrepend)
    test(_TestListFrom)
    test(_TestListApply)
    test(_TestListValues)
    test(_TestListConcat)
    test(_TestListMap)
    test(_TestListFlatMap)
    test(_TestListFilter)
    test(_TestListFold)
    test(_TestListEveryExists)
    test(_TestListPartition)
    test(_TestListDrop)
    test(_TestListDropWhile)
    test(_TestListTake)
    test(_TestListTakeWhile)
    test(_TestMap)
    test(_TestMapVsMap)
    test(_TestSet)
    test(_TestVec)
    test(_TestVecIterators)

class iso _TestListPrepend is UnitTest
  fun name(): String => "collections/persistent/List (prepend)"

  fun apply(h: TestHelper) ? =>
    let a = Lists[U32].empty()
    let b = Lists[U32].cons(1, Lists[U32].empty())
    let c = Lists[U32].cons(2, b)
    let d = c.prepend(3)
    let e = a.prepend(10)

    h.assert_eq[USize](a.size(), 0)
    h.assert_eq[USize](b.size(), 1)
    h.assert_eq[USize](c.size(), 2)
    h.assert_eq[USize](d.size(), 3)
    h.assert_eq[USize](e.size(), 1)

    h.assert_eq[U32](b.head()?, 1)
    h.assert_eq[USize](b.tail()?.size(), 0)
    h.assert_eq[U32](c.head()?, 2)
    h.assert_eq[USize](c.tail()?.size(), 1)
    h.assert_eq[U32](d.head(), 3)
    h.assert_eq[USize](d.tail().size(), 2)
    h.assert_eq[U32](e.head(), 10)
    h.assert_eq[USize](e.tail().size(), 0)

class iso _TestListFrom is UnitTest
  fun name(): String => "collections/persistent/Lists (from)"

  fun apply(h: TestHelper) ? =>
    let l1 = Lists[U32]([1; 2; 3])
    h.assert_eq[USize](l1.size(), 3)
    h.assert_eq[U32](l1.head()?, 1)

class iso _TestListApply is UnitTest
  fun name(): String => "collections/persistent/List (apply)"

  fun apply(h: TestHelper) ? =>
    let l1 = Lists[U32]([1; 2; 3])
    h.assert_eq[U32](l1(0)?, 1)
    h.assert_eq[U32](l1(1)?, 2)
    h.assert_eq[U32](l1(2)?, 3)
    h.assert_error({() ? => l1(3)? })
    h.assert_error({() ? => l1(4)? })

    let l2 = Lists[U32].empty()
    h.assert_error({() ? => l2(0)? })

class iso _TestListValues is UnitTest
  fun name(): String => "collections/persistent/List (values)"

  fun apply(h: TestHelper) ? =>
    let iter = Lists[U32]([1; 2; 3]).values()
    h.assert_true(iter.has_next())
    h.assert_eq[U32](iter.next()?, 1)
    h.assert_true(iter.has_next())
    h.assert_eq[U32](iter.next()?, 2)
    h.assert_true(iter.has_next())
    h.assert_eq[U32](iter.next()?, 3)
    h.assert_false(iter.has_next())
    h.assert_false(try iter.next()?; true else false end)
    h.assert_false(iter.has_next())
    h.assert_false(try iter.next()?; true else false end)

class iso _TestListConcat is UnitTest
  fun name(): String => "collections/persistent/List (concat)"

  fun apply(h: TestHelper) ? =>
    let l1 = Lists[U32]([1; 2; 3])
    let l2 = Lists[U32]([4; 5; 6])
    let l3 = l1.concat(l2)
    let l4 = l3.reverse()
    h.assert_eq[USize](l3.size(), 6)
    h.assert_true(Lists[U32].eq(l3, Lists[U32]([1; 2; 3; 4; 5; 6]))?)
    h.assert_true(Lists[U32].eq(l4, Lists[U32]([6; 5; 4; 3; 2; 1]))?)

    let l5 = Lists[U32].empty()
    let l6 = l5.reverse()
    let l7 = l6.concat(l1)
    h.assert_eq[USize](l6.size(), 0)
    h.assert_true(Lists[U32].eq(l7, Lists[U32]([1; 2; 3]))?)

    let l8 = Lists[U32]([1])
    let l9 = l8.reverse()
    h.assert_true(Lists[U32].eq(l9, Lists[U32]([1]))?)

class iso _TestListMap is UnitTest
  fun name(): String => "collections/persistent/Lists (map)"

  fun apply(h: TestHelper) ? =>
    let l5 = Lists[U32]([1; 2; 3]).map[U32]({(x: U32): U32 => x * 2 })
    h.assert_true(Lists[U32].eq(l5, Lists[U32]([2; 4; 6]))?)

class iso _TestListFlatMap is UnitTest
  fun name(): String => "collections/persistent/Lists (flat_map)"

  fun apply(h: TestHelper) ? =>
    let f = {(x: U32): List[U32] => Lists[U32]([x - 1; x; x + 1]) }
    let l6 = Lists[U32]([2; 5; 8]).flat_map[U32](f)
    h.assert_true(Lists[U32].eq(l6, Lists[U32]([1; 2; 3; 4; 5; 6; 7; 8; 9]))?)

class iso _TestListFilter is UnitTest
  fun name(): String => "collections/persistent/Lists (filter)"

  fun apply(h: TestHelper) ? =>
    let is_even = {(x: U32): Bool => (x % 2) == 0 }
    let l7 = Lists[U32]([1; 2; 3; 4; 5; 6; 7; 8]).filter(is_even)
    h.assert_true(Lists[U32].eq(l7, Lists[U32]([2; 4; 6; 8]))?)

class iso _TestListFold is UnitTest
  fun name(): String => "collections/persistent/Lists (fold)"

  fun apply(h: TestHelper) ? =>
    let add = {(acc: U32, x: U32): U32 => acc + x }
    let value = Lists[U32]([1; 2; 3]).fold[U32](add, 0)
    h.assert_eq[U32](value, 6)

    let doubleAndPrepend =
      {(acc: List[U32], x: U32): List[U32] => acc.prepend(x * 2) }
    let l8 =
      Lists[U32]([1; 2; 3]).fold[List[U32]](
        doubleAndPrepend, Lists[U32].empty())
    h.assert_true(Lists[U32].eq(l8, Lists[U32]([6; 4; 2]))?)

class iso _TestListEveryExists is UnitTest
  fun name(): String => "collections/persistent/Lists (every, exists)"

  fun apply(h: TestHelper) =>
    let is_even = {(x: U32): Bool => (x % 2) == 0 }
    let l9 = Lists[U32]([4; 2; 10])
    let l10 = Lists[U32]([1; 1; 3])
    let l11 = Lists[U32]([1; 1; 2])
    let l12 = Lists[U32]([2; 2; 3])
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

class iso _TestListPartition is UnitTest
  fun name(): String => "collections/persistent/Lists (partition)"

  fun apply(h: TestHelper) ? =>
    let is_even = {(x: U32): Bool => (x % 2) == 0 }
    let l = Lists[U32]([1; 2; 3; 4; 5; 6])
    (let hits, let misses) = l.partition(is_even)
    h.assert_true(Lists[U32].eq(hits, Lists[U32]([2; 4; 6]))?)
    h.assert_true(Lists[U32].eq(misses, Lists[U32]([1; 3; 5]))?)

class iso _TestListDrop is UnitTest
  fun name(): String => "collections/persistent/List (drop)"

  fun apply(h: TestHelper) ? =>
    let l = Lists[String](["a"; "b"; "c"; "d"; "e"])
    let l2 = Lists[U32]([1; 2])
    let empty = Lists[String].empty()
    h.assert_true(Lists[String].eq(l.drop(3), Lists[String](["d"; "e"]))?)
    h.assert_true(Lists[U32].eq(l2.drop(3), Lists[U32].empty())?)
    h.assert_true(Lists[String].eq(empty.drop(3), Lists[String].empty())?)

class iso _TestListDropWhile is UnitTest
  fun name(): String => "collections/persistent/List (drop_while)"

  fun apply(h: TestHelper) ? =>
    let is_even = {(x: U32): Bool => (x % 2) == 0 }
    let l = Lists[U32]([4; 2; 6; 1; 3; 4; 6])
    let empty = Lists[U32].empty()
    h.assert_true(Lists[U32].eq(l.drop_while(is_even),
      Lists[U32]([1; 3; 4; 6]))?)
    h.assert_true(Lists[U32].eq(empty.drop_while(is_even), Lists[U32].empty())?)

class iso _TestListTake is UnitTest
  fun name(): String => "collections/persistent/List (take)"

  fun apply(h: TestHelper) ? =>
    let l = Lists[String](["a"; "b"; "c"; "d"; "e"])
    let l2 = Lists[U32]([1; 2])
    let empty = Lists[String].empty()
    h.assert_true(Lists[String].eq(l.take(3), Lists[String](["a"; "b"; "c"]))?)
    h.assert_true(Lists[U32].eq(l2.take(3), Lists[U32]([1; 2]))?)
    h.assert_true(Lists[String].eq(empty.take(3), Lists[String].empty())?)

class iso _TestListTakeWhile is UnitTest
  fun name(): String => "collections/persistent/List (take_while)"

  fun apply(h: TestHelper) ? =>
    let is_even = {(x: U32): Bool => (x % 2) == 0 }
    let l = Lists[U32]([4; 2; 6; 1; 3; 4; 6])
    let empty = Lists[U32].empty()
    h.assert_true(Lists[U32].eq(l.take_while(is_even), Lists[U32]([4; 2; 6]))?)
    h.assert_true(Lists[U32].eq(empty.take_while(is_even), Lists[U32].empty())?)

class iso _TestMap is UnitTest
  fun name(): String => "collections/persistent/Map (update, remove, concat)"

  fun apply(h: TestHelper) ? =>
    let m1 = Map[String,U32]
    h.assert_error({() ? => m1("a")? })
    let s1 = m1.size()
    h.assert_eq[USize](s1, 0)

    let m2 = m1("a") = 5
    let m3 = m2("b") = 10
    let m4 = m3("a") = 4
    let m5 = m4("c") = 0
    h.assert_eq[U32](m2("a")?, 5)
    h.assert_eq[U32](m3("b")?, 10)
    h.assert_eq[U32](m4("a")?, 4)
    h.assert_eq[U32](m5("c")?, 0)

    let vs = [as (String, U32): ("a", 2); ("b", 3); ("d", 4); ("e", 5)]
    let m6 = Map[String,U32].concat(vs.values())
    let m7 = m6("a") = 10
    h.assert_eq[U32](m6("a")?, 2)
    h.assert_eq[U32](m6("b")?, 3)
    h.assert_eq[U32](m6("d")?, 4)
    h.assert_eq[U32](m6("e")?, 5)
    h.assert_eq[U32](m7("a")?, 10)
    h.assert_eq[U32](m7("b")?, 3)
    h.assert_eq[U32](m7("a")?, 10)
    let m8 = m7.remove("a")?
    h.assert_error({() ? => m8("a")? })
    h.assert_eq[U32](m8("b")?, 3)
    h.assert_eq[U32](m8("d")?, 4)
    h.assert_eq[U32](m8("e")?, 5)
    let m9 = m7.remove("e")?
    h.assert_error({() ? => m9("e")? })
    h.assert_eq[U32](m9("b")?, 3)
    h.assert_eq[U32](m9("d")?, 4)
    let m10 = m9.remove("b")?.remove("d")?
    h.assert_error({() ? => m10("b")? })
    h.assert_error({() ? => m10("d")? })

class iso _TestMapVsMap is UnitTest
  fun name(): String => "collections/persistent/Map (persistent vs mutable)"

  fun apply(h: TestHelper) ? =>
    let keys: USize = 300
    var p_map = Map[String, U64]
    let m_map = mut.Map[String, U64](keys)
    let r = MT

    var count: USize = 0
    while count < keys do
      let k: String = count.string()
      let v = r.next()
      p_map = p_map(k) = v
      m_map(k) = v
      count = count + 1
      h.assert_eq[USize](m_map.size(), p_map.size())
    end

    h.assert_eq[USize](m_map.size(), keys)
    h.assert_eq[USize](p_map.size(), keys)

    for (k, v) in m_map.pairs() do
      h.assert_eq[U64](p_map(k)?, v)
    end

    var c: USize = 0
    for (k, v) in p_map.pairs() do
      c = c + 1
      m_map.remove(k)?
    end
    h.assert_eq[USize](p_map.size(), c)
    h.assert_eq[USize](m_map.size(), 0)

class iso _TestSet is UnitTest
  fun name(): String => "collections/persistent/Set"

  fun apply(h: TestHelper) =>
    let a = Set[USize] + 1 + 2 + 3
    let b = Set[USize] + 2 + 3 + 4

    h.assert_false(a == b)
    h.assert_true(a != b)

    h.assert_false(a.contains(4))
    h.assert_true((a + 4).contains(4))

    h.assert_true(a.contains(3))
    h.assert_false((a - 3).contains(3))

    h.assert_true((a or b) == (Set[USize] + 1 + 2 + 3 + 4))
    h.assert_true((b or a) == (Set[USize] + 1 + 2 + 3 + 4))
    h.assert_true((a and b) == (Set[USize] + 2 + 3))
    h.assert_true((b and a) == (Set[USize] + 2 + 3))
    h.assert_true((a xor b) == (Set[USize] + 1 + 4))
    h.assert_true((b xor a) == (Set[USize] + 1 + 4))
    h.assert_true(a.without(b) == (Set[USize] + 1))
    h.assert_true(b.without(a) == (Set[USize] + 4))

class iso _TestVec is UnitTest
  fun name(): String => "collections/persistent/Vec"

  fun apply(h: TestHelper) ? =>
    var v = Vec[USize]
    let n: USize = 33_000 // resize up to 4 levels in depth

    // push
    for i in mut.Range(0, n) do
      v = v.push(i)
      h.assert_eq[USize](v(i)?, i)
    end

    // update
    for i in mut.Range(0, n) do
      v = v.update(i, -i)?
      h.assert_eq[USize](v(i)?, -i)
    end

    var idx: USize = 0
    for num in v.values() do
      h.assert_eq[USize](num, -idx)
      idx = idx + 1
    end
    h.assert_eq[USize](v.size(), idx)

    // pop
    for i in mut.Range(0, n) do
      v = v.pop()?
      h.assert_error({() ? => v(n - i)? })
      h.assert_eq[USize](v.size(), n - i - 1)
    end

    // concat
    v = Vec[USize].concat(mut.Range(0, n))
    for i in mut.Range(0, n) do
      h.assert_eq[USize](v(i)?, i)
    end

    // insert
    let insert_idx: USize = 14
    v = v.insert(insert_idx, 9999)?
    h.assert_eq[USize](v(insert_idx - 1)?, insert_idx - 1)
    h.assert_eq[USize](v(insert_idx)?, 9999)
    h.assert_eq[USize](v(insert_idx + 1)?, insert_idx)
    h.assert_eq[USize](v.size(), n + 1)
    h.assert_error({() ? => v.insert(v.size(), 0)? })
    h.assert_error({() ? => v.insert(-1, 0)? })

    // delete
    v = v.delete(insert_idx)?
    h.assert_eq[USize](v(insert_idx - 1)?, insert_idx - 1)
    h.assert_eq[USize](v(insert_idx)?, insert_idx)
    h.assert_eq[USize](v.size(), n)
    h.assert_error({() ? => v.delete(v.size())? })
    h.assert_error({() ? => v.delete(-1)? })

    // remove
    v = v.remove(0, 1)?
    h.assert_eq[USize](v(0)?, 1)
    h.assert_eq[USize](v(1)?, 2)
    h.assert_eq[USize](v.size(), n - 1)

    v = v.remove(10, 10)?
    h.assert_eq[USize](v(9)?, 10)
    h.assert_eq[USize](v(10)?, 21)
    h.assert_eq[USize](v(n - 12)?, n - 1)
    h.assert_eq[USize](v.size(), n - 11)

class iso _TestVecIterators is UnitTest
  fun name(): String => "collections/persistent/Vec (iterators)"

  fun apply(h: TestHelper) ? =>
    let n: USize = 33_000 // resize up to 4 levels in depth
    var vec = Vec[USize]
    for i in mut.Range(0, n) do vec = vec.push(i) end
    var c = vec.size()
    for (i, v) in vec.pairs() do
      c = c - 1
      h.assert_eq[USize](v, vec(i)?)
    end
    h.assert_eq[USize](c, 0)
