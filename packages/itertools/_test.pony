use "ponytest"
use "collections"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None
  fun tag tests(test: PonyTest) =>
    test(_TestChain)
    test(_TestZip)
    test(_TestRepeat)
    test(_TestTake)
    test(_TestCycle)
    test(_TestMapFn)
    test(_TestFilter)

class iso _TestChain is UnitTest
  fun name(): String => "itertools/Chain"

  fun apply(h: TestHelper) =>
    None
    let input0: Array[String] = Array[String]
    let input1 = ["a", "b", "c"]
    let input2 = ["d", "e", "f"]
    let expected = ["a", "b", "c", "d", "e", "f"]
    var actual = Array[String]

    for x in Chain[String]([input1.values(), input2.values()].values()) do
      actual.push(x)
    end

    h.assert_array_eq[String](expected, actual)
    actual.clear()

    for x in Chain[String]([input0.values(), input1.values(), input2.values()].
      values()) do
      actual.push(x)
    end

    h.assert_array_eq[String](expected, actual)
    actual.clear()

    for x in Chain[String]([input1.values(), input2.values(), input0.values()].
      values()) do
      actual.push(x)
    end
  
    h.assert_array_eq[String](expected, actual)
    actual.clear()

    for x in Chain[String]([input0.values()].values()) do
      actual.push(x)
    end
  
    h.assert_array_eq[String](input0, actual)
    actual.clear()

    for x in Chain[String]([input0.values(), input0.values()].values()) do
      actual.push(x)
    end
  
    h.assert_array_eq[String](input0, actual)
    actual.clear()

    for x in Chain[String](Array[Iterator[String]].values()) do
      actual.push(x)
    end
  
    h.assert_array_eq[String](input0, actual)
    actual.clear()

    h.assert_false(Chain[String](Array[Iterator[String]].values()).has_next())

    let chain = Chain[String]([input0.values(), input1.values(), input0.values()].values())
    h.assert_true(chain.has_next())
    try
      chain.next()
      chain.next()
      chain.next()
      h.assert_false(chain.has_next())
    else
      h.fail()
    end

class iso _TestZip is UnitTest
  fun name(): String => "itertools/Zip"

  fun apply(h: TestHelper) =>
    let input1 = ["a", "b", "c"]
    let input2 = [as U32: 1, 2, 3, 4]
    let input3 = [as F32: 75.8, 90.1, 82.7, 13.4, 17.9]
    let input4 = [as I32: 51, 62, 73, 84]
    let input5 = [as USize: 14, 27, 39]

    let expected1 = ["a", "b", "c"]
    let expected2 = [as U32: 1, 2, 3]
    let expected3 = [as F32: 75.8, 90.1, 82.7, 13.4, 17.9]
    let expected4 = [as I32: 51, 62, 73]
    let expected5 = [as USize: 14, 27, 39]

    let actual1 = Array[String]
    let actual2 = Array[U32]
    let actual3 = Array[F32]
    let actual4 = Array[I32]
    let actual5 = Array[USize]

    for (a1, a2, a3, a4, a5) in
      Zip5[String, U32, F32, I32, USize](input1.values(), input2.values(),
      input3.values(), input4.values(), input5.values()) do
      actual1.push(a1)
      actual2.push(a2)
      actual3.push(a3)
      actual4.push(a4)
      actual5.push(a5)
    end

    h.assert_array_eq[String](expected1, actual1)

    // Skipping for now because of issue 563
    // (https://github.com/ponylang/ponyc/issues/563)
    // h.assert_array_eq[U32](expected2, actual2)
    // h.assert_array_eq[F32](expected3, actual3)
    // h.assert_array_eq[I32](expected4, actual4)
    // h.assert_array_eq[USize](expected5, actual5)

class iso _TestRepeat is UnitTest
  fun name(): String => "itertools/Repeat"

  fun apply(h: TestHelper) =>
    let input = "a"
    let expected = ["a", "a", "a"]
    var actual = Array[String]

    let repeater = Repeat[String]("a")

    actual.push(repeater.next()) // 1
    actual.push(repeater.next()) // 2
    actual.push(repeater.next()) // 3 ... Repeater!

    h.assert_array_eq[String](expected, actual)

class iso _TestTake is UnitTest
  fun name(): String => "itertools/Take"

  fun apply(h: TestHelper) =>
    let take: USize = 3
    let input = ["a", "b", "c", "d", "e"]
    let expected = ["a", "b", "c"]
    var actual = Array[String]

    for x in Take[String](input.values(), take) do
      actual.push(x)
    end

    h.assert_array_eq[String](expected, actual)

class iso _TestCycle is UnitTest
  fun name(): String => "itertools/Cycle"

  fun apply(h: TestHelper) =>
    let input = ["a", "b", "c"]
    let expected = ["a", "b", "c", "a", "b", "c", "a", "b", "c", "a"]
    var actual: Array[String] = Array[String]

    let cycle = Cycle[String](input.values())
    try
      actual.push(cycle.next()) // 1 "a"
      actual.push(cycle.next()) // 2 "b"
      actual.push(cycle.next()) // 3 "c"
      actual.push(cycle.next()) // 4 "a" <= start first cached cycle
      actual.push(cycle.next()) // 5 "b"
      actual.push(cycle.next()) // 6 "c"
      actual.push(cycle.next()) // 7 "a" <= start second cached cycle
      actual.push(cycle.next()) // 8 "b"
      actual.push(cycle.next()) // 9 "c"
      actual.push(cycle.next()) // 10 "a" <= start third cached cycle
    else
      h.fail()
    end
    
    h.assert_array_eq[String](expected, actual)

class iso _TestMapFn is UnitTest
  fun name(): String => "itertools/MapFn"

  fun apply(h: TestHelper) =>
    let input = ["a", "b", "c"]
    let expected = ["ab", "bb", "cb"]
    let actual = Array[String]

    let fn = lambda (x: String): String => x + "b" end
    for x in MapFn[String, String](input.values(), fn) do
      actual.push(x)
    end

    h.assert_array_eq[String](actual, expected)

class iso _TestFilter is UnitTest
  fun name(): String => "itertools/Filter"

  fun apply(h: TestHelper) =>
    let input1 = ["ax", "bxx", "c", "dx", "exx", "f", "g", "hx"]
    let input2 = ["ax", "bxx", "c", "dx", "exx", "f", "g"]
    let input3 = ["c", "dx", "exx", "f", "g"]
    let expected = ["c", "f", "g"]
    let actual = Array[String]

    let fn1 = lambda (x: String): Bool => x.size() == 1 end
    for x in Filter[String](input1.values(), fn1) do
      actual.push(x)
    end

    h.assert_array_eq[String](actual, expected)
    actual.clear()

    let fn2 = lambda (x: String): Bool => x.size() == 1 end
    for x in Filter[String](input2.values(), fn2) do
      actual.push(x)
    end

    h.assert_array_eq[String](actual, expected)
    actual.clear()

    let fn3 = lambda (x: String): Bool => x.size() == 1 end
    for x in Filter[String](input3.values(), fn3) do
      actual.push(x)
    end

    h.assert_array_eq[String](actual, expected)
