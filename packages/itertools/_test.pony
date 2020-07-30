use "collections"
use "ponytest"
use "time"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestIterChain)
    test(_TestIterRepeatValue)
    test(_TestIterAll)
    test(_TestIterAny)
    test(_TestIterCollect)
    test(_TestIterCount)
    test(_TestIterCycle)
    test(_TestIterEnum)
    test(_TestIterFilter)
    test(_TestIterFilterMap)
    test(_TestIterFind)
    test(_TestIterFlatMap)
    test(_TestIterFold)
    test(_TestIterLast)
    test(_TestIterMap)
    test(_TestIterNextOr)
    test(_TestIterNth)
    test(_TestIterRun)
    test(_TestIterSkip)
    test(_TestIterSkipWhile)
    test(_TestIterTake)
    test(_TestIterTakeWhile)
    test(_TestIterZip)

class iso _TestIterChain is UnitTest
  fun name(): String => "itertools/Iter.chain"

  fun apply(h: TestHelper) =>
    let input0: Array[String] = Array[String]
    let input1 = ["a"; "b"; "c"]
    let input2 = ["d"; "e"; "f"]
    let expected = ["a"; "b"; "c"; "d"; "e"; "f"]
    var actual = Array[String]

    for x in Iter[String].chain([input1.values(); input2.values()].values()) do
      actual.push(x)
    end

    h.assert_array_eq[String](expected, actual)
    actual.clear()

    for x in
      Iter[String].chain(
        [input0.values(); input1.values(); input2.values()].values())
    do
      actual.push(x)
    end

    h.assert_array_eq[String](expected, actual)
    actual.clear()

    for x in
      Iter[String].chain(
        [input1.values(); input2.values(); input0.values()].values())
    do
      actual.push(x)
    end

    h.assert_array_eq[String](expected, actual)
    actual.clear()

    for x in Iter[String].chain([input0.values()].values()) do
      actual.push(x)
    end

    h.assert_array_eq[String](input0, actual)
    actual.clear()

    for x in Iter[String].chain([input0.values(); input0.values()].values()) do
      actual.push(x)
    end

    h.assert_array_eq[String](input0, actual)
    actual.clear()

    for x in Iter[String].chain(Array[Iterator[String]].values()) do
      actual.push(x)
    end

    h.assert_array_eq[String](input0, actual)
    actual.clear()

    h.assert_false(Iter[String].chain(Array[Iterator[String]].values()).has_next())

    let chain =
      Iter[String].chain(
        [input0.values(); input1.values(); input0.values()].values())
    h.assert_true(chain.has_next())
    try
      chain.next()?
      chain.next()?
      chain.next()?
      h.assert_false(chain.has_next())
    else
      h.fail()
    end

class iso _TestIterRepeatValue is UnitTest
  fun name(): String => "itertools/Iter.repeat_value"

  fun apply(h: TestHelper) ? =>
    let repeater = Iter[String].repeat_value("a")
    for _ in Range(0, 3) do
      h.assert_eq[String]("a", repeater.next()?)
    end

class iso _TestIterAll is UnitTest
  fun name(): String => "itertools/Iter.all"

  fun apply(h: TestHelper) ? =>
    let input = [as I64: 1; 3; 6; 7; 9]
    let is_positive = {(n: I64): Bool => n > 0 }
    h.assert_true(Iter[I64](input.values()).all(is_positive))
    input(2)? = -6
    h.assert_false(Iter[I64](input.values()).all(is_positive))

class iso _TestIterAny is UnitTest
  fun name(): String => "itertools/Iter.any"

  fun apply(h: TestHelper) ? =>
    let input = [as I64: -1; -3; 6; -7; -9]
    let is_positive = {(n: I64): Bool => n > 0 }
    h.assert_true(Iter[I64](input.values()).any(is_positive))
    input(2)? = -6
    h.assert_false(Iter[I64](input.values()).any(is_positive))

class iso _TestIterCollect is UnitTest
  fun name(): String => "itertools/Iter.collect"

  fun apply(h: TestHelper) =>
    let input = [as I64: 1; 2; 3; 4; 5]
    h.assert_array_eq[I64](
      input,
      Iter[I64](input.values()).collect(Array[I64]))

class iso _TestIterCount is UnitTest
  fun name(): String => "itertools/Iter.count"

  fun apply(h: TestHelper) =>
    let input1 = [as I64: 1; 2; 3; 4; 5]
    h.assert_eq[USize](
      input1.size(),
      Iter[I64](input1.values()).count())

    let input2 = Array[I64]
    h.assert_eq[USize](
      input2.size(),
      Iter[I64](input2.values()).count())

