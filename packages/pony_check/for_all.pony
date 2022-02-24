use "ponytest"

class ForAll[T]
  let _gen: Generator[T] val
  let _helper: TestHelper

  new create(gen': Generator[T] val, testHelper: TestHelper) =>
    _gen = gen'
    _helper = testHelper

  fun ref apply(prop: {(T, PropertyHelper) ?} val) ? =>
    """execute"""
    Property1UnitTest[T](
      object iso is Property1[T]
        fun name(): String => ""

        fun gen(): Generator[T] => _gen

        fun ref property(arg1: T, h: PropertyHelper) ? =>
          prop(consume arg1, h)?
      end
    ).apply(_helper)?

class ForAll2[T1, T2]
  let _gen1: Generator[T1] val
  let _gen2: Generator[T2] val
  let _helper: TestHelper

  new create(
    gen1': Generator[T1] val,
    gen2': Generator[T2] val,
    h: TestHelper)
  =>
    _gen1 = gen1'
    _gen2 = gen2'
    _helper = h

  fun ref apply(prop: {(T1, T2, PropertyHelper) ?} val) ? =>
    Property2UnitTest[T1, T2](
      object iso is Property2[T1, T2]
        fun name(): String => ""
        fun gen1(): Generator[T1] => _gen1
        fun gen2(): Generator[T2] => _gen2
        fun ref property2(arg1: T1, arg2: T2, h: PropertyHelper) ? =>
          prop(consume arg1, consume arg2, h)?
      end
    ).apply(_helper)?

class ForAll3[T1, T2, T3]
  let _gen1: Generator[T1] val
  let _gen2: Generator[T2] val
  let _gen3: Generator[T3] val
  let _helper: TestHelper

  new create(
    gen1': Generator[T1] val,
    gen2': Generator[T2] val,
    gen3': Generator[T3] val,
    h: TestHelper)
  =>
    _gen1 = gen1'
    _gen2 = gen2'
    _gen3 = gen3'
    _helper = h

  fun ref apply(prop: {(T1, T2, T3, PropertyHelper) ?} val) ? =>
    Property3UnitTest[T1, T2, T3](
      object iso is Property3[T1, T2, T3]
        fun name(): String => ""
        fun gen1(): Generator[T1] => _gen1
        fun gen2(): Generator[T2] => _gen2
        fun gen3(): Generator[T3] => _gen3
        fun ref property3(arg1: T1, arg2: T2, arg3: T3, h: PropertyHelper) ? =>
          prop(consume arg1, consume arg2, consume arg3, h)?
      end
    ).apply(_helper)?

class ForAll4[T1, T2, T3, T4]
  let _gen1: Generator[T1] val
  let _gen2: Generator[T2] val
  let _gen3: Generator[T3] val
  let _gen4: Generator[T4] val
  let _helper: TestHelper

  new create(
    gen1': Generator[T1] val,
    gen2': Generator[T2] val,
    gen3': Generator[T3] val,
    gen4': Generator[T4] val,
    h: TestHelper)
  =>
    _gen1 = gen1'
    _gen2 = gen2'
    _gen3 = gen3'
    _gen4 = gen4'
    _helper = h

  fun ref apply(prop: {(T1, T2, T3, T4, PropertyHelper) ?} val) ? =>
    Property4UnitTest[T1, T2, T3, T4](
      object iso is Property4[T1, T2, T3, T4]
        fun name(): String => ""
        fun gen1(): Generator[T1] => _gen1
        fun gen2(): Generator[T2] => _gen2
        fun gen3(): Generator[T3] => _gen3
        fun gen4(): Generator[T4] => _gen4
        fun ref property4(arg1: T1, arg2: T2, arg3: T3, arg4: T4, h: PropertyHelper) ? =>
          prop(consume arg1, consume arg2, consume arg3, consume arg4, h)?
      end
    ).apply(_helper)?

