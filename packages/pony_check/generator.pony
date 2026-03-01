use "collections"
use "assert"
use "itertools"
use "debug"

type ValueAndShrink[T1] is (T1^, Iterator[T1^])
  """
  Possible return type for
  [`Generator.generate`](pony_check-Generator.md#generate).
  Represents a generated value and an Iterator of shrunken values.
  """

type GenerateResult[T2] is (T2^ | ValueAndShrink[T2])
  """
  Return type for
  [`Generator.generate`](pony_check-Generator.md#generate).

  Either a single value or a Tuple of a value and an Iterator
  of shrunken values based upon this value.
  """

class CountdownIter[T: (Int & Integer[T] val) = USize] is Iterator[T]
  var _cur: T
  let _to: T

  new create(from: T, to: T = T.min_value()) =>
    """
    Create am `Iterator` that counts down according to the specified arguments.

    `from` is exclusive, `to` is inclusive.
    """
    _cur = from
    _to = to

  fun ref has_next(): Bool =>
    _cur > _to

  fun ref next(): T =>
    let res = _cur - 1
    _cur = res
    res

trait box GenObj[T]
  fun generate(rnd: Randomness): GenerateResult[T] ?

  fun shrink(t: T): ValueAndShrink[T] =>
    (consume t, Poperator[T].empty())

  fun generate_value(rnd: Randomness): T^ ? =>
    """
    Simply generate a value and ignore any possible
    shrink values.
    """
    let g = this
    match \exhaustive\ g.generate(rnd)?
    | let t: T => consume t
    | (let t: T, _) => consume t
    end

  fun generate_and_shrink(rnd: Randomness): ValueAndShrink[T] ? =>
    """
    Generate a value and also return a shrink result,
    even if the generator does not return any when calling `generate`.
    """
    let g = this
    match \exhaustive\ g.generate(rnd)?
    | let t: T => g.shrink(consume t)
    | (let t: T, let shrinks: Iterator[T^])=> (consume t, shrinks)
    end

  fun iter(rnd: Randomness): Iterator[GenerateResult[T]]^ =>
    let that: GenObj[T] = this

    object is Iterator[GenerateResult[T]]
      fun ref has_next(): Bool => true
      fun ref next(): GenerateResult[T] ? => that.generate(rnd)?
    end

  fun value_iter(rnd: Randomness): Iterator[T^] =>
    let that: GenObj[T] = this

    object is Iterator[T^]
      fun ref has_next(): Bool => true
      fun ref next(): T^ ? =>
        match \exhaustive\ that.generate(rnd)?
        | let value_only: T => consume value_only
        | (let v: T, _) => consume v
        end
    end

  fun value_and_shrink_iter(rnd: Randomness): Iterator[ValueAndShrink[T]] =>
    let that: GenObj[T] = this

    object is Iterator[ValueAndShrink[T]]
      fun ref has_next(): Bool => true
      fun ref next(): ValueAndShrink[T] ? =>
        match \exhaustive\ that.generate(rnd)?
        | let value_only: T => that.shrink(consume value_only)
        | (let v: T, let shrinks: Iterator[T^]) => (consume v, consume shrinks)
        end
    end


