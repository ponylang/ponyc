use "pony_test"
use "pony_check"
use mut = "collections"
use "random"
use "time"

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    run_tests(test)

  fun tag run_tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    test(_TestListApply)
    test(_TestListConcat)
    test(_TestListDrop)
    test(_TestListDropWhile)
    test(_TestListEveryExists)
    test(_TestListFilter)
    test(_TestListFlatMap)
    test(_TestListFold)
    test(_TestListFrom)
    test(_TestListMap)
    test(_TestListPartition)
    test(_TestListPrepend)
    test(_TestListTake)
    test(_TestListTakeWhile)
    test(_TestListValues)
    test(_TestMap)
    test(_TestMapNoneValue)
    test(_TestMapVsMap)
    test(_TestSet)
    test(_TestVec)
    test(_TestVecContains)
    test(_TestVecFind)
    test(_TestVecIterators)
    test(_TestVecReverse)
    test(_TestVecSlice)
    test(Property1UnitTest[(Array[USize], USize)](_VecFindContainsProperty))
    test(Property1UnitTest[Array[USize]](_VecIteratorsProperty))
    test(Property1UnitTest[(Array[USize], USize, USize)](_VecLawsProperty))
    test(Property1UnitTest[(USize, Array[_VecAction])](_VecModelProperty))

class \nodoc\ iso _TestListPrepend is UnitTest
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

class \nodoc\ iso _TestListFrom is UnitTest
  fun name(): String => "collections/persistent/Lists (from)"

  fun apply(h: TestHelper) ? =>
    let l1 = Lists[U32].from([1; 2; 3].values())
    h.assert_eq[USize](l1.size(), 3)
    h.assert_eq[U32](l1.head()?, 1)

class \nodoc\ iso _TestListApply is UnitTest
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

class \nodoc\ iso _TestListValues is UnitTest
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

class \nodoc\ iso _TestListConcat is UnitTest
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

class \nodoc\ iso _TestListMap is UnitTest
  fun name(): String => "collections/persistent/Lists (map)"

  fun apply(h: TestHelper) ? =>
    let l5 = Lists[U32]([1; 2; 3]).map[U32]({(x) => x * 2 })
    h.assert_true(Lists[U32].eq(l5, Lists[U32]([2; 4; 6]))?)

class \nodoc\ iso _TestListFlatMap is UnitTest
  fun name(): String => "collections/persistent/Lists (flat_map)"

  fun apply(h: TestHelper) ? =>
    let f = {(x: U32): List[U32] => Lists[U32]([x - 1; x; x + 1]) }
    let l6 = Lists[U32]([2; 5; 8]).flat_map[U32](f)
    h.assert_true(Lists[U32].eq(l6, Lists[U32]([1; 2; 3; 4; 5; 6; 7; 8; 9]))?)

class \nodoc\ iso _TestListFilter is UnitTest
  fun name(): String => "collections/persistent/Lists (filter)"

  fun apply(h: TestHelper) ? =>
    let is_even = {(x: U32): Bool => (x % 2) == 0 }
    let l7 = Lists[U32]([1; 2; 3; 4; 5; 6; 7; 8]).filter(is_even)
    h.assert_true(Lists[U32].eq(l7, Lists[U32]([2; 4; 6; 8]))?)

class \nodoc\ iso _TestListFold is UnitTest
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

class \nodoc\ iso _TestListEveryExists is UnitTest
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

class \nodoc\ iso _TestListPartition is UnitTest
  fun name(): String => "collections/persistent/Lists (partition)"

  fun apply(h: TestHelper) ? =>
    let is_even = {(x: U32): Bool => (x % 2) == 0 }
    let l = Lists[U32]([1; 2; 3; 4; 5; 6])
    (let hits, let misses) = l.partition(is_even)
    h.assert_true(Lists[U32].eq(hits, Lists[U32]([2; 4; 6]))?)
    h.assert_true(Lists[U32].eq(misses, Lists[U32]([1; 3; 5]))?)

class \nodoc\ iso _TestListDrop is UnitTest
  fun name(): String => "collections/persistent/List (drop)"

  fun apply(h: TestHelper) ? =>
    let l = Lists[String](["a"; "b"; "c"; "d"; "e"])
    let l2 = Lists[U32]([1; 2])
    let empty = Lists[String].empty()
    h.assert_true(Lists[String].eq(l.drop(3), Lists[String](["d"; "e"]))?)
    h.assert_true(Lists[U32].eq(l2.drop(3), Lists[U32].empty())?)
    h.assert_true(Lists[String].eq(empty.drop(3), Lists[String].empty())?)