class iso _TestIterCycle is UnitTest
  fun name(): String => "itertools/Iter.cycle"

  fun apply(h: TestHelper) =>
    let input = ["a"; "b"; "c"]
    let expected = ["a"; "b"; "c"; "a"; "b"; "c"; "a"; "b"; "c"; "a"]
    let actual: Array[String] = Array[String]

    let cycle = Iter[String](input.values()).cycle()
    try
      actual.push(cycle.next()?) // 1 "a"
      actual.push(cycle.next()?) // 2 "b"
      actual.push(cycle.next()?) // 3 "c"
      actual.push(cycle.next()?) // 4 "a" <= start first cached cycle
      actual.push(cycle.next()?) // 5 "b"
      actual.push(cycle.next()?) // 6 "c"
      actual.push(cycle.next()?) // 7 "a" <= start second cached cycle
      actual.push(cycle.next()?) // 8 "b"
      actual.push(cycle.next()?) // 9 "c"
      actual.push(cycle.next()?) // 10 "a" <= start third cached cycle
    else
      h.fail()
    end

    h.assert_array_eq[String](expected, actual)

class iso _TestIterEnum is UnitTest
  fun name(): String => "itertools/Iter.enum"

  fun apply(h: TestHelper) ? =>
    let input = ["a"; "b"; "c"]
    let expected = [as (USize, String): (0, "a"); (1, "b"); (2, "c")]

    let iter = Iter[String](input.values()).enum()

    var i: USize = 0
    for (n, s) in iter do
      h.assert_eq[USize](n, expected(i)?._1)
      h.assert_eq[String](s, expected(i)?._2)
      i = i + 1
    end
    h.assert_eq[USize](i, 3)

class iso _TestIterFilter is UnitTest
  fun name(): String => "itertools/Iter.filter"

  fun apply(h: TestHelper) =>
    let input1 = ["ax"; "bxx"; "c"; "dx"; "exx"; "f"; "g"; "hx"]
    let input2 = ["ax"; "bxx"; "c"; "dx"; "exx"; "f"; "g"]
    let input3 = ["c"; "dx"; "exx"; "f"; "g"]
    let expected = ["c"; "f"; "g"]
    let actual = Array[String]

    let fn1 = {(x: String): Bool => x.size() == 1 }
    for x in Iter[String](input1.values()).filter(fn1) do
      actual.push(x)
    end

    h.assert_array_eq[String](actual, expected)
    actual.clear()

    let fn2 = {(x: String): Bool => x.size() == 1 }
    for x in Iter[String](input2.values()).filter(fn2) do
      actual.push(x)
    end

    h.assert_array_eq[String](actual, expected)
    actual.clear()

    let fn3 = {(x: String): Bool => x.size() == 1 }
    for x in Iter[String](input3.values()).filter(fn3) do
      actual.push(x)
    end

    h.assert_array_eq[String](actual, expected)

class iso _TestIterFilterMap is UnitTest
  fun name(): String => "itertools/Iter.filter_map"

  fun apply(h: TestHelper) ? =>
    let input = ["1"; "2"; "cat"]
    let iter =
      Iter[String](input.values())
        .filter_map[I64](
          {(s: String): (I64 | None) =>
            (let i, let c) =
              try s.read_int[I64]()?
              else (0, 0)
              end
            if c > 0 then i end
          })
    h.assert_eq[I64](1, iter.next()?)
    h.assert_eq[I64](2, iter.next()?)
    h.assert_false(iter.has_next())

class iso _TestIterFind is UnitTest
  fun name(): String => "itertools/Iter.find"

  fun apply(h: TestHelper) ? =>
    let input = [as I64: 1; 2; 3; -4; 5]
    h.assert_error({() ? =>
      Iter[I64](input.values()).find({(x: I64): Bool => x == 0 })?
    })
    h.assert_eq[I64](-4, Iter[I64](input.values()).find({(x) => x < 0 })?)
    h.assert_error({() ? =>
      Iter[I64](input.values()).find({(x) => x < 0 }, 2)?
    })

    h.assert_eq[I64](5, Iter[I64](input.values()).find({(x) => x > 0 }, 4)?)