class box Generator[T] is GenObj[T]
  """
  A Generator is capable of generating random values of a certain type `T`
  given a source of `Randomness`
  and knows how to shrink or simplify values of that type.

  When testing a property against one or more given Generators,
  those generators' `generate` methods are being called many times
  to generate sample values that are then used to validate the property.

  When a failing sample is found, the PonyCheck engine is trying to find a
  smaller or more simple sample by shrinking it with `shrink`.
  If the generator did not provide any shrinked samples
  as a result of `generate`, its `shrink` method is called
  to obtain simpler results. PonyCheck obtains more shrunken samples until
  the property is not failing anymore.
  The last failing sample, which is considered the most simple one,
  is then reported to the user.
  """
  let _gen: GenObj[T]

  new create(gen: GenObj[T]) =>
    _gen = gen

  fun generate(rnd: Randomness): GenerateResult[T] ? =>
    """
    Let this generator generate a value
    given a source of `Randomness`.

    Also allow for returning a value and pre-generated shrink results
    as a `ValueAndShrink[T]` instance, a tuple of `(T^, Seq[T])`.
    This helps propagating shrink results through all kinds of Generator
    combinators like `filter`, `map` and `flat_map`.

    If implementing a custom `Generator` based on another one,
    with a Generator Combinator, you should use shrunken values
    returned by `generate` to also return shrunken values based on them.

    If generating an example value is costly, it might be more efficient
    to simply return the generated value and only shrink in big steps or do no
    shrinking at all.
    If generating values is lightweight, shrunken values should also be
    returned.
    """
    _gen.generate(rnd)?

  fun shrink(t: T): ValueAndShrink[T] =>
    """
    Simplify the given value.

    As the returned value can also be `iso`, it needs to be consumed and
    returned.

    It is preferred to already return a `ValueAndShrink` from `generate`.
    """
    _gen.shrink(consume t)

  fun generate_value(rnd: Randomness): T^ ? =>
    _gen.generate_value(rnd)?

  fun generate_and_shrink(rnd: Randomness): ValueAndShrink[T] ? =>
    _gen.generate_and_shrink(rnd)?

  fun filter(predicate: {(T): (T^, Bool)} box): Generator[T] =>
    """
    Apply `predicate` to the values generated by this Generator
    and only yields values for which `predicate` returns `true`.

    Example:

    ```pony
    let even_i32s =
      Generators.i32()
        .filter(
          {(t) => (t, ((t % 2) == 0)) })
    ```
    """
    Generator[T](
      object is GenObj[T]
        fun generate(rnd: Randomness): GenerateResult[T] ? =>
          (let t: T, let shrunken: Iterator[T^]) = _gen.generate_and_shrink(rnd)?
          (let t1, let matches) = predicate(consume t)
          if not matches then
            generate(rnd)? // recurse, this might recurse infinitely
          else
            // filter the shrunken examples
            (consume t1, _filter_shrunken(shrunken))
          end

        fun shrink(t: T): ValueAndShrink[T] =>
          """
          shrink `t` using the generator this one filters upon
          and call the filter predicate on the shrunken values
          """
          (let s, let shrunken: Iterator[T^]) = _gen.shrink(consume t)
          (consume s, _filter_shrunken(shrunken))

        fun _filter_shrunken(shrunken: Iterator[T^]): Iterator[T^] =>
          Iter[T^](shrunken)
            .filter_map[T^]({
              (t: T): (T^| None) =>
                match predicate(consume t)
                | (let matching: T, true) => consume matching
                end
            })
      end)

  fun map[U](fn: {(T): U^} box)
    : Generator[U]
  =>
    """
    Apply `fn` to each value of this iterator
    and yield the results.

    Example:

    ```pony
    let single_code_point_string_gen =
      Generators.u32()
        .map[String]({(u) => String.from_utf32(u) })
    ```
    """
    Generator[U](
      object is GenObj[U]
        fun generate(rnd: Randomness): GenerateResult[U] ? =>
          (let generated: T, let shrunken: Iterator[T^]) =
            _gen.generate_and_shrink(rnd)?

          (fn(consume generated), _map_shrunken(shrunken))

        fun shrink(u: U): ValueAndShrink[U] =>
          """
          We can only shrink if T is a subtype of U.

          This method should in general not be called on this generator
          as it is always returning shrinks with the call to `generate`
          and they should be used for executing the shrink, but in case
          a strange hierarchy of generators is used, which does not make use of
          the pre-generated shrink results, we keep this method here.
          """
          match u
          | let ut: T =>
            (let uts: T, let shrunken: Iterator[T^]) = _gen.shrink(consume ut)
            (fn(consume uts), _map_shrunken(shrunken))
          else
            (consume u, Poperator[U].empty())
          end

        fun _map_shrunken(shrunken: Iterator[T^]): Iterator[U^] =>
          Iter[T^](shrunken)
            .map[U^]({(t) => fn(consume t) })

      end)

  fun flat_map[U](fn: {(T): Generator[U]} box): Generator[U] =>
    """
    For each value of this generator, create a generator that is then combined.
    """
    // TODO: enable proper shrinking:
    Generator[U](
      object is GenObj[U]
        fun generate(rnd: Randomness): GenerateResult[U] ? =>
          let value: T = _gen.generate_value(rnd)?
          fn(consume value).generate_and_shrink(rnd)?

      end)

  fun union[U](other: Generator[U]): Generator[(T | U)] =>
    """
    Create a generator that produces the value of this generator or the other
    with the same probability, returning a union type of this generator and
    the other one.
    """
    Generator[(T | U)](
      object is GenObj[(T | U)]
        fun generate(rnd: Randomness): GenerateResult[(T | U)] ? =>
          if rnd.bool() then
            _gen.generate_and_shrink(rnd)?
          else
            other.generate_and_shrink(rnd)?
          end

        fun shrink(t: (T | U)): ValueAndShrink[(T | U)] =>
          match \exhaustive\ consume t
          | let tt: T => _gen.shrink(consume tt)
          | let tu: U => other.shrink(consume tu)
          end
      end
    )

type WeightedGenerator[T] is (USize, Generator[T] box)
  """
  A generator with an associated weight, used in Generators.frequency.
  """

