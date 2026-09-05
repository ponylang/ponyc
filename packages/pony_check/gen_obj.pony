use "collections"
use persistent = "collections/persistent"
use "itertools"

trait box GenObj[T]
  """
  Defines how to produce a random value for property-based testing.

  Generators are pure functions of a `Randomness` source. The framework records
  every random decision during generation, then replays generators against
  mutated decision sequences to produce shrunk values. Users never implement
  shrinking logic.
  """
  fun generate(rnd: Randomness): T^ ?
    """
    Produce a random value from the given source of randomness.
    """

class box Generator[T] is GenObj[T]
  """
  Produces random values of type `T` given a source of `Randomness`.

  Shrinking is handled automatically by the framework through choice-sequence
  recording and replay — generators do not provide shrink logic.
  """
  let _gen: GenObj[T]

  new create(gen: GenObj[T]) =>
    _gen = gen

  fun generate(rnd: Randomness): T^ ? =>
    _gen.generate(rnd)?

  fun filter(predicate: {(T): (T^, Bool)} box): Generator[T] =>
    """
    Only yield values for which `predicate` returns `true`.

    Rejected values are retried up to 100 times. Each attempt is
    recorded in its own discardable span so the shrinker can remove
    the failed draws cleanly. Errors after exhausting retries.
    """
    Generator[T](
      object is GenObj[T]
        fun generate(rnd: Randomness): T^ ? =>
          var tries: USize = 0
          while tries < 100 do
            rnd.start_span(1)
            let t: T = _gen.generate(rnd)?
            (let t1, let matches) = predicate(consume t)
            if matches then
              rnd.end_span()
              return consume t1
            end
            rnd.end_span(true)
            tries = tries + 1
          end
          error
      end)

  fun map[U](fn: {(T): U^} box): Generator[U] =>
    """
    Apply `fn` to each generated value.
    """
    Generator[U](
      object is GenObj[U]
        fun generate(rnd: Randomness): U^ ? =>
          fn(_gen.generate(rnd)?)
      end)

  fun flat_map[U](fn: {(T): Generator[U]} box): Generator[U] =>
    """
    For each value of this generator, create a generator that is then combined.
    Both outer and inner choices are in the same sequence, so shrinking the
    outer value works automatically.
    """
    Generator[U](
      object is GenObj[U]
        fun generate(rnd: Randomness): U^ ? =>
          let outer: T = _gen.generate(rnd)?
          fn(consume outer).generate(rnd)?
      end)

  fun union[U](other: Generator[U]): Generator[(T | U)] =>
    """
    Produce the value of this generator or the other with equal probability.
    """
    Generator[(T | U)](
      object is GenObj[(T | U)]
        fun generate(rnd: Randomness): (T^ | U^) ? =>
          if rnd.bool()? then
            _gen.generate(rnd)?
          else
            other.generate(rnd)?
          end
      end)

type WeightedGenerator[T] is (USize, Generator[T] box)