class iso _TestIterFlatMap is UnitTest
  fun name(): String => "itertools/Iter.flat_map"

  fun apply(h: TestHelper) ? =>
    h.assert_array_eq[U32](
      Iter[String](["alpha"; "beta"; "gamma"].values())
        .flat_map[U32]({(s: String): Iterator[U32] => s.values() })
        .collect(Array[U32]),
      [ as U32:
        'a'; 'l'; 'p'; 'h'; 'a'; 'b'; 'e'; 't'; 'a'; 'g'; 'a'; 'm'; 'm'; 'a'])
    h.assert_array_eq[U32](
      Iter[String]([""; "ab"; ""].values())
        .flat_map[U32]({(s: String): Iterator[U32] => s.values() })
        .collect(Array[U32]),
      [as U32: 'a'; 'b'])
    h.assert_array_eq[U32](
      Iter[String](["ab"; ""; "cd"].values())
        .flat_map[U32]({(s: String): Iterator[U32] => s.values() })
        .collect(Array[U32]),
      [as U32: 'a'; 'b'; 'c'; 'd'])
    h.assert_array_eq[U32](
      Iter[String](["ab"; "cd"; ""].values())
        .flat_map[U32]({(s: String): Iterator[U32] => s.values() })
        .collect(Array[U32]),
      [as U32: 'a'; 'b'; 'c'; 'd'])

    let iter =
      Iter[U8](Range[U8](1, 3))
        .flat_map[U8]({(n) => Range[U8](0, n) })
    h.assert_eq[U8](0, iter.next()?)
    h.assert_eq[U8](0, iter.next()?)
    h.assert_eq[U8](1, iter.next()?)
    h.assert_false(iter.has_next())

class iso _TestIterFold is UnitTest
  fun name(): String => "itertools/Iter.fold"

  fun apply(h: TestHelper) =>
    let ns = [as I64: 1; 2; 3; 4; 5; 6]
    let sum = Iter[I64](ns.values()).fold[I64](0, {(sum, n) => sum + n })
    h.assert_eq[I64](sum, 21)

    h.assert_error({() ? =>
      Iter[I64](ns.values())
        .fold_partial[I64](0, {(acc, x) ? => error })?
    })

class iso _TestIterLast is UnitTest
  fun name(): String => "itertools/Iter.last"

  fun apply(h: TestHelper) ? =>
    let input1 = Array[I64]
    h.assert_error({() ? => Iter[I64](input1.values()).last()? })

    let input2 =[as I64: 1]
    h.assert_eq[I64](1,
      Iter[I64](input2.values()).last()?)

    let input3 = [as I64: 1; 2]
    h.assert_eq[I64](2,
      Iter[I64](input3.values()).last()?)

    let input4 = [as I64: 1; 2; 3]
    h.assert_eq[I64](3,
      Iter[I64](input4.values()).last()?)

class iso _TestIterMap is UnitTest
  fun name(): String => "itertools/Iter.map"

  fun apply(h: TestHelper) =>
    let input = ["a"; "b"; "c"]
    let expected = ["ab"; "bb"; "cb"]
    let actual = Array[String]

    for x in Iter[String](input.values()).map[String]({(x) => x + "b" }) do
      actual.push(x)
    end

    h.assert_array_eq[String](actual, expected)

class iso _TestIterNextOr is UnitTest
  fun name(): String => "itertools/Iter.next_or"

  fun apply(h: TestHelper) =>
    let input: (U64 | None) = 42
    var iter = Iter[U64].maybe(input)
    h.assert_true(iter.has_next())
    h.assert_eq[U64](42, iter.next_or(0))
    h.assert_false(iter.has_next())
    h.assert_eq[U64](0, iter.next_or(0))
    h.assert_error({()? => Iter[U64].maybe(None).next()? })

    iter = Iter[U64].maybe(None)
    h.assert_false(iter.has_next())
    h.assert_eq[U64](1, iter.next_or(1))

class iso _TestIterNth is UnitTest
  fun name(): String => "itertools/Iter.nth"

  fun apply(h: TestHelper) ? =>
    let input = [as USize: 1; 2; 3]
    h.assert_eq[USize](1,
      Iter[USize](input.values()).nth(1)?)
    h.assert_eq[USize](2,
      Iter[USize](input.values()).nth(2)?)
    h.assert_eq[USize](3,
      Iter[USize](input.values()).nth(3)?)
    h.assert_error({()? => Iter[USize](input.values()).nth(4)? })
    h.assert_error({()? =>
        Iter[U8](
          object is Iterator[U8]
            var c: U8 = 0
            fun ref has_next(): Bool => c < 5
            fun ref next(): U8 =>
              c = c + 1
          end).nth(6)?
    })

class iso _TestIterRun is UnitTest
  fun name(): String => "itertools/Iter.run"

  fun apply(h: TestHelper) =>
    let actions =
      Map[String, Bool] .> concat(
        [ ("1", false)
          ("2", false)
          ("3", false)
          ("error", false) ].values())

    let xs = [as I64: 1; 2; 3]

    Iter[I64](xs.values())
      .map_stateful[None]({(x) => actions(x.string()) = true })
      .run()

    Iter[I64](
      object ref is Iterator[I64]
        fun ref has_next(): Bool => true
        fun ref next(): I64 ? => error
      end)
        .run({ref() => actions("error") = true })

    for (_, v) in actions.pairs() do h.assert_true(v) end