class \nodoc\ iso _TestListDropWhile is UnitTest
  fun name(): String => "collections/persistent/List (drop_while)"

  fun apply(h: TestHelper) ? =>
    let is_even = {(x: U32): Bool => (x % 2) == 0 }
    let l = Lists[U32]([4; 2; 6; 1; 3; 4; 6])
    let empty = Lists[U32].empty()
    h.assert_true(Lists[U32].eq(l.drop_while(is_even),
      Lists[U32]([1; 3; 4; 6]))?)
    h.assert_true(Lists[U32].eq(empty.drop_while(is_even), Lists[U32].empty())?)

class \nodoc\ iso _TestListTake is UnitTest
  fun name(): String => "collections/persistent/List (take)"

  fun apply(h: TestHelper) ? =>
    let l = Lists[String](["a"; "b"; "c"; "d"; "e"])
    let l2 = Lists[U32]([1; 2])
    let empty = Lists[String].empty()
    h.assert_true(Lists[String].eq(l.take(3), Lists[String](["a"; "b"; "c"]))?)
    h.assert_true(Lists[U32].eq(l2.take(3), Lists[U32]([1; 2]))?)
    h.assert_true(Lists[String].eq(empty.take(3), Lists[String].empty())?)

class \nodoc\ iso _TestListTakeWhile is UnitTest
  fun name(): String => "collections/persistent/List (take_while)"

  fun apply(h: TestHelper) ? =>
    let is_even = {(x: U32): Bool => (x % 2) == 0 }
    let l = Lists[U32]([4; 2; 6; 1; 3; 4; 6])
    let empty = Lists[U32].empty()
    h.assert_true(Lists[U32].eq(l.take_while(is_even), Lists[U32]([4; 2; 6]))?)
    h.assert_true(Lists[U32].eq(empty.take_while(is_even), Lists[U32].empty())?)

class \nodoc\ iso _TestMap is UnitTest
  fun name(): String =>
    "collections/persistent/Map (update, remove, concat, add, sub)"

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

    let m11 = m10 + ("b", 3)
    h.assert_eq[U32](m11("b")?, 3)
    let m12 = m11 - "b"
    h.assert_error({() ? => m12("b")? })

    let seed = Time.millis()
    h.log("seed: " + seed.string())
    let rand = Rand(seed)

    var map = Map[USize, None]
    for n in mut.Range(0, 100) do
      try
        map(USize.max_value())?
        h.fail("expected error for nonexistent key")
        return
      end
      map = map.update(rand.int[USize](USize.max_value() - 1), None)
    end

    gen_test(h, rand)?

  fun gen_test(h: TestHelper, rand: Rand) ? =>
    var a = _Map
    let b = mut.Map[U64, U64]

    let ops = gen_ops(300, rand)?
    for op in ops.values() do
      h.log(op.str())
      let prev = a
      a = op(a, b)?

      var n: USize = 0
      for (k, v) in a.pairs() do
        n = n + 1
        h.assert_eq[U64](b(k)?, v)
      end
      h.assert_eq[USize](n, a.size())
    end

  fun gen_ops(n: USize, rand: Rand): Array[_TestOp] ? =>
    let ops = Array[_TestOp](n)
    let keys = Array[U64](n)
    for v in mut.Range[U64](0, n.u64()) do
      let op_n = if keys.size() == 0 then 0 else rand.int[U64](4) end
      ops.push(
        match op_n
        | 0 | 1 =>
          // insert
          let k = rand.u64()
          keys.push(k)
          _OpMapUpdate(k, v)
        | 2 =>
          // update
          let k = keys(rand.int[USize](keys.size()))?
          _OpMapUpdate(k, v)
        | 3 =>
          // remove
          let k = keys.delete(rand.int[USize](keys.size()))?
          _OpMapRemove(k)
        else error
        end)
    end
    ops

type _Map is HashMap[U64, U64, _CollisionHash]

primitive \nodoc\ _CollisionHash is mut.HashFunction[U64]
  fun hash(x: U64): USize => x.usize() % 100
  fun eq(x: U64, y: U64): Bool => x == y

interface \nodoc\ val _TestOp
  fun apply(a: _Map, b: mut.Map[U64, U64]): _Map ?
  fun str(): String

