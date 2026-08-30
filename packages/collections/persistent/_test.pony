use "pony_test"
use "pony_check"
use mut = "collections"

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
    test(_TestMapDefaultHash)
    test(_TestMapEmptyIteratorContract)
    test(_TestMapNoneValue)
    test(_TestMapRemoveAbsentKey)
    test(_TestSet)
    test(_TestSetRemoveAbsent)
    test(_TestVec)
    test(_TestVecContains)
    test(_TestVecFind)
    test(_TestVecIterators)
    test(_TestVecRemoveOverrun)
    test(_TestVecReverse)
    test(_TestVecSlice)
    test(Property1UnitTest[Array[_MapAction]](_MapIteratorsProperty))
    test(Property1UnitTest[(USize, Array[_MapAction])](_MapModelProperty))
    test(Property1UnitTest[Array[_MapAction]](_MapStructureProperty))
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

    var map = Map[USize, None]
    for n in mut.Range(0, 100) do
      try
        map(USize.max_value())?
        h.fail("expected error for nonexistent key")
        return
      end
      map = map.update(n, None)
    end

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

class \nodoc\ iso _TestVecRemoveOverrun is UnitTest
  """
  `remove` pops `n` elements off the end before it shifts the survivors down.
  When fewer than `n` elements follow `i`, the pop takes elements the caller
  never named and the shift loop is empty, so they are not put back. `size`
  is reduced to match, which is why nothing downstream disagrees.

  The count is saturated rather than rejected, matching `Array.remove`, which
  this method is named after, and `slice`, which already documents a saturated
  range. The two methods must agree on what remove means; a mismatch with
  `Array.remove` on the same contents is a failure.
  """
  fun name(): String => "collections/persistent/Vec (remove overrun)"

  fun apply(h: TestHelper) ? =>
    // a size that stays inside the tail, so a failure is about the arithmetic
    // rather than about which trie level it landed on
    _check(h, 5, 4, 3)?  // one element follows i, three named
    _check(h, 5, 3, 3)?  // two follow, three named
    _check(h, 5, 4, 4)?
    _check(h, 5, 0, 99)? // every element named and then some
    _check(h, 5, 4, 1)?  // exactly reaches the end, no overrun
    _check(h, 5, 1, 2)?  // wholly inside, the case that already worked
    _check(h, 5, 2, 0)?  // removes nothing

    // and across a level boundary, where the pop walks the trie rather than
    // the tail
    _check(h, 33, 32, 8)?
    _check(h, 1056, 1050, 100)?

    // `i` itself out of bounds still raises: saturating the count does not
    // make an out of range start acceptable
    let v = _VecGen.build([as USize: 0; 1; 2])
    h.assert_error({() ? => v.remove(3, 1)? }, "i == size")
    h.assert_error({() ? => v.remove(9, 1)? }, "i past size")
    h.assert_error({() ? => Vec[USize].remove(0, 1)? }, "empty vec")

  fun _check(h: TestHelper, size: USize, i: USize, n: USize) ? =>
    let elements = Array[USize](size)
    for x in mut.Range(0, size) do elements.push(x) end

    let expected = Array[USize](size) .> append(elements)
    expected.remove(i, n)

    let got = _VecCheck.contents(_VecGen.build(elements).remove(i, n)?)

    let at: String val =
      " (size " + size.string() + ", remove(" + i.string() + ", " +
        n.string() + "))"
    h.assert_eq[USize](expected.size(), got.size(), "size" + at)
    h.assert_array_eq[USize](expected, got, "contents" + at)

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
          // drawn from the whole size, so a count that runs past the end is
          // generated as readily as one that fits. `Array.remove` saturates,
          // so the model already carries the answer `Vec.remove` must give.
          let count = 1 + (b % n)
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

type _TrieMap is HashMap[U64, U64, _TrieHash]