primitive Generators
  """
  Convenience combinators and factories for common types of Generators.
  """

  fun unit[T](t: T): Generator[box->T] =>
    """
    Generate a reference to the same value over and over again.
    """
    Generator[box->T](
      object is GenObj[box->T]
        let _t: T = consume t
        fun generate(rnd: Randomness): box->T => _t
      end)

  fun none[T: None](): Generator[(T | None)] =>
    Generators.unit[(T | None)](None)

  fun repeatedly[T](f: {(): T^ ?} box): Generator[T] =>
    """
    Generate values by calling the lambda `f` repeatedly.

    Values generated this way produce zero recorded choices and cannot
    be shrunk. Use a proper generator that draws from `Randomness` when
    shrinking matters.
    """
    Generator[T](
      object is GenObj[T]
        fun generate(rnd: Randomness): T^ ? => f()?
      end)

  fun array_of[T](
    gen: Generator[T],
    from: USize = 0,
    to: USize = 100)
    : Generator[Array[T]]
  =>
    """
    Generate an `Array[T]` with size in the range `from` to `to`.
    Uses boolean-per-element encoding for shrinking: each element is preceded
    by a boolean deciding whether to include it. Shrinking deletes elements
    while preserving all others exactly.
    """
    let lo = from.min(to)
    let hi = from.max(to)

    Generator[Array[T]](
      object is GenObj[Array[T]]
        let _gen: GenObj[T] = gen
        fun generate(rnd: Randomness): Array[T]^ ? =>
          let result = Array[T](hi)
          var count: USize = 0
          while count < hi do
            rnd.start_span(2)
            let cont =
              if count < lo then
                rnd.bool()?
                true
              else
                rnd.bool()?
              end
            if cont then
              let elem = _gen.generate(rnd)?
              result.push(consume elem)
              count = count + 1
              rnd.end_span()
            else
              rnd.end_span()
              break
            end
          end
          result
      end)

  fun iso_seq_of[T: Any #send, S: Seq[T] iso](
    gen: Generator[T],
    from: USize = 0,
    to: USize = 100)
    : Generator[S]
  =>
    """
    Generate a `Seq[T]` where `T` must be sendable.
    Uses boolean-per-element encoding.
    """
    let lo = from.min(to)
    let hi = from.max(to)

    Generator[S](
      object is GenObj[S]
        let _gen: GenObj[T] = gen
        fun generate(rnd: Randomness): S^ ? =>
          let result: S = recover iso S.create(hi) end
          var count: USize = 0
          while count < hi do
            rnd.start_span(2)
            let cont =
              if count < lo then
                rnd.bool()?
                true
              else
                rnd.bool()?
              end
            if cont then
              let elem = _gen.generate(rnd)?
              result.push(consume elem)
              count = count + 1
              rnd.end_span()
            else
              rnd.end_span()
              break
            end
          end
          consume result
      end)

  fun seq_of[T, S: Seq[T] ref](
    gen: Generator[T],
    from: USize = 0,
    to: USize = 100)
    : Generator[S]
  =>
    """
    Create a `Seq` from generated values with size in `from` to `to`.
    Uses boolean-per-element encoding.
    """
    let lo = from.min(to)
    let hi = from.max(to)

    Generator[S](
      object is GenObj[S]
        let _gen: GenObj[T] = gen
        fun generate(rnd: Randomness): S^ ? =>
          let result: S = S.create(hi)
          var count: USize = 0
          while count < hi do
            rnd.start_span(2)
            let cont =
              if count < lo then
                rnd.bool()?
                true
              else
                rnd.bool()?
              end
            if cont then
              let elem = _gen.generate(rnd)?
              result.push(consume elem)
              count = count + 1
              rnd.end_span()
            else
              rnd.end_span()
              break
            end
          end
          result
      end)

  fun shuffled_array_gen[T](
    gen: Generator[Array[T]])
    : Generator[Array[T]]
  =>
    """
    Generate an array and shuffle it.
    """
    Generator[Array[T]](
      object is GenObj[Array[T]]
        let _gen: GenObj[Array[T]] = gen
        fun generate(rnd: Randomness): Array[T]^ ? =>
          let arr = _gen.generate(rnd)?
          rnd.shuffle[T](arr)?
          arr
      end)

  fun shuffled_iter[T](array: Array[T]): Generator[Iterator[this->T!]] =>
    """
    Shuffle the elements of the array and return an iterator.
    """
    Generator[Iterator[this->T!]](
      object is GenObj[Iterator[this->T!]]
        fun generate(rnd: Randomness): Iterator[this->T!]^ ? =>
          let cloned = array.clone()
          rnd.shuffle[this->T!](cloned)?
          cloned.values()
      end)

  fun list_of[T](
    gen: Generator[T],
    from: USize = 0,
    to: USize = 100)
    : Generator[List[T]]
  =>
    Generators.seq_of[T, List[T]](gen, from, to)

  fun set_of[T: (Hashable #read & Equatable[T] #read)](
    gen: Generator[T],
    from: USize = 0,
    to: USize = 100)
    : Generator[Set[T]]
  =>
    """
    Generate a `Set[T]` with elements in `from` to `to`.
    Uses boolean-per-element encoding with a stall guard for duplicate
    rejection.
    """
    let lo = from.min(to)
    let hi = from.max(to)

    Generator[Set[T]](
      object is GenObj[Set[T]]
        let _gen: GenObj[T] = gen
        fun generate(rnd: Randomness): Set[T]^ ? =>
          let result = Set[T].create(hi)
          var stall: USize = 0
          while (result.size() < hi) and (stall < 100) do
            rnd.start_span(2)
            let cont =
              if result.size() < lo then
                rnd.bool()?
                true
              else
                rnd.bool()?
              end
            if cont then
              let prev = result.size()
              result.set(_gen.generate(rnd)?)
              if result.size() == prev then
                stall = stall + 1
              else
                stall = 0
              end
              rnd.end_span()
            else
              rnd.end_span()
              break
            end
          end
          result
      end)

  fun set_is_of[T](
    gen: Generator[T],
    from: USize = 0,
    to: USize = 100)
    : Generator[SetIs[T]]
  =>
    """
    Generate a `SetIs[T]` with elements in `from` to `to`.
    """
    let lo = from.min(to)
    let hi = from.max(to)

    Generator[SetIs[T]](
      object is GenObj[SetIs[T]]
        fun generate(rnd: Randomness): SetIs[T]^ ? =>
          let result = SetIs[T].create(hi)
          var stall: USize = 0
          while (result.size() < hi) and (stall < 100) do
            rnd.start_span(2)
            let cont =
              if result.size() < lo then
                rnd.bool()?
                true
              else
                rnd.bool()?
              end
            if cont then
              let prev = result.size()
              result.set(gen.generate(rnd)?)
              if result.size() == prev then
                stall = stall + 1
              else
                stall = 0
              end
              rnd.end_span()
            else
              rnd.end_span()
              break
            end
          end
          result
      end)

  fun map_of[K: (Hashable #read & Equatable[K] #read), V](
    gen: Generator[(K, V)],
    from: USize = 0,
    to: USize = 100)
    : Generator[Map[K, V]]
  =>
    """
    Generate a `Map[K, V]` with entries in `from` to `to`.
    """
    let lo = from.min(to)
    let hi = from.max(to)

    Generator[Map[K, V]](
      object is GenObj[Map[K, V]]
        fun generate(rnd: Randomness): Map[K, V]^ ? =>
          let result = Map[K, V].create(hi)
          var stall: USize = 0
          while (result.size() < hi) and (stall < 100) do
            rnd.start_span(2)
            let cont =
              if result.size() < lo then
                rnd.bool()?
                true
              else
                rnd.bool()?
              end
            if cont then
              let prev = result.size()
              (let k, let v) = gen.generate(rnd)?
              result(consume k) = consume v
              if result.size() == prev then
                stall = stall + 1
              else
                stall = 0
              end
              rnd.end_span()
            else
              rnd.end_span()
              break
            end
          end
          result
      end)

  fun map_is_of[K, V](
    gen: Generator[(K, V)],
    from: USize = 0,
    to: USize = 100)
    : Generator[MapIs[K, V]]
  =>
    """
    Generate a `MapIs[K, V]` with entries in `from` to `to`.
    """
    let lo = from.min(to)
    let hi = from.max(to)

    Generator[MapIs[K, V]](
      object is GenObj[MapIs[K, V]]
        fun generate(rnd: Randomness): MapIs[K, V]^ ? =>
          let result = MapIs[K, V].create(hi)
          var stall: USize = 0
          while (result.size() < hi) and (stall < 100) do
            rnd.start_span(2)
            let cont =
              if result.size() < lo then
                rnd.bool()?
                true
              else
                rnd.bool()?
              end
            if cont then
              let prev = result.size()
              (let k, let v) = gen.generate(rnd)?
              result(consume k) = consume v
              if result.size() == prev then
                stall = stall + 1
              else
                stall = 0
              end
              rnd.end_span()
            else
              rnd.end_span()
              break
            end
          end
          result
      end)

  fun vec_of[T: Any #share](
    gen: Generator[T],
    from: USize = 0,
    to: USize = 100)
    : Generator[persistent.Vec[T]]
  =>
    """
    Generate a persistent `Vec[T]` with size in `from` to `to`.
    """
    let lo = from.min(to)
    let hi = from.max(to)

    Generator[persistent.Vec[T]](
      object is GenObj[persistent.Vec[T]]
        let _gen: GenObj[T] = gen
        fun generate(rnd: Randomness): persistent.Vec[T]^ ? =>
          var result = persistent.Vec[T]
          var count: USize = 0
          while count < hi do
            rnd.start_span(2)
            let cont =
              if count < lo then
                rnd.bool()?
                true
              else
                rnd.bool()?
              end
            if cont then
              result = result.push(_gen.generate(rnd)?)
              count = count + 1
              rnd.end_span()
            else
              rnd.end_span()
              break
            end
          end
          result
      end)

  fun persistent_list_of[T: Any #share](
    gen: Generator[T],
    from: USize = 0,
    to: USize = 100)
    : Generator[persistent.List[T]]
  =>
    """
    Generate a persistent `List[T]` with size in `from` to `to`.
    """
    let lo = from.min(to)
    let hi = from.max(to)

    Generator[persistent.List[T]](
      object is GenObj[persistent.List[T]]
        let _gen: GenObj[T] = gen
        fun generate(rnd: Randomness): persistent.List[T]^ ? =>
          var result: persistent.List[T] = persistent.Lists[T].empty()
          var count: USize = 0
          while count < hi do
            rnd.start_span(2)
            let cont =
              if count < lo then
                rnd.bool()?
                true
              else
                rnd.bool()?
              end
            if cont then
              result = result.prepend(_gen.generate(rnd)?)
              count = count + 1
              rnd.end_span()
            else
              rnd.end_span()
              break
            end
          end
          result.reverse()
      end)

  fun persistent_set_of[T: (Hashable val & Equatable[T] val)](
    gen: Generator[T],
    from: USize = 0,
    to: USize = 100)
    : Generator[persistent.Set[T]]
  =>
    """
    Generate a persistent `Set[T]` with elements in `from` to `to`.
    """
    let lo = from.min(to)
    let hi = from.max(to)

    Generator[persistent.Set[T]](
      object is GenObj[persistent.Set[T]]
        let _gen: GenObj[T] = gen
        fun generate(rnd: Randomness): persistent.Set[T]^ ? =>
          var result = persistent.Set[T].create()
          var stall: USize = 0
          while (result.size() < hi) and (stall < 100) do
            rnd.start_span(2)
            let cont =
              if result.size() < lo then
                rnd.bool()?
                true
              else
                rnd.bool()?
              end
            if cont then
              let prev = result.size()
              result = result + _gen.generate(rnd)?
              if result.size() == prev then
                stall = stall + 1
              else
                stall = 0
              end
              rnd.end_span()
            else
              rnd.end_span()
              break
            end
          end
          result
      end)

  fun persistent_set_is_of[T: Any #share](
    gen: Generator[T],
    from: USize = 0,
    to: USize = 100)
    : Generator[persistent.SetIs[T]]
  =>
    """
    Generate a persistent `SetIs[T]` with elements in `from` to `to`.
    """
    let lo = from.min(to)
    let hi = from.max(to)

    Generator[persistent.SetIs[T]](
      object is GenObj[persistent.SetIs[T]]
        let _gen: GenObj[T] = gen
        fun generate(rnd: Randomness): persistent.SetIs[T]^ ? =>
          var result = persistent.SetIs[T].create()
          var stall: USize = 0
          while (result.size() < hi) and (stall < 100) do
            rnd.start_span(2)
            let cont =
              if result.size() < lo then
                rnd.bool()?
                true
              else
                rnd.bool()?
              end
            if cont then
              let prev = result.size()
              result = result + _gen.generate(rnd)?
              if result.size() == prev then
                stall = stall + 1
              else
                stall = 0
              end
              rnd.end_span()
            else
              rnd.end_span()
              break
            end
          end
          result
      end)

  fun persistent_map_of[
    K: (Hashable val & Equatable[K] val),
    V: Any #share](
    gen: Generator[(K, V)],
    from: USize = 0,
    to: USize = 100)
    : Generator[persistent.Map[K, V]]
  =>
    """
    Generate a persistent `Map[K, V]` with entries in `from` to `to`.
    """
    let lo = from.min(to)
    let hi = from.max(to)

    Generator[persistent.Map[K, V]](
      object is GenObj[persistent.Map[K, V]]
        fun generate(rnd: Randomness): persistent.Map[K, V]^ ? =>
          var result = persistent.Map[K, V].create()
          var stall: USize = 0
          while (result.size() < hi) and (stall < 100) do
            rnd.start_span(2)
            let cont =
              if result.size() < lo then
                rnd.bool()?
                true
              else
                rnd.bool()?
              end
            if cont then
              let prev = result.size()
              (let k, let v) = gen.generate(rnd)?
              result = result.update(consume k, consume v)
              if result.size() == prev then
                stall = stall + 1
              else
                stall = 0
              end
              rnd.end_span()
            else
              rnd.end_span()
              break
            end
          end
          result
      end)

  fun persistent_map_is_of[K: Any #share, V: Any #share](
    gen: Generator[(K, V)],
    from: USize = 0,
    to: USize = 100)
    : Generator[persistent.MapIs[K, V]]
  =>
    """
    Generate a persistent `MapIs[K, V]` with entries in `from` to `to`.
    """
    let lo = from.min(to)
    let hi = from.max(to)

    Generator[persistent.MapIs[K, V]](
      object is GenObj[persistent.MapIs[K, V]]
        fun generate(rnd: Randomness): persistent.MapIs[K, V]^ ? =>
          var result = persistent.MapIs[K, V].create()
          var stall: USize = 0
          while (result.size() < hi) and (stall < 100) do
            rnd.start_span(2)
            let cont =
              if result.size() < lo then
                rnd.bool()?
                true
              else
                rnd.bool()?
              end
            if cont then
              let prev = result.size()
              (let k, let v) = gen.generate(rnd)?
              result = result.update(consume k, consume v)
              if result.size() == prev then
                stall = stall + 1
              else
                stall = 0
              end
              rnd.end_span()
            else
              rnd.end_span()
              break
            end
          end
          result
      end)

  fun one_of[T](xs: ReadSeq[T]): Generator[box->T] =>
    """
    Generate a random value from the given ReadSeq.
    Errors if `xs` is empty.
    """
    Generator[box->T](
      object is GenObj[box->T]
        fun generate(rnd: Randomness): box->T ? =>
          xs(rnd.usize(0, xs.size() - 1)?)?
      end)

  fun one_of_safe[T](xs: ReadSeq[T]): Generator[box->T] ? =>
    """
    Version of `one_of` that errors at construction time if `xs` is empty.
    """
    if xs.size() == 0 then error end
    Generators.one_of[T](xs)

  fun frequency[T](
    weighted_generators: ReadSeq[WeightedGenerator[T]])
    : Generator[T]
  =>
    """
    Choose a value from one of the given Generators, weighted by associated
    weights.
    """
    Generator[T](
      object is GenObj[T]
        fun generate(rnd: Randomness): T^ ? =>
          let weight_sum: USize =
            Iter[WeightedGenerator[T]](weighted_generators.values())
              .fold[USize](
                0,
                {(acc: USize, weighted_gen: WeightedGenerator[T]): USize^ =>
                  weighted_gen._1 + acc
                })
          let desired_sum = rnd.usize(0, weight_sum)?
          var running_sum: USize = 0
          for weighted_gen in weighted_generators.values() do
            let new_sum = running_sum + weighted_gen._1
            if
              ((desired_sum == 0) or
                ((running_sum < desired_sum) and
                  (desired_sum <= new_sum)))
            then
              return weighted_gen._2.generate(rnd)?
            else
              running_sum = new_sum
            end
          end
          error
      end)

  fun frequency_safe[T](
    weighted_generators: ReadSeq[WeightedGenerator[T]])
    : Generator[T] ?
  =>
    """
    Version of `frequency` that errors if the given `weighted_generators` is
    empty.
    """
    if weighted_generators.size() == 0 then error end
    Generators.frequency[T](weighted_generators)

  fun zip2[T1, T2](
    gen1: Generator[T1],
    gen2: Generator[T2])
    : Generator[(T1, T2)]
  =>
    Generator[(T1, T2)](
      object is GenObj[(T1, T2)]
        fun generate(rnd: Randomness): (T1^, T2^) ? =>
          (gen1.generate(rnd)?, gen2.generate(rnd)?)
      end)

  fun zip3[T1, T2, T3](
    gen1: Generator[T1],
    gen2: Generator[T2],
    gen3: Generator[T3])
    : Generator[(T1, T2, T3)]
  =>
    Generator[(T1, T2, T3)](
      object is GenObj[(T1, T2, T3)]
        fun generate(rnd: Randomness): (T1^, T2^, T3^) ? =>
          (gen1.generate(rnd)?, gen2.generate(rnd)?, gen3.generate(rnd)?)
      end)

  fun zip4[T1, T2, T3, T4](
    gen1: Generator[T1],
    gen2: Generator[T2],
    gen3: Generator[T3],
    gen4: Generator[T4])
    : Generator[(T1, T2, T3, T4)]
  =>
    Generator[(T1, T2, T3, T4)](
      object is GenObj[(T1, T2, T3, T4)]
        fun generate(rnd: Randomness): (T1^, T2^, T3^, T4^) ? =>
          (gen1.generate(rnd)?, gen2.generate(rnd)?,
            gen3.generate(rnd)?, gen4.generate(rnd)?)
      end)

  fun map2[T1, T2, T3](
    gen1: Generator[T1],
    gen2: Generator[T2],
    fn: {(T1, T2): T3^})
    : Generator[T3]
  =>
    Generators.zip2[T1, T2](gen1, gen2)
      .map[T3]({(arg) =>
        (let arg1, let arg2) = consume arg
        fn(consume arg1, consume arg2)
      })

  fun map3[T1, T2, T3, T4](
    gen1: Generator[T1],
    gen2: Generator[T2],
    gen3: Generator[T3],
    fn: {(T1, T2, T3): T4^})
    : Generator[T4]
  =>
    Generators.zip3[T1, T2, T3](gen1, gen2, gen3)
      .map[T4]({(arg) =>
        (let arg1, let arg2, let arg3) = consume arg
        fn(consume arg1, consume arg2, consume arg3)
      })

  fun map4[T1, T2, T3, T4, T5](
    gen1: Generator[T1],
    gen2: Generator[T2],
    gen3: Generator[T3],
    gen4: Generator[T4],
    fn: {(T1, T2, T3, T4): T5^})
    : Generator[T5]
  =>
    Generators.zip4[T1, T2, T3, T4](gen1, gen2, gen3, gen4)
      .map[T5]({(arg) =>
        (let arg1, let arg2, let arg3, let arg4) = consume arg
        fn(consume arg1, consume arg2, consume arg3, consume arg4)
      })

  fun bool(): Generator[Bool] =>
    Generator[Bool](
      object is GenObj[Bool]
        fun generate(rnd: Randomness): Bool ? => rnd.bool()?
      end)

  fun u8(
    from: U8 = U8.min_value(),
    to: U8 = U8.max_value())
    : Generator[U8]
  =>
    let lo = from.min(to)
    let hi = from.max(to)
    Generator[U8](
      object is GenObj[U8]
        fun generate(rnd: Randomness): U8 ? => rnd.u8(lo, hi)?
      end)

  fun u16(
    from: U16 = U16.min_value(),
    to: U16 = U16.max_value())
    : Generator[U16]
  =>
    let lo = from.min(to)
    let hi = from.max(to)
    Generator[U16](
      object is GenObj[U16]
        fun generate(rnd: Randomness): U16 ? => rnd.u16(lo, hi)?
      end)

  fun u32(
    from: U32 = U32.min_value(),
    to: U32 = U32.max_value())
    : Generator[U32]
  =>
    let lo = from.min(to)
    let hi = from.max(to)
    Generator[U32](
      object is GenObj[U32]
        fun generate(rnd: Randomness): U32 ? => rnd.u32(lo, hi)?
      end)

  fun u64(
    from: U64 = U64.min_value(),
    to: U64 = U64.max_value())
    : Generator[U64]
  =>
    let lo = from.min(to)
    let hi = from.max(to)
    Generator[U64](
      object is GenObj[U64]
        fun generate(rnd: Randomness): U64 ? => rnd.u64(lo, hi)?
      end)

  fun u128(
    from: U128 = U128.min_value(),
    to: U128 = U128.max_value())
    : Generator[U128]
  =>
    let lo = from.min(to)
    let hi = from.max(to)
    Generator[U128](
      object is GenObj[U128]
        fun generate(rnd: Randomness): U128 ? => rnd.u128(lo, hi)?
      end)

  fun usize(
    from: USize = USize.min_value(),
    to: USize = USize.max_value())
    : Generator[USize]
  =>
    let lo = from.min(to)
    let hi = from.max(to)
    Generator[USize](
      object is GenObj[USize]
        fun generate(rnd: Randomness): USize ? => rnd.usize(lo, hi)?
      end)

  fun ulong(
    from: ULong = ULong.min_value(),
    to: ULong = ULong.max_value())
    : Generator[ULong]
  =>
    let lo = from.min(to)
    let hi = from.max(to)
    Generator[ULong](
      object is GenObj[ULong]
        fun generate(rnd: Randomness): ULong ? => rnd.ulong(lo, hi)?
      end)

  fun i8(
    from: I8 = I8.min_value(),
    to: I8 = I8.max_value())
    : Generator[I8]
  =>
    let lo = from.min(to)
    let hi = from.max(to)
    Generator[I8](
      object is GenObj[I8]
        fun generate(rnd: Randomness): I8 ? => rnd.i8(lo, hi)?
      end)

  fun i16(
    from: I16 = I16.min_value(),
    to: I16 = I16.max_value())
    : Generator[I16]
  =>
    let lo = from.min(to)
    let hi = from.max(to)
    Generator[I16](
      object is GenObj[I16]
        fun generate(rnd: Randomness): I16 ? => rnd.i16(lo, hi)?
      end)

  fun i32(
    from: I32 = I32.min_value(),
    to: I32 = I32.max_value())
    : Generator[I32]
  =>
    let lo = from.min(to)
    let hi = from.max(to)
    Generator[I32](
      object is GenObj[I32]
        fun generate(rnd: Randomness): I32 ? => rnd.i32(lo, hi)?
      end)

  fun i64(
    from: I64 = I64.min_value(),
    to: I64 = I64.max_value())
    : Generator[I64]
  =>
    let lo = from.min(to)
    let hi = from.max(to)
    Generator[I64](
      object is GenObj[I64]
        fun generate(rnd: Randomness): I64 ? => rnd.i64(lo, hi)?
      end)

  fun i128(
    from: I128 = I128.min_value(),
    to: I128 = I128.max_value())
    : Generator[I128]
  =>
    let lo = from.min(to)
    let hi = from.max(to)
    Generator[I128](
      object is GenObj[I128]
        fun generate(rnd: Randomness): I128 ? => rnd.i128(lo, hi)?
      end)

  fun ilong(
    from: ILong = ILong.min_value(),
    to: ILong = ILong.max_value())
    : Generator[ILong]
  =>
    let lo = from.min(to)
    let hi = from.max(to)
    Generator[ILong](
      object is GenObj[ILong]
        fun generate(rnd: Randomness): ILong ? => rnd.ilong(lo, hi)?
      end)

  fun isize(
    from: ISize = ISize.min_value(),
    to: ISize = ISize.max_value())
    : Generator[ISize]
  =>
    let lo = from.min(to)
    let hi = from.max(to)
    Generator[ISize](
      object is GenObj[ISize]
        fun generate(rnd: Randomness): ISize ? => rnd.isize(lo, hi)?
      end)

  fun byte_string(
    gen: Generator[U8],
    from: USize = 0,
    to: USize = 100)
    : Generator[String]
  =>
    """
    Generate a string from bytes with length in `from` to `to`.
    Uses boolean-per-element encoding.
    """
    let lo = from.min(to)
    let hi = from.max(to)

    Generator[String](
      object is GenObj[String]
        fun generate(rnd: Randomness): String^ ? =>
          let arr: Array[U8] iso = recover Array[U8](hi) end
          var count: USize = 0
          while count < hi do
            rnd.start_span(2)
            let cont =
              if count < lo then
                rnd.bool()?
                true
              else
                rnd.bool()?
              end
            if cont then
              arr.push(gen.generate(rnd)?)
              count = count + 1
              rnd.end_span()
            else
              rnd.end_span()
              break
            end
          end
          String.from_iso_array(consume arr)
      end)

  fun ascii(
    from: USize = 0,
    to: USize = 100,
    range: ASCIIRange = ASCIIAll)
    : Generator[String]
  =>
    """
    Generate a string within the given `range` with length in `from` to `to`.
    """
    let range_bytes = range.apply()
    let fallback = U8(0)
    let range_bytes_gen = usize(0, range_bytes.size() - 1)
      .map[U8]({(size) =>
        try
          range_bytes(size)?
        else
          fallback
        end
      })
    byte_string(range_bytes_gen, from, to)

  fun ascii_printable(
    from: USize = 0,
    to: USize = 100)
    : Generator[String]
  =>
    ascii(from, to, ASCIIPrintable)

  fun ascii_numeric(
    from: USize = 0,
    to: USize = 100)
    : Generator[String]
  =>
    ascii(from, to, ASCIIDigits)

  fun ascii_letters(
    from: USize = 0,
    to: USize = 100)
    : Generator[String]
  =>
    ascii(from, to, ASCIILetters)

  fun utf32_codepoint_string(
    gen: Generator[U32],
    from: USize = 0,
    to: USize = 100)
    : Generator[String]
  =>
    """
    Generate a string from unicode codepoints with length in `from` to `to`
    codepoints.
    """
    let lo = from.min(to)
    let hi = from.max(to)

    Generator[String](
      object is GenObj[String]
        fun generate(rnd: Randomness): String^ ? =>
          let s: String iso = recover String(hi) end
          var count: USize = 0
          while count < hi do
            rnd.start_span(2)
            let cont =
              if count < lo then
                rnd.bool()?
                true
              else
                rnd.bool()?
              end
            if cont then
              var cp = gen.generate(rnd)?
              while (cp > 0xD7FF) and (cp < 0xE000) do
                cp = gen.generate(rnd)?
              end
              s.push_utf32(cp)
              count = count + 1
              rnd.end_span()
            else
              rnd.end_span()
              break
            end
          end
          consume s
      end)

  fun unicode(
    from: USize = 0,
    to: USize = 100)
    : Generator[String]
  =>
    """
    Generate a random String of Unicode code points.
    """
    let range_1 = u32(0x0, 0xD7FF)
    let range_1_size: USize = 0xD7FF
    let range_2 = u32(0xE000, 0x10FFFF)
    let range_2_size = U32(0x10FFFF - 0xE000).usize()

    let code_point_gen =
      frequency[U32](
        [ (range_1_size, range_1)
          (range_2_size, range_2)
        ])
    utf32_codepoint_string(code_point_gen, from, to)

  fun unicode_bmp(
    from: USize = 0,
    to: USize = 100)
    : Generator[String]
  =>
    """
    Generate a random String of Unicode BMP code points.
    """
    let range_1 = u32(0x0, 0xD7FF)
    let range_1_size: USize = 0xD7FF
    let range_2 = u32(0xE000, 0xFFFF)
    let range_2_size = U32(0xFFFF - 0xE000).usize()

    let code_point_gen =
      frequency[U32](
        [ (range_1_size, range_1)
          (range_2_size, range_2)
        ])
    utf32_codepoint_string(code_point_gen, from, to)