class \nodoc\ val _OpMapUpdate
  let k: U64
  let v: U64

  new val create(k': U64, v': U64) =>
    k = k'
    v = v'

  fun apply(a: _Map, b: mut.Map[U64, U64]): _Map =>
    b.update(k, v)
    a(k) = v

  fun str(): String =>
    "".join(["Update("; k; "_"; _CollisionHash.hash(k); ", "; v; ")"].values())

class \nodoc\ val _OpMapRemove
  let k: U64

  new val create(k': U64) =>
    k = k'

  fun apply(a: _Map, b: mut.Map[U64, U64]): _Map ? =>
    b.remove(k)?
    a.remove(k)?

  fun str(): String =>
    "".join(["Remove("; k; ")"].values())

class \nodoc\ iso _TestMapNoneValue is UnitTest
  fun name(): String => "collections/persistent/Map (None values)"

  fun apply(h: TestHelper) ? =>
    // Verify that maps with value types including None work correctly.
    // This is the exact scenario from issue #4833.
    var m = Map[String, (String | None)]
    m = m("a") = "hello"
    m = m("b") = None
    m = m("c") = "world"

    // apply returns non-None values correctly
    h.assert_eq[String](m("a")? as String, "hello")
    h.assert_eq[String](m("c")? as String, "world")

    // apply returns the stored None for keys mapped to None
    h.assert_true(m("b")? is None)

    // apply raises for nonexistent keys
    h.assert_error({() ? => m("missing")? })

    // contains returns true for keys mapped to None
    h.assert_true(m.contains("b"))

    // contains returns true for keys mapped to non-None
    h.assert_true(m.contains("a"))

    // contains returns false for missing keys
    h.assert_false(m.contains("missing"))

    // get_or_else returns the stored None, not the alt value
    h.assert_true(m.get_or_else("b", "alt") is None)

    // get_or_else returns the alt for genuinely missing keys
    h.assert_eq[String](
      m.get_or_else("missing", "alt") as String, "alt")

    // remove works on keys mapped to None
    let m2 = m.remove("b")?
    h.assert_false(m2.contains("b"))
    h.assert_error({() ? => m2("b")? })
    h.assert_eq[USize](m2.size(), 2)

class \nodoc\ iso _TestMapVsMap is UnitTest
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

class \nodoc\ iso _TestSet is UnitTest
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

class \nodoc\ iso _TestVec is UnitTest
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

class \nodoc\ iso _TestVecIterators is UnitTest
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

class \nodoc\ iso _TestVecFind is UnitTest
  fun name(): String => "collections/persistent/Vec (find)"

  fun apply(h: TestHelper) ? =>
    // 1_100 elements fill 34 leaf nodes and leave 12 in the tail, so the
    // search crosses the boundary between the root trie and the tail.
    let n: USize = 1_100
    let v = Vec[USize].concat(mut.Range(0, n))

    // each value is found at its own index
    for i in mut.Range(0, n) do
      h.assert_eq[USize](v.find(i)?, i)
    end

    // the search starts at offset
    h.assert_eq[USize](v.find(5, 5)?, 5)
    h.assert_error({() ? => v.find(5, 6)? })
    h.assert_eq[USize](v.find(n - 1, n - 1)?, n - 1)

    // a value that is never matched raises an error
    h.assert_error({() ? => v.find(n)? })
    h.assert_error({() ? => v.find(0, n)? })
    h.assert_error({() ? => Vec[USize].find(0)? })

    // nth counts appearances from offset, starting at zero
    let period: USize = 4
    var repeats = Vec[USize]
    for i in mut.Range(0, n) do repeats = repeats.push(i % period) end
    for k in mut.Range(0, n / period) do
      h.assert_eq[USize](repeats.find(1, 0, k)?, 1 + (k * period))
    end
    h.assert_error({() ? => repeats.find(1, 0, n / period)? })

    // the predicate is applied as predicate(element, value)
    let successor = {(l: USize, r: USize): Bool => l == (r + 1) }
    h.assert_eq[USize](v.find(5, 0, 0, successor)?, 6)

class \nodoc\ iso _TestVecContains is UnitTest
  fun name(): String => "collections/persistent/Vec (contains)"

  fun apply(h: TestHelper) =>
    h.assert_false(Vec[USize].contains(0))

    // 1_100 elements fill 34 leaf nodes and leave 12 in the tail, so the
    // search crosses the boundary between the root trie and the tail.
    let n: USize = 1_100
    let v = Vec[USize].concat(mut.Range(0, n))

    for i in mut.Range(0, n) do
      h.assert_true(v.contains(i))
    end
    h.assert_false(v.contains(n))
    h.assert_false(v.contains(-1))

    // the predicate is applied as predicate(element, value)
    let successor = {(l: USize, r: USize): Bool => l == (r + 1) }
    h.assert_true(v.contains(0, successor))
    h.assert_false(v.contains(n - 1, successor))

class \nodoc\ iso _TestVecSlice is UnitTest
  fun name(): String => "collections/persistent/Vec (slice)"

  fun apply(h: TestHelper) ? =>
    // 1_100 elements fill 34 leaf nodes and leave 12 in the tail, so the tail
    // begins at index 1_088.
    let n: USize = 1_100
    let tail_offset = (n / 32) * 32
    let v = Vec[USize].concat(mut.Range(0, n))

    // the default range copies the whole vector
    let all = v.slice()
    h.assert_eq[USize](all.size(), n)
    for i in mut.Range(0, n) do
      h.assert_eq[USize](all(i)?, i)
    end

    // an interior range begins at from and stops before to
    let mid = v.slice(10, 20)
    h.assert_eq[USize](mid.size(), 10)
    for i in mut.Range(0, 10) do
      h.assert_eq[USize](mid(i)?, 10 + i)
    end

    // a range spanning the root/tail boundary
    let edge = v.slice(tail_offset - 2, tail_offset + 2)
    h.assert_eq[USize](edge.size(), 4)
    for i in mut.Range(0, 4) do
      h.assert_eq[USize](edge(i)?, (tail_offset - 2) + i)
    end

    // step skips elements
    let stepped = v.slice(0, 10, 3)
    h.assert_eq[USize](stepped.size(), 4)
    h.assert_eq[USize](stepped(0)?, 0)
    h.assert_eq[USize](stepped(1)?, 3)
    h.assert_eq[USize](stepped(2)?, 6)
    h.assert_eq[USize](stepped(3)?, 9)

    // to saturates at the size of the vector
    h.assert_eq[USize](v.slice(n - 5, n + 100).size(), 5)

    // ranges that select nothing give an empty vector
    h.assert_eq[USize](v.slice(5, 5).size(), 0)
    h.assert_eq[USize](v.slice(20, 10).size(), 0)
    h.assert_eq[USize](v.slice(n).size(), 0)
    h.assert_eq[USize](Vec[USize].slice().size(), 0)

class \nodoc\ iso _TestVecReverse is UnitTest
  fun name(): String => "collections/persistent/Vec (reverse)"

  fun apply(h: TestHelper) ? =>
    // 1_100 elements fill 34 leaf nodes and leave 12 in the tail, so the
    // reversed vector is read from both the root trie and the tail.
    let n: USize = 1_100
    let v = Vec[USize].concat(mut.Range(0, n))

    let r = v.reverse()
    h.assert_eq[USize](r.size(), n)
    for i in mut.Range(0, n) do
      h.assert_eq[USize](r(i)?, (n - 1) - i)
    end

    // reversing twice restores the original order
    let rr = r.reverse()
    h.assert_eq[USize](rr.size(), n)
    for i in mut.Range(0, n) do
      h.assert_eq[USize](rr(i)?, i)
    end

    // the vector reversed from is left alone
    for i in mut.Range(0, n) do
      h.assert_eq[USize](v(i)?, i)
    end

    let one = Vec[USize].push(7).reverse()
    h.assert_eq[USize](one.size(), 1)
    h.assert_eq[USize](one(0)?, 7)

    h.assert_eq[USize](Vec[USize].reverse().size(), 0)

type _VecAction is (U8, USize, USize)
  """
  One generated operation: an operation tag and two arguments whose meaning
  depends on the tag.
  """

primitive \nodoc\ _VecGen
  fun sizes(): Generator[USize] =>
    """
    Sizes biased toward the points where the trie changes shape: at 32 the tail
    is flushed into the root, and at 64 and 1_056 the root gains a level.
    """
    Generators.frequency[USize]([
      as WeightedGenerator[USize]:
      (5, Generators.usize(0, 70))
      (3, Generators.usize(0, 1_500))
      (2, Generators.one_of[USize]([
        as USize: 0; 1; 31; 32; 33; 63; 64; 65; 1_023; 1_055; 1_056; 1_057 ]))
    ])

  fun contents(): Generator[Array[USize]] =>
    """
    Elements are drawn from a small range so that repeats are common, which is
    what `find` and `contains` need to be interesting.
    """
    sizes().flat_map[Array[USize]](
      {(n: USize): Generator[Array[USize]] =>
        Generators.seq_of[USize, Array[USize]](Generators.usize(0, 99), n, n) })

  fun build(elements: ReadSeq[USize]): Vec[USize] =>
    var v = Vec[USize]
    for x in elements.values() do v = v.push(x) end
    v

primitive \nodoc\ _VecCheck
  fun contents(v: Vec[USize]): Array[USize] =>
    """
    Read a vector out through `apply`, which the example based tests cover, so
    that the iterators can be checked against it rather than against
    themselves.
    """
    let out = Array[USize](v.size())
    try
      for i in mut.Range(0, v.size()) do out.push(v(i)?) end
    end
    out

class \nodoc\ iso _VecModelProperty is Property1[(USize, Array[_VecAction])]
  """
  Apply a generated sequence of operations to a `Vec` and to an `Array` used as
  a model, then check that the two hold the same elements.

  Each sample enables a random subset of the operations. A sequence drawing
  from every operation random walks around a small size and never grows the
  trie past its first level; withholding `pop`, `delete` and `remove` from some
  samples is what drives the vector deep enough to add levels to the root.
  """
  fun name(): String => "collections/persistent/Vec (property: model)"

  fun gen(): Generator[(USize, Array[_VecAction])] =>
    Generators.usize(0, 127)
      .flat_map[(USize, Array[_VecAction])](
        {(bits: USize): Generator[(USize, Array[_VecAction])] =>
          // `push` is always enabled; with no way to add elements a sample
          // exercises nothing
          let config = bits or 1
          let ops = Array[U8]
          for op in mut.Range[U8](0, 7) do
            if (config and (USize(1) << op.usize())) != 0 then ops.push(op) end
          end
          Generators.seq_of[_VecAction, Array[_VecAction]](
            Generators.zip3[U8, USize, USize](
              Generators.one_of[U8](ops),
              Generators.usize(0, 1_000),
              Generators.usize(0, 1_000)),
            1,
            100)
            .map[(USize, Array[_VecAction])](
              {(actions: Array[_VecAction]): (USize, Array[_VecAction]) =>
                (config, actions) })
        })

  fun ref property(arg1: (USize, Array[_VecAction]), h: PropertyHelper) ? =>
    (let config, let actions) = arg1
    var v = Vec[USize]
    let model = Array[USize]

    for (op, a, b) in actions.values() do
      let n = model.size()
      match op
      | 0 =>
        v = v.push(a)
        model.push(a)
      | 1 =>
        if n > 0 then
          v = v.pop()?
          model.pop()?
        end
      | 2 =>
        if n > 0 then
          let i = a % n
          v = v.update(i, b)?
          model.update(i, b)?
        end
      | 3 =>
        if n > 0 then
          let i = a % n
          v = v.insert(i, b)?
          model.insert(i, b)?
        end
      | 4 =>
        if n > 0 then
          let i = a % n
          v = v.delete(i)?
          model.delete(i)?
        end
      | 5 =>
        if n > 0 then
          let i = a % n
          let count = 1 + (b % (n - i))
          v = v.remove(i, count)?
          model.remove(i, count)
        end
      | 6 =>
        // batches large enough that a sequence of them carries the vector past
        // 1_056 elements, where the root gains its second level
        let count = b % 300
        let added = Array[USize](count)
        for j in mut.Range(0, count) do added.push(a + j) end
        v = v.concat(added.values())
        model.append(added)
      end
    end

    h.assert_eq[USize](model.size(), v.size(), "size, config " + config.string())
    h.assert_array_eq[USize](
      model, _VecCheck.contents(v), "contents, config " + config.string())

class \nodoc\ iso _VecLawsProperty is Property1[(Array[USize], USize, USize)]
  """
  Laws that relate the operations to each other, each of which must hold for
  any vector.
  """
  fun name(): String => "collections/persistent/Vec (property: laws)"

  fun gen(): Generator[(Array[USize], USize, USize)] =>
    Generators.zip3[Array[USize], USize, USize](
      _VecGen.contents(), Generators.usize(0, 1_000), Generators.usize(0, 99))

  fun ref property(arg1: (Array[USize], USize, USize), h: PropertyHelper) ? =>
    (let elements, let idx, let value) = arg1
    let v = _VecGen.build(elements)
    let n = elements.size()

    // reverse maps index i to n - 1 - i
    let backwards = Array[USize](n)
    for i in mut.Range(0, n) do backwards.push(elements((n - 1) - i)?) end
    h.assert_array_eq[USize](
      backwards, _VecCheck.contents(v.reverse()), "reverse")

    // reversing twice is the identity
    h.assert_array_eq[USize](
      elements, _VecCheck.contents(v.reverse().reverse()), "reverse . reverse")

    // pushing then popping is the identity
    h.assert_array_eq[USize](
      elements, _VecCheck.contents(v.push(value).pop()?), "push . pop")

    // inserting then deleting at the same index is the identity
    if n > 0 then
      let i = idx % n
      h.assert_array_eq[USize](
        elements,
        _VecCheck.contents(v.insert(i, value)?.delete(i)?),
        "insert . delete")
    end

    // splitting anywhere and rejoining is the identity
    let k = idx % (n + 1)
    h.assert_array_eq[USize](
      elements,
      _VecCheck.contents(v.slice(0, k).concat(v.slice(k).values())),
      "slice . concat")

class \nodoc\ iso _VecIteratorsProperty is Property1[Array[USize]]
  """
  The three iterators agree with `apply`, with each other, and leave the vector
  they were created from alone.
  """
  fun name(): String => "collections/persistent/Vec (property: iterators)"

  fun gen(): Generator[Array[USize]] => _VecGen.contents()

  fun ref property(arg1: Array[USize], h: PropertyHelper) =>
    let n = arg1.size()
    let v = _VecGen.build(arg1)

    let indices = Array[USize](n)
    for i in mut.Range(0, n) do indices.push(i) end

    let seen_values = Array[USize](n)
    for x in v.values() do seen_values.push(x) end
    h.assert_array_eq[USize](arg1, seen_values, "values")

    let seen_keys = Array[USize](n)
    for i in v.keys() do seen_keys.push(i) end
    h.assert_array_eq[USize](indices, seen_keys, "keys")

    let paired_keys = Array[USize](n)
    let paired_values = Array[USize](n)
    for (i, x) in v.pairs() do
      paired_keys.push(i)
      paired_values.push(x)
    end
    h.assert_array_eq[USize](indices, paired_keys, "pairs indices")
    h.assert_array_eq[USize](arg1, paired_values, "pairs values")

    // the iterators consume a copy of the leaf nodes, not the vector
    h.assert_array_eq[USize](arg1, _VecCheck.contents(v), "source after iteration")

class \nodoc\ iso _VecFindContainsProperty is Property1[(Array[USize], USize)]
  """
  `find` and `contains` agree with a scan of the elements, including which
  appearance `nth` selects and where `offset` starts.
  """
  fun name(): String => "collections/persistent/Vec (property: find and contains)"

  fun gen(): Generator[(Array[USize], USize)] =>
    _VecGen.contents().flat_map[(Array[USize], USize)](
      {(elements: Array[USize]): Generator[(Array[USize], USize)] =>
        // search for a value that is present as often as one that is not
        let probes = Array[USize](elements.size() + 1)
        probes.append(elements)
        probes.push(1_000)
        Generators.one_of[USize](probes)
          .map[(Array[USize], USize)](
            {(probe: USize): (Array[USize], USize) =>
              // the capture is seen as `box` from inside the lambda
              (elements.clone(), probe) })
      })

  fun ref property(arg1: (Array[USize], USize), h: PropertyHelper) ? =>
    (let elements, let probe) = arg1
    let v = _VecGen.build(elements)

    let hits = Array[USize]
    for (i, x) in elements.pairs() do
      if x == probe then hits.push(i) end
    end

    h.assert_eq[Bool](hits.size() > 0, v.contains(probe), "contains")

    // nth selects the nth appearance
    var found = Array[USize](hits.size())
    for k in mut.Range(0, hits.size()) do
      found.push(try v.find(probe, 0, k)? else USize.max_value() end)
    end
    h.assert_array_eq[USize](hits, found, "find nth")

    // there is no appearance after the last one
    h.assert_error({() ? => v.find(probe, 0, hits.size())? }, "find past last")

    // offset starts the search
    if hits.size() > 0 then
      let last = hits(hits.size() - 1)?
      h.assert_eq[USize](last, v.find(probe, last)?, "find from offset")
      h.assert_error({() ? => v.find(probe, last + 1)? }, "find past offset")
    end

    // every element of the vector is found
    var missing = false
    for x in elements.values() do
      if not v.contains(x) then missing = true end
    end
    h.assert_false(missing, "contains every element")