primitive \nodoc\ _TrieHash is mut.HashFunction[U64]
  """
  The low 32 bits of a key are its hash, so a test states where a key sits in
  the trie rather than assuming it. Bits 0 to 29 are the six 5-bit level
  indices, bits 30 and 31 select the collision bin, and bits 32 and up
  separate keys that share a hash.
  """
  fun hash(x: U64): USize => (x and 0xFFFF_FFFF).usize()
  fun eq(x: U64, y: U64): Bool => x == y

primitive \nodoc\ _TrieKey
  fun apply(levels: Array[U32] box, bin: U32 = 0, dup: U64 = 0): U64 =>
    """
    A key sitting at `levels`, one 5-bit index per depth counting from 0, in
    collision bin `bin`. Keys that differ only in `dup` have the same hash.
    """
    var h: U64 = 0
    for (d, i) in levels.pairs() do
      h = h or ((i and 0b11111).u64() << (d * 5).u64())
    end
    (h or ((bin and 0b11).u64() << 30)) or (dup << 32)

  fun build(keys: Array[U64] box): _TrieMap =>
    var m = _TrieMap
    for k in keys.values() do m = m.update(k, k) end
    m

class \nodoc\ iso _TestMapRemoveAbsentKey is UnitTest
  """
  Reaching a slot that holds a single entry means only that the key's hash
  indexes there. The entry may belong to a different key, so `remove` must
  compare before it deletes.

  The walk can stop on an entry at any depth, so both sides of the comparison
  are checked at every depth rather than only at the root.
  """
  fun name(): String => "collections/persistent/Map (remove absent key)"

  fun apply(h: TestHelper) ? =>
    for d in mut.Range[USize](0, 6) do
      let at: String val = " at depth " + d.string()

      // two keys sharing every level above `d` and differing at `d`, so the
      // levels above hold subnodes and `d` holds two bare entries
      let a_levels = _levels(d, 3)
      let b_levels = _levels(d, 7)
      let a = _TrieKey(a_levels)
      let b = _TrieKey(b_levels)
      let m = _TrieKey.build([a; b])
      h.assert_eq[USize](m.size(), 2, "built" + at)
      h.assert_eq[U64](m(a)?, a, "a present" + at)
      h.assert_eq[U64](m(b)?, b, "b present" + at)

      // an absent key whose walk stops on a's slot: below depth 5 it differs
      // one level further down, at depth 5 it has a's hash and a different key
      let absent =
        if d < 5 then
          let c_levels = _levels(d, 3)
          c_levels.push(9)
          _TrieKey(c_levels)
        else
          _TrieKey(a_levels, 0, 1)
        end
      h.assert_error({() ? => m.remove(absent)? }, "absent raises" + at)
      h.assert_eq[USize](m.size(), 2, "size after absent" + at)
      h.assert_eq[U64](m(a)?, a, "a survives absent" + at)
      h.assert_eq[U64](m(b)?, b, "b survives absent" + at)

      // the other side of the same comparison: a key that is there still goes
      let m2 = m.remove(a)?
      h.assert_eq[USize](m2.size(), 1, "size after present" + at)
      h.assert_false(m2.contains(a), "a gone" + at)
      h.assert_eq[U64](m2(b)?, b, "b survives present" + at)

      // sub returns a map instead of raising, so a caller sees no difference
      // between a no-op and a deletion
      let m3 = m - absent
      h.assert_eq[USize](m3.size(), 2, "sub size" + at)
      h.assert_eq[U64](m3(a)?, a, "sub keeps a" + at)
      h.assert_eq[U64](m3(b)?, b, "sub keeps b" + at)
    end

    // an absent key indexing to an empty slot must raise too, so the checks
    // above cannot be passing for the trivial reason
    let lone = _TrieKey([5])
    let m4 = _TrieKey.build([lone])
    h.assert_error({() ? => m4.remove(_TrieKey([6]))? }, "empty slot")
    h.assert_eq[USize](m4.size(), 1, "empty slot size")

    // removing from an empty map must raise rather than wrap `size` round
    let empty = _TrieMap
    h.assert_error({() ? => empty.remove(lone)? }, "empty map")
    h.assert_eq[USize](empty.size(), 0, "empty map size")
    h.assert_eq[USize]((empty - lone).size(), 0, "empty map sub size")

    // the collision layer compared keys already; this pins that it still does
    let c_a = _TrieKey([9], 0, 0)
    let c_b = _TrieKey([9], 0, 1)
    let m5 = _TrieKey.build([c_a; c_b])
    h.assert_error({() ? => m5.remove(_TrieKey([9], 0, 2))? }, "collision")
    h.assert_eq[USize](m5.size(), 2, "collision size")
    h.assert_eq[U64](m5(c_a)?, c_a, "collision keeps a")
    h.assert_eq[U64](m5(c_b)?, c_b, "collision keeps b")

    // Set removes through the same path, and `-` swallows the error, so the
    // set a caller gets back is the only evidence. Stated over a hash that
    // puts the absent element on a resident element's slot, which the default
    // hash would only do by chance.
    let s_a = _TrieKey([4])
    let s_b = _TrieKey([4; 6])
    let s = (HashSet[U64, _TrieHash] + s_a) + s_b
    h.assert_eq[USize](s.size(), 2, "set built")
    let s2 = s - _TrieKey([4; 2])
    h.assert_eq[USize](s2.size(), 2, "set size")
    h.assert_true(s2.contains(s_a), "set keeps a")
    h.assert_true(s2.contains(s_b), "set keeps b")

  fun _levels(depth: USize, last: U32): Array[U32] =>
    """
    Level indices that are 1 above `depth` and `last` at it, so two keys built
    with different `last` values split exactly at `depth`.
    """
    let levels = Array[U32](depth + 1)
    for _ in mut.Range[USize](0, depth) do levels.push(1) end
    levels.push(last)
    levels