primitive Generators
  """
  Convenience combinators and factories for common types and kind of Generators.
  """

  fun unit[T](t: T, do_shrink: Bool = false): Generator[box->T] =>
    """
    Generate a reference to the same value over and over again.

    This reference will be of type `box->T` and not just `T`
    as this generator will need to keep a reference to the given value.
    """
    Generator[box->T](
      object is GenObj[box->T]
        let _t: T = consume t
        fun generate(rnd: Randomness): GenerateResult[box->T] =>
          if do_shrink then
            (_t, Iter[box->T].repeat_value(_t))
          else
            _t
          end
      end)

  fun none[T: None](): Generator[(T | None)] => Generators.unit[(T | None)](None)

  fun repeatedly[T](f: {(): T^ ?} box): Generator[T] =>
    """
    Generate values by calling the lambda `f` repeatedly,
    once for every invocation of `generate`.

    `f` needs to return an ephemeral type `T^`, that means
    in most cases it needs to consume its returned value.
    Otherwise we would end up with
    an alias for `T` which is `T!`.
    (e.g. `String iso` would be returned as `String iso!`,
    which aliases as a `String tag`).

    Example:

    ```pony
    Generators.repeatedly[Writer]({(): Writer^ =>
      let writer = Writer.>write("consume me, please")
      consume writer
    })
    ```
    """
    Generator[T](
      object is GenObj[T]
        fun generate(rnd: Randomness): GenerateResult[T] ? =>
          f()?
      end)


  fun seq_of[T, S: Seq[T] ref](
    gen: Generator[T],
    min: USize = 0,
    max: USize = 100)
    : Generator[S]
  =>
    """
    Create a `Seq` from the values of the given Generator with an optional
    minimum and maximum size.
    
    Defaults are 0 and 100, respectively.
    """

    Generator[S](
      object is GenObj[S]
        let _gen: GenObj[T] = gen
        fun generate(rnd: Randomness): GenerateResult[S] =>
          let size = rnd.usize(min, max)

          let result: S =
            Iter[T^](_gen.value_iter(rnd))
              .take(size)
              .collect[S](S.create(size))

          // create shrink_iter with smaller seqs and elements generated from _gen.value_iter
          let shrink_iter =
            Iter[USize](CountdownIter(size, min)) //Range(size, min, -1))
              // .skip(1)
              .map_stateful[S^]({
                (s: USize): S^ =>
                  Iter[T^](_gen.value_iter(rnd))
                    .take(s)
                    .collect[S](S.create(s))
              })
          (consume result, shrink_iter)
      end)

  fun iso_seq_of[T: Any #send, S: Seq[T] iso](
    gen: Generator[T],
    min: USize = 0,
    max: USize = 100)
    : Generator[S]
  =>
    """
    Generate a `Seq[T]` where `T` must be sendable (i.e. it must have a
    reference capability of either `tag`, `val`, or `iso`).

    The constraint of the elements being sendable stems from the fact that
    there is no other way to populate the iso seq if the elements might be
    non-sendable (i.e. ref), as then the seq would leak references via
    its elements.
    """
    Generator[S](
      object is GenObj[S]
        let _gen: GenObj[T] = gen
        fun generate(rnd: Randomness): GenerateResult[S] =>
          let size = rnd.usize(min, max)

          let result: S = recover iso S.create(size) end
          let iter = _gen.value_iter(rnd)
          var i = USize(0)

          for elem in iter do
            if i >= size then break end

            result.push(consume elem)
            i = i + 1
          end
          // create shrink_iter with smaller seqs and elements generated from _gen.value_iter
          let shrink_iter =
            Iter[USize](CountdownIter(size, min)) //Range(size, min, -1))
              // .skip(1)
              .map_stateful[S^]({
                (s: USize): S^ =>
                  let res = recover iso S.create(s) end
                  let s_iter = _gen.value_iter(rnd)
                  var j = USize(0)

                  for s_elem in s_iter do
                    if j >= s then break end
                    res.push(consume s_elem)
                    j = j + 1
                  end
                  consume res
              })
          (consume result, shrink_iter)
      end
    )

  fun array_of[T](
    gen: Generator[T],
    min: USize = 0,
    max: USize = 100)
    : Generator[Array[T]]
  =>
    Generators.seq_of[T, Array[T]](gen, min, max)

  fun shuffled_array_gen[T](
    gen: Generator[Array[T]])
    : Generator[Array[T]]
  =>
    Generator[Array[T]](
      object is GenObj[Array[T]]
        let _gen: GenObj[Array[T]] = gen
        fun generate(rnd: Randomness): GenerateResult[Array[T]] ? =>
          (let arr, let source_shrink_iter) = _gen.generate_and_shrink(rnd)?
            rnd.shuffle[T](arr)
            let shrink_iter =
              Iter[Array[T]](source_shrink_iter)
                .map_stateful[Array[T]^]({
                  (shrink_arr: Array[T]): Array[T]^ =>
                      rnd.shuffle[T](shrink_arr)
                      consume shrink_arr
                })
            (consume arr, shrink_iter)
      end
    )

  fun shuffled_iter[T](array: Array[T]): Generator[Iterator[this->T!]] =>
    Generator[Iterator[this->T!]](
      object is GenObj[Iterator[this->T!]]
        fun generate(rnd: Randomness): GenerateResult[Iterator[this->T!]] =>
          let cloned = array.clone()
          rnd.shuffle[this->T!](cloned)
          cloned.values()
      end
    )

  fun list_of[T](
    gen: Generator[T],
    min: USize = 0,
    max: USize = 100)
    : Generator[List[T]]
  =>
    Generators.seq_of[T, List[T]](gen, min, max)

  fun set_of[T: (Hashable #read & Equatable[T] #read)](
    gen: Generator[T],
    max: USize = 100)
    : Generator[Set[T]]
  =>
    """
    Create a generator for `Set` filled with values
    of the given generator `gen`.
    The returned sets will have a size up to `max`,
    but tend to have fewer than `max`
    depending on the source generator `gen`.

    E.g. if the given generator is for `U8` values and `max` is set to 1024,
    the set will only ever be of size 256 max.

    Also for efficiency purposes and to not loop forever,
    this generator will only try to add at most `max` values to the set.
    If there are duplicates, the set won't grow.
    """
    Generator[Set[T]](
      object is GenObj[Set[T]]
        let _gen: GenObj[T] = gen
        fun generate(rnd: Randomness): GenerateResult[Set[T]] =>
          let size = rnd.usize(0, max)
          let result: Set[T] =
            Set[T].create(size).>union(
              Iter[T^](_gen.value_iter(rnd))
              .take(size)
            )
          let shrink_iter: Iterator[Set[T]^] =
            Iter[USize](CountdownIter(size, 0)) // Range(size, 0, -1))
              //.skip(1)
              .map_stateful[Set[T]^]({
                (s: USize): Set[T]^ =>
                  Set[T].create(s).>union(
                    Iter[T^](_gen.value_iter(rnd)).take(s)
                  )
                })
          (consume result, shrink_iter)
      end)

  fun set_is_of[T](
    gen: Generator[T],
    max: USize = 100)
    : Generator[SetIs[T]]
  =>
    """
    Create a generator for `SetIs` filled with values
    of the given generator `gen`.
    The returned `SetIs` will have a size up to `max`,
    but tend to have fewer entries
    depending on the source generator `gen`.

    E.g. if the given generator is for `U8` values and `max` is set to 1024
    the set will only ever be of size 256 max.

    Also for efficiency purposes and to not loop forever,
    this generator will only try to add at most `max` values to the set.
    If there are duplicates, the set won't grow.
    """
    // TODO: how to remove code duplications
    Generator[SetIs[T]](
      object is GenObj[SetIs[T]]
        fun generate(rnd: Randomness): GenerateResult[SetIs[T]] =>
          let size = rnd.usize(0, max)

          let result: SetIs[T] =
            SetIs[T].create(size).>union(
              Iter[T^](gen.value_iter(rnd))
                .take(size)
            )
          let shrink_iter: Iterator[SetIs[T]^] =
            Iter[USize](CountdownIter(size, 0)) //Range(size, 0, -1))
              //.skip(1)
              .map_stateful[SetIs[T]^]({
                (s: USize): SetIs[T]^ =>
                  SetIs[T].create(s).>union(
                    Iter[T^](gen.value_iter(rnd)).take(s)
                  )
                })
          (consume result, shrink_iter)
      end)

  fun map_of[K: (Hashable #read & Equatable[K] #read), V](
    gen: Generator[(K, V)],
    max: USize = 100)
    : Generator[Map[K, V]]
  =>
    """
    Create a generator for `Map` from a generator of key-value tuples.
    The generated maps will have a size up to `max`,
    but tend to have fewer entries depending on the source generator `gen`.

    If the generator generates key-value pairs with
    duplicate keys (based on structural equality),
    the pair that is generated later will overwrite earlier entries in the map.
    """
    Generator[Map[K, V]](
      object is GenObj[Map[K, V]]
        fun generate(rnd: Randomness): GenerateResult[Map[K, V]] =>
          let size = rnd.usize(0, max)

          let result: Map[K, V] =
            Map[K, V].create(size).>concat(
              Iter[(K^, V^)](gen.value_iter(rnd))
                .take(size)
            )
          let shrink_iter: Iterator[Map[K, V]^] =
            Iter[USize](CountdownIter(size, 0)) // Range(size, 0, -1))
              // .skip(1)
              .map_stateful[Map[K, V]^]({
                (s: USize): Map[K, V]^ =>
                  Map[K, V].create(s).>concat(
                    Iter[(K^, V^)](gen.value_iter(rnd)).take(s)
                  )
                })
          (consume result, shrink_iter)

      end)

  fun map_is_of[K, V](
    gen: Generator[(K, V)],
    max: USize = 100)
    : Generator[MapIs[K, V]]
  =>
    """
    Create a generator for `MapIs` from a generator of key-value tuples.
    The generated maps will have a size up to `max`,
    but tend to have fewer entries depending on the source generator `gen`.

    If the generator generates key-value pairs with
    duplicate keys (based on identity),
    the pair that is generated later will overwrite earlier entries in the map.
    """
    Generator[MapIs[K, V]](
      object is GenObj[MapIs[K, V]]
        fun generate(rnd: Randomness): GenerateResult[MapIs[K, V]] =>
          let size = rnd.usize(0, max)

          let result: MapIs[K, V] =
            MapIs[K, V].create(size).>concat(
              Iter[(K^, V^)](gen.value_iter(rnd))
                .take(size)
            )
          let shrink_iter: Iterator[MapIs[K, V]^] =
            Iter[USize](CountdownIter(size, 0)) //Range(size, 0, -1))
              // .skip(1)
              .map_stateful[MapIs[K, V]^]({
                (s: USize): MapIs[K, V]^ =>
                  MapIs[K, V].create(s).>concat(
                    Iter[(K^, V^)](gen.value_iter(rnd)).take(s)
                  )
                })
          (consume result, shrink_iter)
      end)


  fun one_of[T](xs: ReadSeq[T], do_shrink: Bool = false): Generator[box->T] =>
    """
    Generate a random value from the given ReadSeq.
    This generator will generate nothing if the given xs is empty.

    Generators created with this method do not support shrinking.
    If `do_shrink` is set to `true`, it will return the same value
    for each shrink round. Otherwise it will return nothing.
    """

    Generator[box->T](
      object is GenObj[box->T]
        fun generate(rnd: Randomness): GenerateResult[box->T] ? =>
          let idx = rnd.usize(0, xs.size() - 1)
          let res = xs(idx)?
          if do_shrink then
            (res, Iter[box->T].repeat_value(res))
          else
            res
          end
      end)

  fun one_of_safe[T](xs: ReadSeq[T], do_shrink: Bool = false): Generator[box->T] ? =>
    """
    Version of `one_of` that will error if `xs` is empty.
    """
    Fact(xs.size() > 0, "cannot use one_of_safe on empty ReadSeq")?
    Generators.one_of[T](xs, do_shrink)

  fun frequency[T](
    weighted_generators: ReadSeq[WeightedGenerator[T]])
    : Generator[T]
  =>
    """
    Choose a value of one of the given Generators,
    while controlling the distribution with the associated weights.

    The weights are of type `USize` and control how likely a value is chosen.
    The likelihood of a value `v` to be chosen
    is `weight_v` / `weights_sum`.
    If all `weighted_generators` have equal size the distribution
    will be uniform.

    Example of a generator to output odd `U8` values
    twice as likely as even ones:

    ```pony
    Generators.frequency[U8]([
      (1, Generators.u8().filter({(u) => (u, (u % 2) == 0 }))
      (2, Generators.u8().filter({(u) => (u, (u % 2) != 0 }))
    ])
    ```
    """

    // nasty hack to avoid handling the theoretical error case where we have
    // no generator and thus would have to change the type signature
    Generator[T](
      object is GenObj[T]
        fun generate(rnd: Randomness): GenerateResult[T] ? =>
          let weight_sum: USize =
            Iter[WeightedGenerator[T]](weighted_generators.values())
              .fold[USize](
                0,
                // segfaults when types are removed - TODO: investigate
                {(acc: USize, weighted_gen: WeightedGenerator[T]): USize^ =>
                  weighted_gen._1 + acc
                })
          let desired_sum = rnd.usize(0, weight_sum)
          var running_sum: USize = 0
          var chosen: (Generator[T] | None) = None
          for weighted_gen in weighted_generators.values() do
            let new_sum = running_sum + weighted_gen._1
            if ((desired_sum == 0) or ((running_sum < desired_sum) and (desired_sum <= new_sum))) then
              // we just crossed or reached the desired sum
              chosen = weighted_gen._2
              break
            else
              // update running sum
              running_sum = new_sum
            end
          end
          match \exhaustive\ chosen
          | let x: Generator[T] box => x.generate(rnd)?
          | None =>
            Debug("chosen is None, desired_sum: " + desired_sum.string() +
              "running_sum: " + running_sum.string())
            error
          end
      end)

  fun frequency_safe[T](
    weighted_generators: ReadSeq[WeightedGenerator[T]])
    : Generator[T] ?
  =>
    """
    Version of `frequency` that errors if the given `weighted_generators` is
    empty.
    """
    Fact(weighted_generators.size() > 0,
      "cannot use frequency_safe on empty ReadSeq[WeightedGenerator]")?
    Generators.frequency[T](weighted_generators)

  fun zip2[T1, T2](
    gen1: Generator[T1],
    gen2: Generator[T2])
    : Generator[(T1, T2)]
  =>
    """
    Zip two generators into a generator of a 2-tuple
    containing the values generated by both generators.
    """
    Generator[(T1, T2)](
      object is GenObj[(T1, T2)]
        fun generate(rnd: Randomness): GenerateResult[(T1, T2)] ? =>
          (let v1: T1, let shrinks1: Iterator[T1^]) =
            gen1.generate_and_shrink(rnd)?
          (let v2: T2, let shrinks2: Iterator[T2^]) =
            gen2.generate_and_shrink(rnd)?
          ((consume v1, consume v2), Iter[T1^](shrinks1).zip[T2^](shrinks2))

        fun shrink(t: (T1, T2)): ValueAndShrink[(T1, T2)] =>
          (let t1, let t2) = consume t
          (let t11, let t1_shrunken: Iterator[T1^]) = gen1.shrink(consume t1)
          (let t21, let t2_shrunken: Iterator[T2^]) = gen2.shrink(consume t2)

          let shrunken = Iter[T1^](t1_shrunken).zip[T2^](t2_shrunken)
          ((consume t11, consume t21), shrunken)
      end)

  fun zip3[T1, T2, T3](
    gen1: Generator[T1],
    gen2: Generator[T2],
    gen3: Generator[T3])
    : Generator[(T1, T2, T3)]
  =>
    """
    Zip three generators into a generator of a 3-tuple
    containing the values generated by those three generators.
    """
    Generator[(T1, T2, T3)](
      object is GenObj[(T1, T2, T3)]
        fun generate(rnd: Randomness): GenerateResult[(T1, T2, T3)] ? =>
          (let v1: T1, let shrinks1: Iterator[T1^]) =
            gen1.generate_and_shrink(rnd)?
          (let v2: T2, let shrinks2: Iterator[T2^]) =
            gen2.generate_and_shrink(rnd)?
          (let v3: T3, let shrinks3: Iterator[T3^]) =
            gen3.generate_and_shrink(rnd)?
          ((consume v1, consume v2, consume v3),
              Iter[T1^](shrinks1).zip2[T2^, T3^](shrinks2, shrinks3))

        fun shrink(t: (T1, T2, T3)): ValueAndShrink[(T1, T2, T3)] =>
          (let t1, let t2, let t3) = consume t
          (let t11, let t1_shrunken: Iterator[T1^]) = gen1.shrink(consume t1)
          (let t21, let t2_shrunken: Iterator[T2^]) = gen2.shrink(consume t2)
          (let t31, let t3_shrunken: Iterator[T3^]) = gen3.shrink(consume t3)

          let shrunken = Iter[T1^](t1_shrunken).zip2[T2^, T3^](t2_shrunken, t3_shrunken)
          ((consume t11, consume t21, consume t31), shrunken)
        end)

  fun zip4[T1, T2, T3, T4](
    gen1: Generator[T1],
    gen2: Generator[T2],
    gen3: Generator[T3],
    gen4: Generator[T4])
    : Generator[(T1, T2, T3, T4)]
  =>
    """
    Zip four generators into a generator of a 4-tuple
    containing the values generated by those four generators.
    """
    Generator[(T1, T2, T3, T4)](
      object is GenObj[(T1, T2, T3, T4)]
        fun generate(rnd: Randomness): GenerateResult[(T1, T2, T3, T4)] ? =>
          (let v1: T1, let shrinks1: Iterator[T1^]) =
            gen1.generate_and_shrink(rnd)?
          (let v2: T2, let shrinks2: Iterator[T2^]) =
            gen2.generate_and_shrink(rnd)?
          (let v3: T3, let shrinks3: Iterator[T3^]) =
            gen3.generate_and_shrink(rnd)?
          (let v4: T4, let shrinks4: Iterator[T4^]) =
            gen4.generate_and_shrink(rnd)?
          ((consume v1, consume v2, consume v3, consume v4),
              Iter[T1^](shrinks1).zip3[T2^, T3^, T4^](shrinks2, shrinks3, shrinks4))

        fun shrink(t: (T1, T2, T3, T4)): ValueAndShrink[(T1, T2, T3, T4)] =>
          (let t1, let t2, let t3, let t4) = consume t
          (let t11, let t1_shrunken) = gen1.shrink(consume t1)
          (let t21, let t2_shrunken) = gen2.shrink(consume t2)
          (let t31, let t3_shrunken) = gen3.shrink(consume t3)
          (let t41, let t4_shrunken) = gen4.shrink(consume t4)

          let shrunken = Iter[T1^](t1_shrunken)
            .zip3[T2^, T3^, T4^](t2_shrunken, t3_shrunken, t4_shrunken)
          ((consume t11, consume t21, consume t31, consume t41), shrunken)
        end)

  fun map2[T1, T2, T3](
    gen1: Generator[T1],
    gen2: Generator[T2],
    fn: {(T1, T2): T3^})
    : Generator[T3]
  =>
    """
    Convenience combinator for mapping 2 generators into 1.
    """
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
    """
    Convenience combinator for mapping 3 generators into 1.
    """
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
    """
    Convenience combinator for mapping 4 generators into 1.
    """
    Generators.zip4[T1, T2, T3, T4](gen1, gen2, gen3, gen4)
      .map[T5]({(arg) =>
        (let arg1, let arg2, let arg3, let arg4) = consume arg
        fn(consume arg1, consume arg2, consume arg3, consume arg4)
      })

  fun bool(): Generator[Bool] =>
    """
    Create a generator of bool values.
    """
    Generator[Bool](
      object is GenObj[Bool]
        fun generate(rnd: Randomness): Bool =>
          rnd.bool()
        end)

  fun _int_shrink[T: (Int & Integer[T] val)](t: T^, min: T): ValueAndShrink[T] =>
    """
    """
    let relation = t.compare(min)
    let t_copy: T = T.create(t)
    //Debug(t.string() + " is " + relation.string() + " than min " + min.string())
    let sub_iter =
      object is Iterator[T^]
        var _cur: T = t_copy
        var _subtract: F64 = 1.0
        var _overflow: Bool = false

        fun ref _next_minuend(): T =>
          // f(x) = x + (2^-5 * x^2)
          T.from[F64](_subtract = _subtract + (0.03125 * _subtract * _subtract))

        fun ref has_next(): Bool =>
          match \exhaustive\ relation
          | Less => (_cur < min) and not _overflow
          | Equal => false
          | Greater => (_cur > min) and not _overflow
          end

        fun ref next(): T^ ? =>
          match \exhaustive\ relation
          | Less =>
            let minuend: T = _next_minuend()
            let old = _cur
            _cur = _cur + minuend
            if old > _cur then
              _overflow = true
            end
            old
          | Equal => error
          | Greater =>
            let minuend: T = _next_minuend()
            let old = _cur
            _cur = _cur - minuend
            if old < _cur then
              _overflow = true
            end
            old
          end
      end

    let min_iter =
      match \exhaustive\ relation
      | let _: (Less | Greater) => Poperator[T]([min])
      | Equal => Poperator[T].empty()
      end

    let shrunken_iter = Iter[T].chain(
      [
        Iter[T^](sub_iter).skip(1)
        min_iter
      ].values())
    (consume t, shrunken_iter)

  fun u8(
    min: U8 = U8.min_value(),
    max: U8 = U8.max_value())
    : Generator[U8]
  =>
    """
    Create a generator for U8 values.
    """
    let that = this
    Generator[U8](
      object is GenObj[U8]
        fun generate(rnd: Randomness): U8^ =>
          rnd.u8(min, max)

        fun shrink(u: U8): ValueAndShrink[U8] =>
          that._int_shrink[U8](consume u, min)
        end)

  fun u16(
    min: U16 = U16.min_value(),
    max: U16 = U16.max_value())
    : Generator[U16]
  =>
    """
    create a generator for U16 values
    """
    let that = this
    Generator[U16](
      object is GenObj[U16]
        fun generate(rnd: Randomness): U16^ =>
          rnd.u16(min, max)

        fun shrink(u: U16): ValueAndShrink[U16] =>
          that._int_shrink[U16](consume u, min)
      end)

  fun u32(
    min: U32 = U32.min_value(),
    max: U32 = U32.max_value())
    : Generator[U32]
  =>
    """
    Create a generator for U32 values.
    """
    let that = this
    Generator[U32](
      object is GenObj[U32]
        fun generate(rnd: Randomness): U32^ =>
          rnd.u32(min, max)

        fun shrink(u: U32): ValueAndShrink[U32] =>
          that._int_shrink[U32](consume u, min)
      end)

  fun u64(
    min: U64 = U64.min_value(),
    max: U64 = U64.max_value())
    : Generator[U64]
  =>
    """
    Create a generator for U64 values.
    """
    let that = this
    Generator[U64](
      object is GenObj[U64]
        fun generate(rnd: Randomness): U64^ =>
          rnd.u64(min, max)

        fun shrink(u: U64): ValueAndShrink[U64] =>
          that._int_shrink[U64](consume u, min)
      end)

  fun u128(
    min: U128 = U128.min_value(),
    max: U128 = U128.max_value())
    : Generator[U128]
  =>
    """
    Create a generator for U128 values.
    """
    let that = this
    Generator[U128](
      object is GenObj[U128]
        fun generate(rnd: Randomness): U128^ =>
          rnd.u128(min, max)

        fun shrink(u: U128): ValueAndShrink[U128] =>
          that._int_shrink[U128](consume u, min)
      end)

  fun usize(
    min: USize = USize.min_value(),
    max: USize = USize.max_value())
    : Generator[USize]
  =>
    """
    Create a generator for USize values.
    """
    let that = this
    Generator[USize](
      object is GenObj[USize]
        fun generate(rnd: Randomness): GenerateResult[USize] =>
          rnd.usize(min, max)

        fun shrink(u: USize): ValueAndShrink[USize] =>
          that._int_shrink[USize](consume u, min)
      end)

  fun ulong(
    min: ULong = ULong.min_value(),
    max: ULong = ULong.max_value())
    : Generator[ULong]
  =>
    """
    Create a generator for ULong values.
    """
    let that = this
    Generator[ULong](
      object is GenObj[ULong]
        fun generate(rnd: Randomness): ULong^ =>
          rnd.ulong(min, max)

        fun shrink(u: ULong): ValueAndShrink[ULong] =>
          that._int_shrink[ULong](consume u, min)
      end)

  fun i8(
    min: I8 = I8.min_value(),
    max: I8 = I8.max_value())
    : Generator[I8]
  =>
    """
    Create a generator for I8 values.
    """
    let that = this
    Generator[I8](
      object is GenObj[I8]
        fun generate(rnd: Randomness): I8^ =>
          rnd.i8(min, max)

        fun shrink(i: I8): ValueAndShrink[I8] =>
          that._int_shrink[I8](consume i, min)
      end)

  fun i16(
    min: I16 = I16.min_value(),
    max: I16 = I16.max_value())
    : Generator[I16]
  =>
    """
    Create a generator for I16 values.
    """
    let that = this
    Generator[I16](
      object is GenObj[I16]
        fun generate(rnd: Randomness): I16^ =>
          rnd.i16(min, max)

        fun shrink(i: I16): ValueAndShrink[I16] =>
          that._int_shrink[I16](consume i, min)
      end)

  fun i32(
    min: I32 = I32.min_value(),
    max: I32 = I32.max_value())
    : Generator[I32]
  =>
    """
    Create a generator for I32 values.
    """
    let that = this
    Generator[I32](
      object is GenObj[I32]
        fun generate(rnd: Randomness): I32^ =>
          rnd.i32(min, max)

        fun shrink(i: I32): ValueAndShrink[I32] =>
          that._int_shrink[I32](consume i, min)
      end)

  fun i64(
    min: I64 = I64.min_value(),
    max: I64 = I64.max_value())
    : Generator[I64]
  =>
    """
    Create a generator for I64 values.
    """
    let that = this
    Generator[I64](
      object is GenObj[I64]
        fun generate(rnd: Randomness): I64^ =>
          rnd.i64(min, max)

        fun shrink(i: I64): ValueAndShrink[I64] =>
          that._int_shrink[I64](consume i, min)
      end)

  fun i128(
    min: I128 = I128.min_value(),
    max: I128 = I128.max_value())
    : Generator[I128]
  =>
    """
    Create a generator for I128 values.
    """
    let that = this
    Generator[I128](
      object is GenObj[I128]
        fun generate(rnd: Randomness): I128^ =>
          rnd.i128(min, max)

        fun shrink(i: I128): ValueAndShrink[I128] =>
          that._int_shrink[I128](consume i, min)
      end)

  fun ilong(
    min: ILong = ILong.min_value(),
    max: ILong = ILong.max_value())
    : Generator[ILong]
    =>
    """
    Create a generator for ILong values.
    """
    let that = this
    Generator[ILong](
      object is GenObj[ILong]
        fun generate(rnd: Randomness): ILong^ =>
          rnd.ilong(min, max)

        fun shrink(i: ILong): ValueAndShrink[ILong] =>
          that._int_shrink[ILong](consume i, min)
      end)

  fun isize(
    min: ISize = ISize.min_value(),
    max: ISize = ISize.max_value())
    : Generator[ISize]
  =>
    """
    Create a generator for ISize values.
    """
    let that = this
    Generator[ISize](
      object is GenObj[ISize]
        fun generate(rnd: Randomness): ISize^ =>
          rnd.isize(min, max)

        fun shrink(i: ISize): ValueAndShrink[ISize] =>
          that._int_shrink[ISize](consume i, min)
      end)


  fun byte_string(
    gen: Generator[U8],
    min: USize = 0,
    max: USize = 100)
    : Generator[String]
  =>
    """
    Create a generator for strings
    generated from the bytes returned by the generator `gen`,
    with a minimum length of `min` (default: 0)
    and a maximum length of `max` (default: 100).
    """
    Generator[String](
      object is GenObj[String]
        fun generate(rnd: Randomness): GenerateResult[String] =>
          let size = rnd.usize(min, max)
          let gen_iter = Iter[U8^](gen.value_iter(rnd))
            .take(size)
          let arr: Array[U8] iso = recover Array[U8](size) end
          for b in gen_iter do
            arr.push(b)
          end
          String.from_iso_array(consume arr)

        fun shrink(s: String): ValueAndShrink[String] =>
          """
          shrink string until `min` length.
          """
          var str: String = s.trim(0, s.size()-1)
          let shorten_iter: Iterator[String^] =
            object is Iterator[String^]
              fun ref has_next(): Bool => str.size() > min
              fun ref next(): String^ =>
                str = str.trim(0, str.size()-1)
            end
          let min_iter =
            if s.size() > min then
              Poperator[String]([s.trim(0, min)])
            else
              Poperator[String].empty()
            end
          let shrink_iter =
            Iter[String^].chain([
              shorten_iter
              min_iter
            ].values())
          (consume s, shrink_iter)
      end)

  fun ascii(
    min: USize = 0,
    max: USize = 100,
    range: ASCIIRange = ASCIIAll)
    : Generator[String]
  =>
    """
    Create a generator for strings withing the given `range`,
    with a minimum length of `min` (default: 0)
    and a maximum length of `max` (default: 100).
    """
    let range_bytes = range.apply()
    let fallback = U8(0)
    let range_bytes_gen = usize(0, range_bytes.size()-1)
      .map[U8]({(size) =>
        try
          range_bytes(size)?
        else
          // should never happen
          fallback
        end })
    byte_string(range_bytes_gen, min, max)

  fun ascii_printable(
    min: USize = 0,
    max: USize = 100)
    : Generator[String]
  =>
    """
    Create a generator for strings of printable ASCII characters,
    with a minimum length of `min` (default: 0)
    and a maximum length of `max` (default: 100).
    """
    ascii(min, max, ASCIIPrintable)

  fun ascii_numeric(
    min: USize = 0,
    max: USize = 100)
    : Generator[String]
  =>
    """
    Create a generator for strings of numeric ASCII characters,
    with a minimum length of `min` (default: 0)
    and a maximum length of `max` (default: 100).
    """
    ascii(min, max, ASCIIDigits)

  fun ascii_letters(
    min: USize = 0,
    max: USize = 100)
    : Generator[String]
  =>
    """
    Create a generator for strings of ASCII letters,
    with a minimum length of `min` (default: 0)
    and a maximum length of `max` (default: 100).
    """
    ascii(min, max, ASCIILetters)

  fun utf32_codepoint_string(
    gen: Generator[U32],
    min: USize = 0,
    max: USize = 100)
    : Generator[String]
  =>
    """
    Create a generator for strings
    from a generator of unicode codepoints,
    with a minimum length of `min` codepoints (default: 0)
    and a maximum length of `max` codepoints (default: 100).

    Note that the byte length of the generated string can be up to 4 times
    the size in code points.
    """
    Generator[String](
      object is GenObj[String]
        fun generate(rnd: Randomness): GenerateResult[String] =>
          let size = rnd.usize(min, max)
          let gen_iter = Iter[U32^](gen.value_iter(rnd))
            .filter({(cp) =>
              // excluding surrogate pairs
              (cp <= 0xD7FF) or (cp >= 0xE000) })
            .take(size)
          let s: String iso = recover String(size) end
          for code_point in gen_iter do
            s.push_utf32(code_point)
          end
          s

        fun shrink(s: String): ValueAndShrink[String] =>
          """
          Strip off codepoints from the end, not just bytes, so we
          maintain a valid utf8 string.

          Only shrink until given `min` is hit.
          """
          var shrink_base = s
          let s_len = s.codepoints()
          let shrink_iter: Iterator[String^] =
            if s_len > min then
              Iter[String^].repeat_value(consume shrink_base)
                .map_stateful[String^](
                  object
                    var len: USize = s_len - 1
                    fun ref apply(str: String): String =>
                      Generators._trim_codepoints(str, len = len - 1)
                  end
                ).take(s_len - min)
                // take_while is buggy in pony < 0.21.0
                //.take_while({(t) => t.codepoints() > min})
            else
              Poperator[String].empty()
            end
          (consume s, shrink_iter)
      end)

  fun _trim_codepoints(s: String, trim_to: USize): String =>
    recover val
      Iter[U32](s.runes())
        .take(trim_to)
        .fold[String ref](
          String.create(trim_to),
          {(acc, cp) => acc.>push_utf32(cp) })
    end

  fun unicode(
    min: USize = 0,
    max: USize = 100)
    : Generator[String]
  =>
    """
    Create a generator for unicode strings,
    with a minimum length of `min` codepoints (default: 0)
    and a maximum length of `max` codepoints (default: 100).

    Note that the byte length of the generated string can be up to 4 times
    the size in code points.
    """
    let range_1 = u32(0x0, 0xD7FF)
    let range_1_size: USize = 0xD7FF
    // excluding surrogate pairs
    // this might be duplicate work but increases efficiency
    let range_2 = u32(0xE000, 0x10FFFF)
    let range_2_size = U32(0x10FFFF - 0xE000).usize()

    let code_point_gen =
      frequency[U32]([
        (range_1_size, range_1)
        (range_2_size, range_2)
      ])
    utf32_codepoint_string(code_point_gen, min, max)

  fun unicode_bmp(
    min: USize = 0,
    max: USize = 100)
    : Generator[String]
  =>
    """
    Create a generator for unicode strings
    from the basic multilingual plane only,
    with a minimum length of `min` codepoints (default: 0)
    and a maximum length of `max` codepoints (default: 100).

    Note that the byte length of the generated string can be up to 4 times
    the size in code points.
    """
    let range_1 = u32(0x0, 0xD7FF)
    let range_1_size: USize = 0xD7FF
    // excluding surrogate pairs
    // this might be duplicate work but increases efficiency
    let range_2 = u32(0xE000, 0xFFFF)
    let range_2_size = U32(0xFFFF - 0xE000).usize()

    let code_point_gen =
      frequency[U32]([
        (range_1_size, range_1)
        (range_2_size, range_2)
      ])
    utf32_codepoint_string(code_point_gen, min, max)