class iso _TestIterSkip is UnitTest
  fun name(): String => "itertools/Iter.skip"

  fun apply(h: TestHelper) ? =>
    let input = [as I64: 1]
    h.assert_error({()? =>
      Iter[I64](input.values()).skip(1).next()?
    })
    input.push(2)
    h.assert_eq[I64](2,
      Iter[I64](input.values()).skip(1).next()?)
    input.push(3)
    h.assert_eq[I64](3,
      Iter[I64](input.values()).skip(2).next()?)

    h.assert_false(Iter[I64](input.values()).skip(4).has_next())

class iso _TestIterSkipWhile is UnitTest
  fun name(): String => "itertools/Iter.skip_while"

  fun apply(h: TestHelper) ? =>
    let input = [as I64: -1; 0; 1; 2; 3]
    h.assert_eq[I64](1,
      Iter[I64](input.values()).skip_while({(x) => x <= 0 }).next()?)
    h.assert_eq[I64](-1,
      Iter[I64](input.values()).skip_while({(x) => x < -2 }).next()?)

class iso _TestIterTake is UnitTest
  fun name(): String => "itertools/Iter.take"

  fun apply(h: TestHelper) =>
    h.long_test(Nanos.from_seconds(10))
    let take: USize = 3
    let input = ["a"; "b"; "c"; "d"; "e"]
    let expected = ["a"; "b"; "c"]
    var actual = Array[String]

    for x in Iter[String](input.values()).take(take) do
      actual.push(x)
    end

    h.assert_array_eq[String](expected, actual)

    let infinite =
      Iter[U64](
        object ref
          fun ref has_next(): Bool => true
          fun ref next(): U64 => 0
        end)

    h.assert_eq[USize](3, infinite.take(3).collect(Array[U64]).size())

    let short = Iter[U64](
      object is Iterator[U64]
        var counter: U64 = 0
        fun ref has_next(): Bool => false
        fun ref next(): U64^ => counter = counter + 1
      end)
    h.assert_eq[USize](0, short.take(10).collect(Array[U64]).size())

    h.complete(true)

class iso _TestIterTakeWhile is UnitTest
  fun name(): String => "itertools/Iter.take_while"

  fun apply(h: TestHelper) ? =>
    let input = [as I64: -1; 0; 1; 2; 3]
    h.assert_array_eq[I64](
      [-1; 0],
      Iter[I64](input.values())
        .take_while({(x) => x < 1 })
        .collect(Array[I64]))
    h.assert_array_eq[I64](
      input,
      Iter[I64](input.values())
        .take_while({(x) => x < 4 })
        .collect(Array[I64]))
    h.assert_array_eq[I64](
      Array[I64],
      Iter[I64](input.values())
        .take_while({(x) ? => error })
        .collect(Array[I64]))

    let infinite =
      Iter[USize](Range(0, 3))
        .cycle()
        .take_while({(n) => n < 2 })
    h.assert_eq[USize](0, infinite.next()?)
    h.assert_eq[USize](1, infinite.next()?)
    h.assert_false(infinite.has_next())

class iso _TestIterZip is UnitTest
  fun name(): String => "itertools/Iter.zip"

  fun apply(h: TestHelper) =>
    let input1 = ["a"; "b"; "c"]
    let input2 = [as U32: 1; 2; 3; 4]
    let input3 = [as F32: 75.8; 90.1; 82.7; 13.4; 17.9]
    let input4 = [as I32: 51; 62; 73; 84]
    let input5 = [as USize: 14; 27; 39]

    let expected1 = ["a"; "b"; "c"]
    let expected2 = [as U32: 1; 2; 3]
    let expected3 = [as F32: 75.8; 90.1; 82.7]
    let expected4 = [as I32: 51; 62; 73]
    let expected5 = [as USize: 14; 27; 39]

    let actual1 = Array[String]
    let actual2 = Array[U32]
    let actual3 = Array[F32]
    let actual4 = Array[I32]
    let actual5 = Array[USize]

    for (a1, a2, a3, a4, a5) in
      Iter[String](input1.values())
        .zip4[U32, F32, I32, USize](input2.values(),
          input3.values(), input4.values(), input5.values())
    do
      actual1.push(a1)
      actual2.push(a2)
      actual3.push(a3)
      actual4.push(a4)
      actual5.push(a5)
    end

    h.assert_array_eq[String](expected1, actual1)
    h.assert_array_eq[U32](expected2, actual2)
    h.assert_array_eq[F32](expected3, actual3)
    h.assert_array_eq[I32](expected4, actual4)
    h.assert_array_eq[USize](expected5, actual5)