class \nodoc\ iso _TestMapEmptyIteratorContract is UnitTest
  """
  `Iterator` requires `has_next` to be false once there is nothing left. A
  `for` loop cannot see a violation, because the sugar turns the error from
  `next` into a `break`, so this drives the protocol directly.
  """
  fun name(): String => "collections/persistent/Map (empty iterators)"

  fun apply(h: TestHelper) ? =>
    let fresh = Map[String, U32]
    h.assert_false(fresh.pairs().has_next(), "fresh pairs")
    h.assert_false(fresh.keys().has_next(), "fresh keys")
    h.assert_false(fresh.values().has_next(), "fresh values")
    h.assert_error({() ? => Map[String, U32].pairs().next()? }, "fresh next")

    // a map emptied by removal, which is the state a running program reaches
    let emptied = (Map[String, U32]("a") = 1).remove("a")?
    h.assert_eq[USize](emptied.size(), 0, "emptied size")
    h.assert_false(emptied.pairs().has_next(), "emptied pairs")
    h.assert_false(emptied.keys().has_next(), "emptied keys")
    h.assert_false(emptied.values().has_next(), "emptied values")

    h.assert_false(Set[String].values().has_next(), "set values")

    // one entry: has_next must be true exactly until the entry is taken
    let one = Map[String, U32]("a") = 1
    let p = one.pairs()
    h.assert_true(p.has_next(), "one before")
    h.assert_eq[String](p.next()?._1, "a", "one value")
    h.assert_false(p.has_next(), "one after")

class \nodoc\ val _MapAction is Stringable
  """
  One generated operation: an operation tag, a key seed and a value.

  A class rather than a tuple so that a failing sample prints the operations
  that produced it. An array of tuples is not `ReadSeq[Stringable]`, so
  PonyCheck reports it as a digest, which for a map says nothing about which
  keys were involved.
  """
  let op: U8
  let seed: U64
  let value: U64

  new val create(op': U8, seed': U64, value': U64) =>
    op = op'
    seed = seed'
    value = value'

  fun string(): String iso^ =>
    "op" + op.string() + "/k" + _MapGen.key(seed).string() +
      "=" + value.string()

primitive \nodoc\ _MapGen
  fun key(seed: U64): U64 =>
    """
    A key that is zero at every level except one, so that two keys diverging
    at the same level branch there and keys diverging at different levels
    share the prefix above them. Drawing each level independently instead
    leaves depths three and below with a single child throughout, and the
    splits and compactions there are never reached.

    Keys differing only in the salt have one hash, which is what carries a
    pair down to the collision layer.
    """
    let depth = (seed and 0b111) % 6
    let index = (seed >> 3) and 0b11111
    let bin = (seed >> 8) and 0b11
    let salt = (seed >> 10) and 0b11
    ((index << (depth * 5)) or (bin << 30)) or (salt << 32)

  fun seeds(): Generator[U64] => Generators.u64(0, 16_383)

  fun actions(op_gen: Generator[U8]): Generator[_MapAction] =>
    Generators.zip3[U8, U64, U64](op_gen, seeds(), Generators.u64(0, 1_000))
      .map[_MapAction](
        {(t: (U8, U64, U64)): _MapAction =>
          _MapAction(t._1, t._2, t._3)
        })

  fun nth_key(model: mut.Map[U64, U64] box, n: USize): U64 ? =>
    """
    A key the model holds, so that an operation on a present key does not have
    to guess one.
    """
    var i: USize = 0
    for k in model.keys() do
      if i == n then return k end
      i = i + 1
    end
    error

primitive \nodoc\ _MapCheck
  fun drain(m: _TrieMap): (Array[(U64, U64)], Bool) =>
    """
    Drain an iterator through `has_next` and `next` rather than a `for` loop.
    The `for` sugar turns an error from `next` into a `break`, so a `for` loop
    cannot tell an exhausted iterator from one that raised.

    The returned flag is true when `next` raised while `has_next` was still
    true, or when `has_next` stayed true past the number of pairs the map
    holds. Both are the same contract violation, and swallowing either would
    leave this unable to fail.
    """
    let out = Array[(U64, U64)](m.size())
    let it = m.pairs()
    let limit = m.size() + 2
    while it.has_next() do
      if out.size() >= limit then return (out, true) end
      try out.push(it.next()?) else return (out, true) end
    end
    (out, false)

class \nodoc\ iso _MapModelProperty is Property1[(USize, Array[_MapAction])]
  """
  Apply a generated sequence of operations to a persistent map and to a
  mutable map used as a model, then check that the two hold the same pairs.

  Each sample enables a random subset of the operations. A sequence drawing
  from every operation random walks around a small size; withholding the
  removals is what drives the trie deep enough to reach the collision layer.
  """
  fun name(): String => "collections/persistent/Map (property: model)"

  fun gen(): Generator[(USize, Array[_MapAction])] =>
    Generators.usize(0, 255)
      .flat_map[(USize, Array[_MapAction])](
        {(bits: USize): Generator[(USize, Array[_MapAction])] =>
          // insert is always enabled; with no way to add pairs a sample
          // exercises nothing
          let config = bits or 1
          let ops = Array[U8]
          for op in mut.Range[U8](0, 8) do
            if (config and (USize(1) << op.usize())) != 0 then ops.push(op) end
          end
          Generators.seq_of[_MapAction, Array[_MapAction]](
            _MapGen.actions(Generators.one_of[U8](ops)), 1, 60)
            .map[(USize, Array[_MapAction])](
              {(actions: Array[_MapAction]): (USize, Array[_MapAction]) =>
                (config, actions)
              })
        })

  fun ref property(arg1: (USize, Array[_MapAction]), h: PropertyHelper) ? =>
    (let config, let actions) = arg1
    let msg: String val = ", config " + config.string()
    var m = _TrieMap
    let model = mut.Map[U64, U64]

    for action in actions.values() do
      let op = action.op
      let v = action.value
      let k = _MapGen.key(action.seed)
      let n = model.size()
      match op
      | 0 =>
        m = m.update(k, v)
        model(k) = v
      | 1 =>
        if n > 0 then
          let k' = _MapGen.nth_key(model, v.usize() % n)?
          m = m.update(k', v)
          model(k') = v
        end
      | 2 =>
        if n > 0 then
          let k' = _MapGen.nth_key(model, v.usize() % n)?
          m = m.remove(k')?
          model.remove(k')?
        end
      | 3 =>
        // an absent key that walks to a resident key's slot: the same hash
        // bits below 32, a different key. Built by flipping a bit above the
        // hash, so it reaches that key's slot at whatever depth it sits. A
        // uniformly drawn key lands on an occupied slot only by chance.
        if n > 0 then
          let resident = _MapGen.nth_key(model, v.usize() % n)?
          let absent = resident xor (U64(1) << 32)
          if not model.contains(absent) then
            let before = m
            h.assert_error(
              {() ? => before.remove(absent)? },
              "remove absent" + msg)
            h.assert_eq[USize](m.size(), n, "remove absent size" + msg)
            h.assert_eq[U64](
              m(resident)?, model(resident)?, "remove absent victim" + msg)
          end
        end
      | 4 =>
        if n > 0 then
          let k' = _MapGen.nth_key(model, v.usize() % n)?
          m = m - k'
          model.remove(k')?
        end
      | 5 =>
        if n > 0 then
          let resident = _MapGen.nth_key(model, v.usize() % n)?
          let absent = resident xor (U64(1) << 32)
          if not model.contains(absent) then
            m = m - absent
            h.assert_eq[USize](m.size(), n, "sub absent size" + msg)
            h.assert_eq[U64](
              m(resident)?, model(resident)?, "sub absent victim" + msg)
          end
        end
      | 6 =>
        let batch = Array[(U64, U64)]
        for j in mut.Range[U64](0, 1 + (v % 8)) do
          let k' = _MapGen.key(action.seed + j)
          batch.push((k', v + j))
          model(k') = v + j
        end
        m = m.concat(batch.values())
      | 7 =>
        // the three lookups are three views of one operation and must agree
        if model.contains(k) then
          let want = model(k)?
          h.assert_eq[U64](m(k)?, want, "apply" + msg)
          h.assert_true(m.contains(k), "contains" + msg)
          h.assert_eq[U64](
            m.get_or_else(k, U64.max_value()), want, "get_or_else" + msg)
        else
          h.assert_false(m.contains(k), "contains absent" + msg)
          h.assert_eq[U64](
            m.get_or_else(k, U64.max_value()),
            U64.max_value(),
            "get_or_else absent" + msg)
        end
      end
    end

    // both directions: the map holds everything the model does, and nothing
    // the model does not. Checking one direction and a count passes even when
    // an entry has been silently dropped.
    h.assert_eq[USize](model.size(), m.size(), "size" + msg)
    for (k, v) in model.pairs() do
      h.assert_eq[U64](m(k)?, v, "model to map" + msg)
    end
    (let drained, let broke) = _MapCheck.drain(m)
    h.assert_false(broke, "iterator contract" + msg)
    h.assert_eq[USize](drained.size(), m.size(), "pairs count" + msg)
    let seen = mut.Set[U64]
    for (k, v) in drained.values() do
      h.assert_eq[U64](model(k)?, v, "map to model" + msg)
      h.assert_false(seen.contains(k), "duplicate key" + msg)
      seen.set(k)
    end

class \nodoc\ iso _MapIteratorsProperty is Property1[Array[_MapAction]]
  """
  The three iterators agree with `apply`, agree with each other, and report
  exhaustion honestly. Driven through `has_next` and `next` directly, because
  the `for` sugar hides an iterator that raises instead of ending.
  """
  fun name(): String => "collections/persistent/Map (property: iterators)"

  fun gen(): Generator[Array[_MapAction]] =>
    Generators.seq_of[_MapAction, Array[_MapAction]](
      _MapGen.actions(Generators.one_of[U8]([as U8: 0; 1])), 0, 60)

  fun ref property(arg1: Array[_MapAction], h: PropertyHelper) ? =>
    var m = _TrieMap
    let model = mut.Map[U64, U64]

    for action in arg1.values() do
      let op = action.op
      let v = action.value
      let k = _MapGen.key(action.seed)
      match op
      | 0 =>
        m = m.update(k, v)
        model(k) = v
      | 1 =>
        if model.size() > 0 then
          let k' = _MapGen.nth_key(model, v.usize() % model.size())?
          m = m.remove(k')?
          model.remove(k')?
        end
      end
    end

    (let drained, let broke) = _MapCheck.drain(m)
    h.assert_false(broke, "pairs contract")
    h.assert_eq[USize](m.size(), drained.size(), "pairs count")

    // keys() and values() are separate classes with their own next()
    let ks = Array[U64](m.size())
    let kit = m.keys()
    while kit.has_next() do
      if ks.size() >= (m.size() + 2) then break end
      try ks.push(kit.next()?) else h.fail("keys contract"); break end
    end
    h.assert_eq[USize](m.size(), ks.size(), "keys count")

    let vs = Array[U64](m.size())
    let vit = m.values()
    while vit.has_next() do
      if vs.size() >= (m.size() + 2) then break end
      try vs.push(vit.next()?) else h.fail("values contract"); break end
    end
    h.assert_eq[USize](m.size(), vs.size(), "values count")

    let pair_keys = Array[U64](drained.size())
    let pair_values = Array[U64](drained.size())
    for (k, v) in drained.values() do
      pair_keys.push(k)
      pair_values.push(v)
    end
    h.assert_array_eq_unordered[U64](pair_keys, ks, "keys agree with pairs")
    h.assert_array_eq_unordered[U64](pair_values, vs, "values agree with pairs")

    // the keys the iterator yields are compared against the model rather than
    // against another iterator, so the check is not circular
    let model_keys = Array[U64](model.size())
    for k in model.keys() do model_keys.push(k) end
    h.assert_array_eq_unordered[U64](
      model_keys, pair_keys, "keys agree with the model")

    // the map is unchanged by having been iterated
    h.assert_eq[USize](model.size(), m.size(), "size after iterating")
    for (k, v) in model.pairs() do
      h.assert_eq[U64](m(k)?, v, "value after iterating")
    end

class \nodoc\ iso _TestMapDefaultHash is UnitTest
  """
  Uses the default `String` hash over ordinary keys, which is the distribution
  a caller gets, and checks the map against a mutable one built from the same
  pairs.
  """
  fun name(): String => "collections/persistent/Map (default hash)"

  fun apply(h: TestHelper) ? =>
    let count: USize = 300
    var p = Map[String, U64]
    let model = mut.Map[String, U64]

    for i in mut.Range(0, count) do
      let k: String val = "key" + i.string()
      let v = (i * 7919).u64()
      p = p(k) = v
      model(k) = v
      h.assert_eq[USize](model.size(), p.size(), "size while building")
    end

    for (k, v) in model.pairs() do
      h.assert_eq[U64](p(k)?, v, "model to map")
      h.assert_true(p.contains(k), "contains")
    end

    var seen: USize = 0
    let it = p.pairs()
    while it.has_next() do
      if seen > count then
        h.fail("pairs did not report exhaustion")
        break
      end
      (let k, let v) = it.next()?
      seen = seen + 1
      h.assert_eq[U64](model(k)?, v, "map to model")
    end
    h.assert_eq[USize](count, seen, "pairs count")

    // whatever the default hash does with these, none of them is in the map,
    // so none of them may change it
    for i in mut.Range(0, count) do
      let absent: String val = "absent" + i.string()
      let before = p
      h.assert_error({() ? => before.remove(absent)? }, "absent " + i.string())
    end
    h.assert_eq[USize](count, p.size(), "size after absent removes")
    for (k, v) in model.pairs() do
      h.assert_eq[U64](p(k)?, v, "value after absent removes")
    end

    for k in model.keys() do
      p = p.remove(k)?
    end
    h.assert_eq[USize](0, p.size(), "drained size")
    h.assert_false(p.pairs().has_next(), "drained iterator")

primitive \nodoc\ _MapShape
  fun check(root: _MapSubNodes[U64, U64, _TrieHash]): (USize, String) =>
    """
    Walk the trie and check the shape the node code maintains. Returns the
    number of entries found, and the first invariant broken or an empty
    string when the shape is sound.

    Counting the entries here is independent of both `size`, which is a
    field, and `pairs`, which walks the same nodes an operation just rebuilt,
    so it disagrees with them when either is wrong.

    The walk reports rather than asserts, for two reasons. A failure carries
    the depth and slot it happened at, which an assertion per node cannot
    because every node reaches the same handful of checks from the same
    lines. And the caller asserts once per operation, so a broken shape names
    the operation that broke it instead of repeating itself for every node
    left in the trie.
    """
    try _walk(root, 0, 0, true)? else (0, "the walk raised") end

  fun _walk(
    node: _MapSubNodes[U64, U64, _TrieHash],
    depth: U32,
    path: U32,
    is_root: Bool)
    : (USize, String) ?
  =>
    """
    `path` is the hash bits every key below this node shares: the level
    indices from the root down to `depth`, each in its own five bits.
    """
    let at: String val =
      " at depth " + depth.string() + ", path " + path.string()

    if (node.data_map and node.node_map) != 0 then
      return (0, "bitmaps overlap" + at)
    end
    let counted = (node.data_map.popcount() + node.node_map.popcount()).usize()
    if counted != node.nodes.size() then
      return (0, "node count disagrees with the bitmaps" + at)
    end
    if depth > 5 then return (0, "subnode below depth five" + at) end

    if not is_root then
      // only the root may hold nothing: there is no parent to compact it
      // into. `MapPairs` relies on this to iterate without a guard per node.
      if node.nodes.size() == 0 then
        return (0, "empty node below the root" + at)
      end
      if (node.nodes.size() == 1) and (node.data_map != 0) then
        return (0, "a lone entry was left uncompacted" + at)
      end
    end

    var found: USize = 0
    for idx in mut.Range[U32](0, 32) do
      let c_idx = node.compressed_idx(idx)
      if c_idx != -1 then
        // the bits a key in this slot has from the root down to here
        let slot = path or (idx << (depth * 5))
        let in_slot: String val = at + ", slot " + idx.string()

        match \exhaustive\ node.nodes(c_idx.usize_unsafe())?
        | let e: _MapEntry[U64, U64, _TrieHash] val =>
          // an entry reached by walking to this slot must be one the walk
          // for its own key would reach. Compaction lifts entries between
          // slots, and a lift to the wrong one leaves every count and
          // bitmap intact.
          let mask = (U32(1) << ((depth + 1) * 5)) - 1
          if (_TrieHash.hash(e.key).u32() and mask) != slot then
            return (found, "entry off its hash path" + in_slot)
          end
          found = found + 1
        | let sn: _MapSubNodes[U64, U64, _TrieHash] val =>
          (let n, let err) = _walk(sn, depth + 1, slot, false)?
          found = found + n
          if err != "" then return (found, err) end
        | let cs: _MapCollisions[U64, U64, _TrieHash] val =>
          if depth != 5 then
            return (found, "collisions node above depth five" + in_slot)
          end
          var n: USize = 0
          for (b, bin) in cs.bins.pairs() do
            for e in bin.values() do
              let hash = _TrieHash.hash(e.key).u32()
              // depths zero to five take bits 0 to 29; the bin is 30 and 31
              if (hash and 0x3FFF_FFFF) != slot then
                return (found + n, "collision entry off its path" + in_slot)
              end
              if _Bits.mask32(hash, _Bits.collision_depth()) != b.u32() then
                return (found + n, "collision entry in the wrong bin" + in_slot)
              end
              n = n + 1
            end
          end
          found = found + n
          if n < 2 then
            return (found, "collisions node holding one entry" + in_slot)
          end
        end
      end
    end
    (found, "")

class \nodoc\ iso _MapStructureProperty is Property1[Array[_MapAction]]
  """
  The trie's shape after every operation, checked against the invariants the
  node code maintains rather than against what a lookup returns.
  """
  fun name(): String => "collections/persistent/Map (property: structure)"

  fun gen(): Generator[Array[_MapAction]] =>
    Generators.seq_of[_MapAction, Array[_MapAction]](
      _MapGen.actions(Generators.one_of[U8]([as U8: 0; 1; 2])), 0, 60)

  fun ref property(arg1: Array[_MapAction], h: PropertyHelper) ? =>
    var m = _TrieMap
    let model = mut.Map[U64, U64]

    for (i, action) in arg1.pairs() do
      let k = _MapGen.key(action.seed)
      let n = model.size()
      match action.op
      | 0 | 2 =>
        m = m.update(k, action.value)
        model(k) = action.value
      | 1 =>
        if n > 0 then
          let k' = _MapGen.nth_key(model, action.value.usize() % n)?
          m = m.remove(k')?
          model.remove(k')?
        end
      end
      let after: String val =
        " after action " + i.string() + " " + action.string()
      (let leaves, let broken) = _MapShape.check(m._root_node())
      h.assert_eq[String]("", broken, "shape" + after)
      h.assert_eq[USize](m.size(), leaves, "leaf count" + after)
    end

class \nodoc\ iso _TestSetRemoveAbsent is UnitTest
  """
  `HashSet` has no `remove`, so `sub` is its only removal, and `sub` returns a
  set rather than raising. The set a caller gets back is the only evidence
  that anything went wrong.

  `without` and `op_xor` reach the same path without the caller removing
  anything at all. Both test membership against the receiver but subtract from
  an accumulator, so an argument that yields an element twice subtracts it
  twice, and the second subtraction asks the accumulator to drop an element it
  no longer holds. Reading the receiver is what makes the toggle in `op_xor`
  idempotent, so these depend on `sub` leaving a set alone when the element is
  not there.
  """
  fun name(): String => "collections/persistent/Set (remove absent)"

  fun apply(h: TestHelper) =>
    // a and b share the root slot, so a stray removal of one takes the other
    // and the loss is visible rather than a matter of chance
    let a = _TrieKey([4])
    let b = _TrieKey([4; 6])
    // walks to a's slot and stops on it, which is where a removal that does
    // not compare the element takes a with it
    let absent = _TrieKey([4; 0; 9])
    // stops on a slot holding nothing, so a removal has nothing to take
    let unrelated = _TrieKey([7])
    let s = (HashSet[U64, _TrieHash] + a) + b
    _expect(h, "built", s, [a; b])

    _expect(h, "sub absent", s - absent, [a; b])
    _expect(h, "sub unrelated", s - unrelated, [a; b])
    _expect(h, "sub present", s - a, [b])

    // an argument yielding the same element twice removes it twice
    _expect(h, "without duplicate", s.without([a; a].values()), [b])
    _expect(h, "xor duplicate present", s.op_xor([a; a].values()), [b])

    // the same duplicate on the adding side of the toggle: xor against the
    // elements of an argument, not against each yield of it
    _expect(
      h,
      "xor duplicate absent",
      s.op_xor([absent; absent].values()),
      [a; b; absent])

    // every operator also takes a bare Iterator, which is the arm that a set
    // operand never reaches
    _expect(h, "or iterator", s.op_or([absent].values()), [a; b; absent])
    _expect(h, "and iterator", s.op_and([a; absent].values()), [a])
    _expect(h, "without iterator", s.without([b].values()), [a])

    // and the receiver is unchanged by any of it
    _expect(h, "receiver", s, [a; b])

  fun _expect(
    h: TestHelper,
    label: String,
    got: HashSet[U64, _TrieHash],
    want: Array[U64])
  =>
    h.assert_eq[USize](want.size(), got.size(), label + ": size")
    for v in want.values() do
      h.assert_true(got.contains(v), label + ": element missing")
    end
