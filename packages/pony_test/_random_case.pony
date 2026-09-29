use "collections"
use persistent = "collections/persistent"
use "files"
use "random"
use "time"

class \nodoc\ iso _StringifyTest is UnitTest
  fun name(): String => "stringify"

  fun apply(h: TestHelper) =>
    (let _, var s) = _Stringify.apply[(U8, U8)]((0, 1))
    h.assert_eq[String](s, "(0, 1)")
    (let _, s) = _Stringify.apply[(U8, U32, U128)]((0, 1, 2))
    h.assert_eq[String](s, "(0, 1, 2)")
    (let _, s) = _Stringify.apply[(U8, (U32, U128))]((0, (1, 2)))
    h.assert_eq[String](s, "(0, (1, 2))")
    (let _, s) = _Stringify.apply[((U8, U32), U128)](((0, 1), 2))
    h.assert_eq[String](s, "((0, 1), 2)")
    let a: Array[U8] = [ U8(0); U8(42) ]
    (let _, s) = _Stringify.apply[Array[U8]](a)
    h.assert_eq[String](s, "[0 42]")

class \nodoc\ iso _SuccessfulProperty is Property[U8]
  fun name(): String => "as_unit_test/successful/property"

  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun ref property(arg1: U8, h: PropertyHelper) =>
    h.assert_true(arg1 <= U8(10))

class \nodoc\ iso _SuccessfulProperty2 is Property2[U8, U8]
  fun name(): String => "as_unit_test/successful2/property"
  fun gen1(): Generator[U8] => Generators.u8(0, 1)
  fun gen2(): Generator[U8] => Generators.u8(2, 3)

  fun ref property2(arg1: U8, arg2: U8, h: PropertyHelper) =>
    h.assert_ne[U8](arg1, arg2)

class \nodoc\ iso _SuccessfulProperty3 is Property3[U8, U8, U8]
  fun name(): String => "as_unit_test/successful3/property"
  fun gen1(): Generator[U8] => Generators.u8(0, 1)
  fun gen2(): Generator[U8] => Generators.u8(2, 3)
  fun gen3(): Generator[U8] => Generators.u8(4, 5)

  fun ref property3(arg1: U8, arg2: U8, arg3: U8, h: PropertyHelper) =>
    h.assert_ne[U8](arg1, arg2)
    h.assert_ne[U8](arg2, arg3)
    h.assert_ne[U8](arg1, arg3)

class \nodoc\ iso _SuccessfulProperty4 is Property4[U8, U8, U8, U8]
  fun name(): String => "as_unit_test/successful4/property"
  fun gen1(): Generator[U8] => Generators.u8(0, 1)
  fun gen2(): Generator[U8] => Generators.u8(2, 3)
  fun gen3(): Generator[U8] => Generators.u8(4, 5)
  fun gen4(): Generator[U8] => Generators.u8(6, 7)

  fun ref property4(
    arg1: U8, arg2: U8, arg3: U8, arg4: U8, h: PropertyHelper)
  =>
    h.assert_ne[U8](arg1, arg2)
    h.assert_ne[U8](arg1, arg3)
    h.assert_ne[U8](arg1, arg4)
    h.assert_ne[U8](arg2, arg3)
    h.assert_ne[U8](arg2, arg4)
    h.assert_ne[U8](arg3, arg4)

class \nodoc\ iso _ErroringProperty is Property[U8]
  fun name(): String => "as_unit_test/erroring/property"

  fun params(): PropertyParams =>
    PropertyParams(where regression_db' = false)

  fun gen(): Generator[U8] => Generators.u8(0, 1)

  fun ref property(arg1: U8, h: PropertyHelper) ? =>
    if arg1 < 2 then
      error
    end

class \nodoc\ iso _ForAllTest is UnitTest
  fun name(): String => "pony_test/for_all"

  fun apply(h: TestHelper) ? =>
    h.for_all[U8](recover Generators.unit[U8](0) end)(
      {(u, h) => h.assert_eq[U8](u, 0, u.string() + " == 0") })?

class \nodoc\ iso _ForAll2Test is UnitTest
  fun name(): String => "pony_test/for_all2"

  fun apply(h: TestHelper) ? =>
    h.for_all2[U8, U8](
      recover Generators.unit[U8](0) end,
      recover Generators.unit[U8](255) end)(
        {(a, b, h) =>
          h.assert_eq[U8](a, 0)
          h.assert_eq[U8](b, 255)
        })?

class \nodoc\ iso _ForAll3Test is UnitTest
  fun name(): String => "pony_test/for_all3"

  fun apply(h: TestHelper) ? =>
    h.for_all3[U8, U8, U8](
      recover Generators.unit[U8](0) end,
      recover Generators.unit[U8](128) end,
      recover Generators.unit[U8](255) end)(
        {(a, b, c, h) =>
          h.assert_eq[U8](a, 0)
          h.assert_eq[U8](b, 128)
          h.assert_eq[U8](c, 255)
        })?

class \nodoc\ iso _ForAll4Test is UnitTest
  fun name(): String => "pony_test/for_all4"

  fun apply(h: TestHelper) ? =>
    h.for_all4[U8, U8, U8, U8](
      recover Generators.unit[U8](0) end,
      recover Generators.unit[U8](1) end,
      recover Generators.unit[U8](2) end,
      recover Generators.unit[U8](3) end)(
        {(a, b, c, d, h) =>
          h.assert_eq[U8](a, 0)
          h.assert_eq[U8](b, 1)
          h.assert_eq[U8](c, 2)
          h.assert_eq[U8](d, 3)
        })?

class \nodoc\ iso _GenRndTest is UnitTest
  fun name(): String => "Gen/random_behaviour"

  fun apply(h: TestHelper) ? =>
    let gen = Generators.u32()
    let rnd1 = Randomness(0)
    let rnd2 = Randomness(0)
    let rnd3 = Randomness(1)
    var same: U32 = 0
    for x in Range(0, 100) do
      let g1 = gen.generate(rnd1)?
      let g2 = gen.generate(rnd2)?
      let g3 = gen.generate(rnd3)?
      h.assert_eq[U32](g1, g2)
      if g1 == g3 then
        same = same + 1
      end
    end
    h.assert_ne[U32](same, 100)

class \nodoc\ iso _GenFilterTest is UnitTest
  fun name(): String => "Gen/filter"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.u32().filter({
        (u: U32^): (U32^, Bool) =>
          (u, (u % 2) == 0)
      })
    let rnd = Randomness(Time.millis())
    for x in Range(0, 100) do
      let v = gen.generate(rnd)?
      h.assert_true((v % 2) == 0)
    end

class \nodoc\ iso _GenUnionTest is UnitTest
  fun name(): String => "Gen/union"

  fun apply(h: TestHelper) ? =>
    let gen = Generators.ascii().union[U8](Generators.u8())
    let rnd = Randomness(Time.millis())
    var got_string: Bool = false
    var got_u8: Bool = false
    for x in Range(0, 100) do
      match \exhaustive\ gen.generate(rnd)?
      | let vs: String => got_string = true
      | let vs: U8 => got_u8 = true
      end
    end
    h.assert_true(got_string, "union never generated String")
    h.assert_true(got_u8, "union never generated U8")

class \nodoc\ iso _GenFrequencyTest is UnitTest
  fun name(): String => "Gen/frequency"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.frequency[U8](
        [ as WeightedGenerator[U8]:
          (1, Generators.unit[U8](0))
          (0, Generators.unit[U8](42))
          (2, Generators.unit[U8](1))
        ])
    let rnd: Randomness ref = Randomness(Time.millis())

    let generated = Array[U8](100)
    for i in Range(0, 100) do
      generated.push(gen.generate(rnd)?)
    end
    h.assert_false(
      generated.contains(U8(42)),
      "frequency generated value with 0 weight")
    h.assert_true(
      generated.contains(U8(0)),
      "frequency did not generate value with weight of 1")
    h.assert_true(
      generated.contains(U8(1)),
      "frequency did not generate value with weight of 2")

    let empty_gen = Generators.frequency[U8](Array[WeightedGenerator[U8]](0))

    h.assert_error({() ? =>
      empty_gen.generate(Randomness(Time.millis()))?
    })

class \nodoc\ iso _GenFrequencySafeTest is UnitTest
  fun name(): String => "Gen/frequency_safe"

  fun apply(h: TestHelper) =>
    h.assert_error({() ? =>
      Generators.frequency_safe[U8](Array[WeightedGenerator[U8]](0))?
    })

class \nodoc\ iso _GenOneOfTest is UnitTest
  fun name(): String => "Gen/one_of"

  fun apply(h: TestHelper) ? =>
    let gen = Generators.one_of[U8]([as U8: 0; 1])
    let rnd = Randomness(Time.millis())
    for x in Range(0, 100) do
      let v = gen.generate(rnd)?
      h.assert_true(
        (v == 0) or (v == 1),
        "one_of generator generated illegal value")
    end
    let empty_gen = Generators.one_of[U8](Array[U8](0))

    h.assert_error({() ? =>
      empty_gen.generate(Randomness(Time.millis()))?
    })

class \nodoc\ iso _GenOneOfSafeTest is UnitTest
  fun name(): String => "Gen/one_of_safe"

  fun apply(h: TestHelper) =>
    h.assert_error({() ? =>
      Generators.one_of_safe[U8](Array[U8](0))?
    })

class \nodoc\ iso _SeqOfTest is UnitTest
  fun name(): String => "Gen/seq_of"

  fun apply(h: TestHelper) ? =>
    let seq_gen =
      Generators.seq_of[U8, Array[U8]](
        Generators.u8(),
        0,
        10)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = seq_gen.generate(rnd)?
      h.assert_true(
        (sample.size() >= 0) and (sample.size() <= 10),
        "Seqs generated with Generators.seq_of are out of bounds")
    end

class \nodoc\ iso _SetOfTest is UnitTest
  fun name(): String => "Gen/set_of"

  fun apply(h: TestHelper) ? =>
    let set_gen =
      Generators.set_of[U8](
        Generators.u8()
        where to = 1024)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample: Set[U8] = set_gen.generate(rnd)?
      h.assert_true(sample.size() <= 256, "something about U8 is not right")
    end

class \nodoc\ iso _SetOfMaxTest is UnitTest
  fun name(): String => "Gen/set_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    for size in Range[USize](1, U8.max_value().usize()) do
      let set_gen =
        Generators.set_of[U8](
          Generators.u8()
          where to = size)
      let sample: Set[U8] = set_gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated set is too big.")
    end

class \nodoc\ iso _SetOfEmptyTest is UnitTest
  fun name(): String => "Gen/set_of_empty"

  fun apply(h: TestHelper) ? =>
    let set_gen =
      Generators.set_of[U8](
        Generators.u8()
        where to = 0)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample: Set[U8] = set_gen.generate(rnd)?
      h.assert_true(sample.size() == 0, "non-empty set created.")
    end

class \nodoc\ iso _SetIsOfIdentityTest is UnitTest
  fun name(): String => "Gen/set_is_of_identity"

  fun apply(h: TestHelper) ? =>
    let set_is_gen_same =
      Generators.set_is_of[String](
        Generators.unit[String]("the highlander")
        where to = 100)
    let rnd = Randomness(Time.millis())
    let sample: SetIs[String] = set_is_gen_same.generate(rnd)?
    h.assert_true(
      sample.size() <= 1,
      "invalid SetIs instances generated: size " + sample.size().string())

class \nodoc\ iso _MapOfEmptyTest is UnitTest
  fun name(): String => "Gen/map_of_empty"

  fun apply(h: TestHelper) ? =>
    let map_gen =
      Generators.map_of[String, I64](
        Generators.zip2[String, I64](
          Generators.u8().map[String](
            {(u: U8): String^ =>
              let s = u.string()
              consume s
            }),
          Generators.i64(-10, 10))
        where to = 0)
    let rnd = Randomness(Time.millis())
    let sample = map_gen.generate(rnd)?
    h.assert_eq[USize](sample.size(), 0, "non-empty map created")

class \nodoc\ iso _MapOfMaxTest is UnitTest
  fun name(): String => "Gen/map_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())

    for size in Range(1, U8.max_value().usize()) do
      let map_gen =
        Generators.map_of[String, I64](
          Generators.zip2[String, I64](
            Generators.u16().map[String^]({(u: U16): String^ =>
              u.string()
            }),
            Generators.i64(-10, 10))
          where to = size)
      let sample = map_gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated map is too big.")
    end

class \nodoc\ iso _MapOfIdentityTest is UnitTest
  fun name(): String => "Gen/map_of_identity"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let map_gen =
      Generators.map_of[String, I64](
        Generators.zip2[String, I64](
          Generators.repeatedly[String](
            {(): String^ =>
              let s = recover String.create(14) end
              s.add("the highlander")
              consume s
            }),
          Generators.i64(-10, 10))
        where to = 100)
    let sample = map_gen.generate(rnd)?
    h.assert_true(sample.size() <= 1)

class \nodoc\ iso _MapIsOfEmptyTest is UnitTest
  fun name(): String => "Gen/map_is_of_empty"

  fun apply(h: TestHelper) ? =>
    let map_is_gen =
      Generators.map_is_of[String, I64](
        Generators.zip2[String, I64](
          Generators.u8().map[String](
            {(u: U8): String^ =>
              let s = u.string()
              consume s
            }),
          Generators.i64(-10, 10))
        where to = 0)
    let rnd = Randomness(Time.millis())
    let sample = map_is_gen.generate(rnd)?
    h.assert_eq[USize](sample.size(), 0, "non-empty map created")

class \nodoc\ iso _MapIsOfMaxTest is UnitTest
  fun name(): String => "Gen/map_is_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())

    for size in Range(1, U8.max_value().usize()) do
      let map_is_gen =
        Generators.map_is_of[String, I64](
          Generators.zip2[String, I64](
            Generators.u16().map[String](
              {(u: U16): String^ =>
                let s = u.string()
                consume s
              }),
            Generators.i64(-10, 10))
          where to = size)
      let sample = map_is_gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated map is too big.")
    end

class \nodoc\ iso _MapIsOfIdentityTest is UnitTest
  fun name(): String => "Gen/map_is_of_identity"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let map_gen =
      Generators.map_is_of[String, I64](
        Generators.zip2[String, I64](
          Generators.unit[String]("the highlander"),
          Generators.i64(-10, 10))
        where to = 100)
    let sample = map_gen.generate(rnd)?
    h.assert_true(sample.size() <= 1)

class \nodoc\ iso _SetOfMinTest is UnitTest
  fun name(): String => "Gen/set_of_min"

  fun apply(h: TestHelper) ? =>
    let set_gen =
      Generators.set_of[U8](
        Generators.u8()
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample: Set[U8] = set_gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty set created with min = 1")
    end

class \nodoc\ iso _SetIsOfMinTest is UnitTest
  fun name(): String => "Gen/set_is_of_min"

  fun apply(h: TestHelper) ? =>
    let set_is_gen =
      Generators.set_is_of[String](
        Generators.ascii(where from = 1, to = 10)
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample: SetIs[String] = set_is_gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty SetIs created with min = 1")
    end

class \nodoc\ iso _MapOfMinTest is UnitTest
  fun name(): String => "Gen/map_of_min"

  fun apply(h: TestHelper) ? =>
    let map_gen =
      Generators.map_of[String, I64](
        Generators.zip2[String, I64](
          Generators.u16().map[String^]({(u: U16): String^ =>
            u.string()
          }),
          Generators.i64(-10, 10))
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = map_gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty map created with min = 1")
    end

class \nodoc\ iso _MapIsOfMinTest is UnitTest
  fun name(): String => "Gen/map_is_of_min"

  fun apply(h: TestHelper) ? =>
    let map_is_gen =
      Generators.map_is_of[String, I64](
        Generators.zip2[String, I64](
          Generators.u16().map[String^]({(u: U16): String^ =>
            u.string()
          }),
          Generators.i64(-10, 10))
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = map_is_gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty MapIs created with min = 1")
    end

class \nodoc\ iso _ASCIIRangeTest is UnitTest
  fun name(): String => "Gen/ascii_range"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let ascii_gen =
      Generators.ascii(where from = 1, to = 1, range = ASCIIAll)

    for i in Range[USize](0, 100) do
      let sample = ascii_gen.generate(rnd)?
      h.assert_true(
        ASCIIAll().contains(sample),
        "\"" + sample + "\" not valid ascii")
    end

class \nodoc\ iso _GenF32Test is UnitTest
  fun name(): String => "Gen/f32"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let gen = Generators.f32(where from = 5.0, to = 10.0)
    for i in Range[USize](0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(sample >= 5.0, "f32 below min")
      h.assert_true(sample <= 10.0, "f32 above max")
    end

class \nodoc\ iso _GenF32ReversedTest is UnitTest
  fun name(): String => "Gen/f32_from_to_reversed"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let gen = Generators.f32(where from = 10.0, to = 5.0)
    for i in Range[USize](0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(sample >= 5.0, "f32 below min when from > to")
      h.assert_true(sample <= 10.0, "f32 above max when from > to")
    end

class \nodoc\ iso _GenF32FullRangeTest is UnitTest
  fun name(): String => "Gen/f32_full_range"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let gen =
      Generators.f32(where from = -F32.max_value(), to = F32.max_value())
    for i in Range[USize](0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(sample.finite(), "f32 full range produced non-finite")
    end

class \nodoc\ iso _GenF32NaNTest is UnitTest
  fun name(): String => "Gen/f32_nan_errors"

  fun apply(h: TestHelper) =>
    let rnd = Randomness(Time.millis())
    let nan = F32(0) / F32(0)
    let gen = Generators.f32(where from = nan, to = 1.0)
    try
      gen.generate(rnd)?
      h.fail("f32 NaN input should error")
    end

class \nodoc\ iso _GenF64Test is UnitTest
  fun name(): String => "Gen/f64"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let gen = Generators.f64(where from = 50.0, to = 100.0)
    for i in Range[USize](0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(sample >= 50.0, "f64 below min")
      h.assert_true(sample <= 100.0, "f64 above max")
    end

class \nodoc\ iso _GenF64ReversedTest is UnitTest
  fun name(): String => "Gen/f64_from_to_reversed"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let gen = Generators.f64(where from = 100.0, to = 50.0)
    for i in Range[USize](0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(sample >= 50.0, "f64 below min when from > to")
      h.assert_true(sample <= 100.0, "f64 above max when from > to")
    end

class \nodoc\ iso _GenF64FullRangeTest is UnitTest
  fun name(): String => "Gen/f64_full_range"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let gen =
      Generators.f64(where from = -F64.max_value(), to = F64.max_value())
    for i in Range[USize](0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(sample.finite(), "f64 full range produced non-finite")
    end

class \nodoc\ iso _GenF64NaNTest is UnitTest
  fun name(): String => "Gen/f64_nan_errors"

  fun apply(h: TestHelper) =>
    let rnd = Randomness(Time.millis())
    let nan = F64(0) / F64(0)
    let gen = Generators.f64(where from = nan, to = 1.0)
    try
      gen.generate(rnd)?
      h.fail("f64 NaN input should error")
    end

class \nodoc\ iso _UTF32CodePointStringTest is UnitTest
  fun name(): String => "Gen/utf32_codepoint_string"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let string_gen =
      Generators.utf32_codepoint_string(
        Generators.u32(),
        50,
        100)

    for i in Range[USize](0, 100) do
      let sample = string_gen.generate(rnd)?
      for cp in sample.runes() do
        h.assert_true(
          (cp <= 0xD7FF) or (cp >= 0xE000),
          "\"" + sample + "\" invalid utf32")
      end
    end

class \nodoc\ iso _SuccessfulIntProperty is IntProperty
  fun name(): String  => "property/int/property"

  fun ref int_property[T: (Int & Integer[T] val)](x: T, h: PropertyHelper) =>
    h.assert_eq[T](x.min(T.max_value()), x)
    h.assert_eq[T](x.max(T.min_value()), x)

class \nodoc\ iso _SuccessfulIntPairProperty is IntPairProperty
  fun name(): String => "property/intpair/property"

  fun int_property[T: (Int & Integer[T] val)](
    x: T, y: T, h: PropertyHelper)
  =>
    h.assert_eq[T](x * y, y * x)

class \nodoc\ iso _ErroringGeneratorProperty is Property[String]
  fun name(): String => "property_runner/erroring_generator/property"

  fun params(): PropertyParams =>
    PropertyParams(where regression_db' = false)

  fun gen(): Generator[String] =>
    Generator[String](
      object is GenObj[String]
        fun generate(r: Randomness): String^ ? =>
          error
      end)

  fun ref property(sample: String, h: PropertyHelper) =>
    None

class \nodoc\ iso _SometimesErroringGeneratorProperty is Property[String]
  fun name(): String => "property_runner/sometimes_erroring_generator"

  fun params(): PropertyParams =>
    PropertyParams(where
      num_samples' = 3,
      seed' = 6,
      max_generator_retries' = 1
    )

  fun gen(): Generator[String] =>
    Generator[String](
      object is GenObj[String]
        fun generate(r: Randomness): String^ ? =>
          match (r.u64() % 2)
          | 0 => "foo"
          else
            error
          end
      end
    )

  fun ref property(sample: String, h: PropertyHelper) =>
    None

interface \nodoc\ val _RandomCase[A: Comparable[A] #read]
  new val create()

  fun test(min: A, max: A): A

  fun generator(): Generator[A]

primitive \nodoc\ _RandomCaseF32 is _RandomCase[F32]
  fun test(min: F32, max: F32): F32 =>
    let rnd = Randomness(Time.millis())
    rnd.f32(min, max)

  fun generator(): Generator[F32] =>
    Generators.f32(where from = -F32.max_value(), to = F32.max_value())

primitive \nodoc\ _RandomCaseF64 is _RandomCase[F64]
  fun test(min: F64, max: F64): F64 =>
    let rnd = Randomness(Time.millis())
    rnd.f64(min, max)

  fun generator(): Generator[F64] =>
    Generators.f64(where from = -F64.max_value(), to = F64.max_value())

primitive \nodoc\ _RandomCaseU8 is _RandomCase[U8]
  fun test(min: U8, max: U8): U8 =>
    let rnd = Randomness(Time.millis())
    rnd.u8(min, max)

  fun generator(): Generator[U8] =>
    Generators.u8()

primitive \nodoc\ _RandomCaseU16 is _RandomCase[U16]
  fun test(min: U16, max: U16): U16 =>
    let rnd = Randomness(Time.millis())
    rnd.u16(min, max)

  fun generator(): Generator[U16] =>
    Generators.u16()

primitive \nodoc\ _RandomCaseU32 is _RandomCase[U32]
  fun test(min: U32, max: U32): U32 =>
    let rnd = Randomness(Time.millis())
    rnd.u32(min, max)

  fun generator(): Generator[U32] =>
    Generators.u32()

primitive \nodoc\ _RandomCaseU64 is _RandomCase[U64]
  fun test(min: U64, max: U64): U64 =>
    let rnd = Randomness(Time.millis())
    rnd.u64(min, max)

  fun generator(): Generator[U64] =>
    Generators.u64()

primitive \nodoc\ _RandomCaseU128 is _RandomCase[U128]
  fun test(min: U128, max: U128): U128 =>
    let rnd = Randomness(Time.millis())
    rnd.u128(min, max)

  fun generator(): Generator[U128] =>
    Generators.u128()

primitive \nodoc\ _RandomCaseI8 is _RandomCase[I8]
  fun test(min: I8, max: I8): I8 =>
    let rnd = Randomness(Time.millis())
    rnd.i8(min, max)

  fun generator(): Generator[I8] =>
    Generators.i8()

primitive \nodoc\ _RandomCaseI16 is _RandomCase[I16]
  fun test(min: I16, max: I16): I16 =>
    let rnd = Randomness(Time.millis())
    rnd.i16(min, max)

  fun generator(): Generator[I16] =>
    Generators.i16()

primitive \nodoc\ _RandomCaseI32 is _RandomCase[I32]
  fun test(min: I32, max: I32): I32 =>
    let rnd = Randomness(Time.millis())
    rnd.i32(min, max)

  fun generator(): Generator[I32] =>
    Generators.i32()

primitive \nodoc\ _RandomCaseI64 is _RandomCase[I64]
  fun test(min: I64, max: I64): I64 =>
    let rnd = Randomness(Time.millis())
    rnd.i64(min, max)

  fun generator(): Generator[I64] =>
    Generators.i64()

primitive \nodoc\ _RandomCaseI128 is _RandomCase[I128]
  fun test(min: I128, max: I128): I128 =>
    let rnd = Randomness(Time.millis())
    rnd.i128(min, max)

  fun generator(): Generator[I128] =>
    Generators.i128()

primitive \nodoc\ _RandomCaseISize is _RandomCase[ISize]
  fun test(min: ISize, max: ISize): ISize =>
    let rnd = Randomness(Time.millis())
    rnd.isize(min, max)

  fun generator(): Generator[ISize] =>
    Generators.isize()

primitive \nodoc\ _RandomCaseILong is _RandomCase[ILong]
  fun test(min: ILong, max: ILong): ILong =>
    let rnd = Randomness(Time.millis())
    rnd.ilong(min, max)

  fun generator(): Generator[ILong] =>
    Generators.ilong()

class \nodoc\ iso _RandomnessProperty[
  A: Comparable[A] #read, R: _RandomCase[A] val]
  is Property[(A, A)]
  let _type_name: String

  new iso create(type_name: String) =>
    _type_name = type_name

  fun name(): String => "randomness/" + _type_name

  fun gen(): Generator[(A, A)] =>
    let min = R.generator()
    let max = R.generator()
    Generators.zip2[A, A](min, max)
      .filter(
        {(pair) => (pair, (pair._1 <= pair._2)) }
      )

  fun property(arg1: (A, A), h: PropertyHelper) =>
    (let min, let max) = arg1

    let value = R.test(min, max)
    h.assert_true(value >= min)
    h.assert_true(value <= max)

class \nodoc\ iso _VecOfEmptyTest is UnitTest
  fun name(): String => "Gen/vec_of_empty"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.vec_of[U8](
        Generators.u8()
        where to = 0)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_eq[USize](sample.size(), 0, "non-empty vec created")
    end

class \nodoc\ iso _VecOfFromToReversedTest is UnitTest
  fun name(): String => "Gen/vec_of_from_to_reversed"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.vec_of[U8](
        Generators.u8()
        where from = 10, to = 5)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 5,
        "vec smaller than lo when from > to")
      h.assert_true(
        sample.size() <= 10,
        "vec larger than hi when from > to")
    end

class \nodoc\ iso _VecOfMaxTest is UnitTest
  fun name(): String => "Gen/vec_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    for size in Range[USize](1, 50) do
      let gen =
        Generators.vec_of[U8](
          Generators.u8()
          where to = size)
      let sample = gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated vec is too big")
    end

class \nodoc\ iso _VecOfMinTest is UnitTest
  fun name(): String => "Gen/vec_of_min"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.vec_of[U8](
        Generators.u8()
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty vec created with from = 1")
    end

class \nodoc\ iso _PersistentListOfEmptyTest is UnitTest
  fun name(): String => "Gen/persistent_list_of_empty"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_list_of[U8](
        Generators.u8()
        where to = 0)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_eq[USize](sample.size(), 0, "non-empty list created")
    end

class \nodoc\ iso _PersistentListOfMaxTest is UnitTest
  fun name(): String => "Gen/persistent_list_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    for size in Range[USize](1, 50) do
      let gen =
        Generators.persistent_list_of[U8](
          Generators.u8()
          where to = size)
      let sample = gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated list is too big")
    end

class \nodoc\ iso _PersistentListOfMinTest is UnitTest
  fun name(): String => "Gen/persistent_list_of_min"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_list_of[U8](
        Generators.u8()
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty list created with from = 1")
    end

class \nodoc\ iso _PersistentSetIsOfIdentityTest is UnitTest
  fun name(): String => "Gen/persistent_set_is_of_identity"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_set_is_of[String](
        Generators.unit[String]("the highlander")
        where to = 100)
    let rnd = Randomness(Time.millis())
    let sample = gen.generate(rnd)?
    h.assert_true(
      sample.size() <= 1,
      "invalid persistent SetIs: size " + sample.size().string())

class \nodoc\ iso _PersistentSetOfEmptyTest is UnitTest
  fun name(): String => "Gen/persistent_set_of_empty"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_set_of[U8](
        Generators.u8()
        where to = 0)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_eq[USize](sample.size(), 0, "non-empty set created")
    end

class \nodoc\ iso _PersistentSetOfMaxTest is UnitTest
  fun name(): String => "Gen/persistent_set_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    for size in Range[USize](1, U8.max_value().usize()) do
      let gen =
        Generators.persistent_set_of[U8](
          Generators.u8()
          where to = size)
      let sample = gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated set is too big")
    end

class \nodoc\ iso _PersistentSetOfMinTest is UnitTest
  fun name(): String => "Gen/persistent_set_of_min"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_set_of[U8](
        Generators.u8()
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty set created with from = 1")
    end

class \nodoc\ iso _PersistentSetIsOfEmptyTest is UnitTest
  fun name(): String => "Gen/persistent_set_is_of_empty"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_set_is_of[U8](
        Generators.u8()
        where to = 0)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_eq[USize](sample.size(), 0, "non-empty SetIs created")
    end

class \nodoc\ iso _PersistentSetIsOfMaxTest is UnitTest
  fun name(): String => "Gen/persistent_set_is_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    for size in Range[USize](1, U8.max_value().usize()) do
      let gen =
        Generators.persistent_set_is_of[U8](
          Generators.u8()
          where to = size)
      let sample = gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated SetIs is too big")
    end

class \nodoc\ iso _PersistentSetIsOfMinTest is UnitTest
  fun name(): String => "Gen/persistent_set_is_of_min"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_set_is_of[U8](
        Generators.u8()
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty SetIs created with from = 1")
    end

class \nodoc\ iso _PersistentMapOfIdentityTest is UnitTest
  fun name(): String => "Gen/persistent_map_of_identity"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let gen =
      Generators.persistent_map_of[String, U8](
        Generators.zip2[String, U8](
          Generators.repeatedly[String](
            {(): String^ =>
              let s = recover String.create(14) end
              s.add("the highlander")
              consume s
            }),
          Generators.u8())
        where to = 100)
    let sample = gen.generate(rnd)?
    h.assert_true(sample.size() <= 1)

class \nodoc\ iso _PersistentMapIsOfIdentityTest is UnitTest
  fun name(): String => "Gen/persistent_map_is_of_identity"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    let gen =
      Generators.persistent_map_is_of[String, U8](
        Generators.zip2[String, U8](
          Generators.unit[String]("the highlander"),
          Generators.u8())
        where to = 100)
    let sample = gen.generate(rnd)?
    h.assert_true(sample.size() <= 1)

class \nodoc\ iso _PersistentMapOfEmptyTest is UnitTest
  fun name(): String => "Gen/persistent_map_of_empty"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_map_of[U8, U8](
        Generators.zip2[U8, U8](
          Generators.u8(),
          Generators.u8())
        where to = 0)
    let rnd = Randomness(Time.millis())
    let sample = gen.generate(rnd)?
    h.assert_eq[USize](sample.size(), 0, "non-empty map created")

class \nodoc\ iso _PersistentMapOfMaxTest is UnitTest
  fun name(): String => "Gen/persistent_map_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    for size in Range[USize](1, U8.max_value().usize()) do
      let gen =
        Generators.persistent_map_of[U8, U8](
          Generators.zip2[U8, U8](
            Generators.u8(),
            Generators.u8())
          where to = size)
      let sample = gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated map is too big")
    end

class \nodoc\ iso _PersistentMapOfMinTest is UnitTest
  fun name(): String => "Gen/persistent_map_of_min"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_map_of[U8, U8](
        Generators.zip2[U8, U8](
          Generators.u8(),
          Generators.u8())
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty map created with from = 1")
    end

class \nodoc\ iso _PersistentMapIsOfEmptyTest is UnitTest
  fun name(): String => "Gen/persistent_map_is_of_empty"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_map_is_of[U8, U8](
        Generators.zip2[U8, U8](
          Generators.u8(),
          Generators.u8())
        where to = 0)
    let rnd = Randomness(Time.millis())
    let sample = gen.generate(rnd)?
    h.assert_eq[USize](sample.size(), 0, "non-empty MapIs created")

class \nodoc\ iso _PersistentMapIsOfMaxTest is UnitTest
  fun name(): String => "Gen/persistent_map_is_of_max"

  fun apply(h: TestHelper) ? =>
    let rnd = Randomness(Time.millis())
    for size in Range[USize](1, U8.max_value().usize()) do
      let gen =
        Generators.persistent_map_is_of[U8, U8](
          Generators.zip2[U8, U8](
            Generators.u8(),
            Generators.u8())
          where to = size)
      let sample = gen.generate(rnd)?
      h.assert_true(sample.size() <= size, "generated MapIs is too big")
    end

class \nodoc\ iso _PersistentMapIsOfMinTest is UnitTest
  fun name(): String => "Gen/persistent_map_is_of_min"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.persistent_map_is_of[U8, U8](
        Generators.zip2[U8, U8](
          Generators.u8(),
          Generators.u8())
        where from = 1)
    let rnd = Randomness(Time.millis())
    for i in Range(0, 100) do
      let sample = gen.generate(rnd)?
      h.assert_true(
        sample.size() >= 1,
        "empty MapIs created with from = 1")
    end

// --- Replay determinism test ---
class \nodoc\ iso _ReplayDeterminismTest is UnitTest
  fun name(): String => "replay/determinism"

  fun apply(h: TestHelper) ? =>
    let gen = Generators.u32(0, 1000)
    let rnd = Randomness(42)
    rnd._start_recording()
    let v1 = gen.generate(rnd)?
    let choices = rnd._get_choices()

    let rnd2 = Randomness(0)
    rnd2._replay(choices)
    let v2 = gen.generate(rnd2)?

    h.assert_eq[U32](v1, v2)

// --- Boolean-per-element structure test ---
class \nodoc\ iso _BooleanPerElementStructureTest is UnitTest
  fun name(): String => "structure/boolean_per_element"

  fun apply(h: TestHelper) ? =>
    let gen =
      Generators.array_of[U8](Generators.u8() where from = 0, to = 5)
    let rnd = Randomness(42)
    rnd._start_recording()
    let arr = gen.generate(rnd)?
    let choices: Array[_Choice val] val = rnd._get_choices()
    let spans: Array[_Span val] val = rnd._get_spans()

    var bool_count: USize = 0
    var int_count: USize = 0
    for choice in choices.values() do
      match choice
      | let _: _BoolChoice => bool_count = bool_count + 1
      | let _: _IntChoice => int_count = int_count + 1
      end
    end

    h.assert_true(
      bool_count >= arr.size(),
      "expected at least " + arr.size().string() +
        " bool choices, got " + bool_count.string())

    h.assert_eq[USize](int_count, arr.size())

    h.assert_true(
      spans.size() >= arr.size(),
      "expected at least " + arr.size().string() +
        " spans, got " + spans.size().string())

    for span in spans.values() do
      h.assert_true(
        span.label is SpanElement,
        "expected SpanElement label, got " + span.label.string())
    end

// --- Shrinker unit tests with hand-constructed choice sequences ---
class \nodoc\ iso _ShrinkerDeleteSpanTest is UnitTest
  fun name(): String => "shrinker/delete_span"

  fun apply(h: TestHelper) ? =>
    let choices: Array[_Choice val] val =
      [_IntChoice(10, 0, 100, 0); _IntChoice(20, 0, 100, 0)]
    let spans: Array[_Span val] val =
      [_Span(0, 2, SpanShuffle, false)]
    let shrinker = _Shrinker(choices, spans)
    let candidates = shrinker.candidates()

    h.assert_true(candidates.has_next())
    let candidate = candidates.next()?
    h.assert_eq[USize](candidate.size(), 0)

class \nodoc\ iso _ShrinkerLowerChoicesTest is UnitTest
  fun name(): String => "shrinker/lower_choices"

  fun apply(h: TestHelper) ? =>
    let choices: Array[_Choice val] val =
      [_IntChoice(100, 0, 100, 0)]
    let spans: Array[_Span val] val =
      recover val Array[_Span val] end
    let shrinker = _Shrinker(choices, spans)
    let candidates = shrinker.candidates()

    h.assert_true(candidates.has_next())
    let c1 = candidates.next()?
    h.assert_eq[USize](c1.size(), 1)
    match c1(0)?
    | let ic: _IntChoice =>
      h.assert_eq[I128](ic.value, 50)
    else
      h.fail("expected IntChoice")
    end

class \nodoc\ iso _ShrinkerConvergenceLoopTest is UnitTest
  fun name(): String => "shrinker/convergence_loop"

  fun apply(h: TestHelper) ? =>
    let choices: Array[_Choice val] val =
      [_IntChoice(100, 0, 100, 0)]
    let spans: Array[_Span val] val =
      recover val Array[_Span val] end
    let shrinker =
      _Shrinker(choices, spans where max_reductions = 10)
    let candidates = shrinker.candidates()

    h.assert_true(candidates.has_next())
    let c1 = candidates.next()?
    let new_spans: Array[_Span val] val =
      recover val Array[_Span val] end
    shrinker.accept(c1, new_spans)

    var found_25 = false
    var count: USize = 0
    while candidates.has_next() and (count < 20) do
      let c = candidates.next()?
      match try c(0)? end
      | let ic: _IntChoice =>
        if ic.value == 25 then
          found_25 = true
          break
        end
      end
      count = count + 1
    end
    h.assert_true(found_25, "convergence loop should produce value 25")

class \nodoc\ iso _ShrinkerRedistributeTest is UnitTest
  fun name(): String => "shrinker/redistribute"

  fun apply(h: TestHelper) ? =>
    let choices: Array[_Choice val] val =
      [_IntChoice(80, 0, 100, 0); _IntChoice(20, 0, 100, 0)]
    let spans: Array[_Span val] val =
      recover val Array[_Span val] end
    let shrinker = _Shrinker(choices, spans)
    let candidates = shrinker.candidates()

    var found = false
    var count: USize = 0
    while candidates.has_next() and (count < 50) do
      let c = candidates.next()?
      if c.size() == 2 then
        match (c(0)?, c(1)?)
        | (let a: _IntChoice, let b: _IntChoice) =>
          if (a.value == 0) and (b.value == 100) then
            found = true
            break
          end
        end
      end
      count = count + 1
    end
    h.assert_true(found, "redistribute should produce (0, 100)")

class \nodoc\ iso _ShrinkerRedistributeU128Test is UnitTest
  fun name(): String => "shrinker/redistribute_u128"

  fun apply(h: TestHelper) ? =>
    let choices: Array[_Choice val] val =
      [_U128Choice(80, 0, 100, 0); _U128Choice(20, 0, 100, 0)]
    let spans: Array[_Span val] val =
      recover val Array[_Span val] end
    let shrinker = _Shrinker(choices, spans)
    let candidates = shrinker.candidates()

    var found = false
    var count: USize = 0
    while candidates.has_next() and (count < 50) do
      let c = candidates.next()?
      if c.size() == 2 then
        match (c(0)?, c(1)?)
        | (let a: _U128Choice, let b: _U128Choice) =>
          if (a.value == 0) and (b.value == 100) then
            found = true
            break
          end
        end
      end
      count = count + 1
    end
    h.assert_true(found, "redistribute should produce (0, 100)")

class \nodoc\ iso _ShrinkerShortenEmptyTest is UnitTest
  fun name(): String => "shrinker/shorten_empty"

  fun apply(h: TestHelper) =>
    let choices: Array[_Choice val] val =
      recover val Array[_Choice val] end
    let spans: Array[_Span val] val =
      recover val Array[_Span val] end
    let shrinker = _Shrinker(choices, spans)
    let iter = _ShortenIter(shrinker)
    h.assert_false(iter.has_next())

class \nodoc\ iso _ShrinkerShortenTest is UnitTest
  fun name(): String => "shrinker/shorten"

  fun apply(h: TestHelper) ? =>
    let choices: Array[_Choice val] val =
      [ _IntChoice(10, 0, 100, 0)
        _IntChoice(20, 0, 100, 0)
        _IntChoice(30, 0, 100, 0)
        _IntChoice(40, 0, 100, 0) ]
    let spans: Array[_Span val] val =
      recover val Array[_Span val] end
    let shrinker = _Shrinker(choices, spans where max_reductions = 1)
    let iter = _ShortenIter(shrinker)

    h.assert_true(iter.has_next())
    let c1 = iter.next()?
    h.assert_eq[USize](c1.size(), 3)

    h.assert_true(iter.has_next())
    let c2 = iter.next()?
    h.assert_eq[USize](c2.size(), 1)

    h.assert_false(iter.has_next())

class \nodoc\ iso _ShrinkerSortSpansTest is UnitTest
  fun name(): String => "shrinker/sort_spans"

  fun apply(h: TestHelper) ? =>
    let choices: Array[_Choice val] val =
      [ _IntChoice(50, 0, 100, 0)
        _IntChoice(60, 0, 100, 0)
        _IntChoice(10, 0, 100, 0)
        _IntChoice(20, 0, 100, 0) ]
    let spans: Array[_Span val] val =
      [_Span(0, 2, SpanElement, false); _Span(2, 4, SpanElement, false)]
    let shrinker = _Shrinker(choices, spans where max_reductions = 1)
    let iter = _SortSpansIter(shrinker)

    h.assert_true(iter.has_next())
    let candidate = iter.next()?
    h.assert_eq[USize](candidate.size(), 4)

    match candidate(0)?
    | let ic: _IntChoice => h.assert_eq[I128](ic.value, 10)
    else h.fail("expected IntChoice at 0")
    end
    match candidate(1)?
    | let ic: _IntChoice => h.assert_eq[I128](ic.value, 20)
    else h.fail("expected IntChoice at 1")
    end
    match candidate(2)?
    | let ic: _IntChoice => h.assert_eq[I128](ic.value, 50)
    else h.fail("expected IntChoice at 2")
    end
    match candidate(3)?
    | let ic: _IntChoice => h.assert_eq[I128](ic.value, 60)
    else h.fail("expected IntChoice at 3")
    end

class \nodoc\ iso _ShrinkerBoolLoweringTest is UnitTest
  fun name(): String => "shrinker/bool_lowering"

  fun apply(h: TestHelper) ? =>
    let choices: Array[_Choice val] val =
      [_BoolChoice(true)]
    let spans: Array[_Span val] val =
      recover val Array[_Span val] end
    let shrinker = _Shrinker(choices, spans)
    let iter = _LowerChoicesIter(shrinker)

    h.assert_true(iter.has_next())
    let candidate = iter.next()?
    h.assert_eq[USize](candidate.size(), 1)
    match candidate(0)?
    | let bc: _BoolChoice =>
      h.assert_false(bc.value)
    else
      h.fail("expected BoolChoice")
    end

class \nodoc\ iso _ShrinkerForcedBoolSkipTest is UnitTest
  fun name(): String => "shrinker/forced_bool_skip"

  fun apply(h: TestHelper) =>
    let choices: Array[_Choice val] val =
      [_BoolChoice(true, true)]
    let spans: Array[_Span val] val =
      recover val Array[_Span val] end
    let shrinker = _Shrinker(choices, spans)
    let iter = _LowerChoicesIter(shrinker)

    h.assert_false(iter.has_next())

class \nodoc\ iso _ShrinkerLowerU128Test is UnitTest
  fun name(): String => "shrinker/lower_u128"

  fun apply(h: TestHelper) ? =>
    let choices: Array[_Choice val] val =
      [_U128Choice(U128(100), 0, 100, 0)]
    let spans: Array[_Span val] val =
      recover val Array[_Span val] end
    let shrinker = _Shrinker(choices, spans)
    let iter = _LowerChoicesIter(shrinker)

    h.assert_true(iter.has_next())
    let candidate = iter.next()?
    h.assert_eq[USize](candidate.size(), 1)
    match candidate(0)?
    | let uc: _U128Choice =>
      h.assert_eq[U128](uc.value, 50)
    else
      h.fail("expected U128Choice")
    end

class \nodoc\ iso _ShrinkerLowerFloatTest is UnitTest
  fun name(): String => "shrinker/lower_float"

  fun apply(h: TestHelper) ? =>
    let choices: Array[_Choice val] val =
      [_FloatChoice(100.0, 0.0, 100.0)]
    let spans: Array[_Span val] val =
      recover val Array[_Span val] end
    let shrinker = _Shrinker(choices, spans)
    let iter = _LowerChoicesIter(shrinker)

    h.assert_true(iter.has_next())
    let candidate = iter.next()?
    h.assert_eq[USize](candidate.size(), 1)
    match candidate(0)?
    | let fc: _FloatChoice =>
      h.assert_eq[F64](fc.value, 50.0)
      h.assert_eq[F64](fc.min, 0.0)
      h.assert_eq[F64](fc.max, 100.0)
    else
      h.fail("expected FloatChoice")
    end

class \nodoc\ iso _ShrinkerDeleteDiscardedSpanTest is UnitTest
  fun name(): String => "shrinker/delete_discarded_span"

  fun apply(h: TestHelper) ? =>
    let choices: Array[_Choice val] val =
      [ _IntChoice(10, 0, 100, 0)
        _IntChoice(20, 0, 100, 0)
        _IntChoice(30, 0, 100, 0)
        _IntChoice(40, 0, 100, 0) ]
    let spans: Array[_Span val] val =
      [ _Span(0, 2, SpanFilter, true)
        _Span(2, 4, SpanElement, false) ]
    let shrinker = _Shrinker(choices, spans)
    let iter = _DeleteSpansIter(shrinker)

    h.assert_true(iter.has_next())
    let candidate = iter.next()?
    h.assert_eq[USize](candidate.size(), 2)
    match candidate(0)?
    | let ic: _IntChoice => h.assert_eq[I128](ic.value, 10)
    else h.fail("expected IntChoice at 0")
    end

    h.assert_false(iter.has_next())

class \nodoc\ iso _ShrinkerSortSpansLengthTest is UnitTest
  fun name(): String => "shrinker/sort_spans_length"

  fun apply(h: TestHelper) ? =>
    let choices: Array[_Choice val] val =
      [ _IntChoice(1, 0, 100, 0)
        _IntChoice(2, 0, 100, 0)
        _IntChoice(3, 0, 100, 0)
        _IntChoice(4, 0, 100, 0)
        _IntChoice(5, 0, 100, 0) ]
    let spans: Array[_Span val] val =
      [_Span(0, 3, SpanElement, false); _Span(3, 5, SpanElement, false)]
    let shrinker = _Shrinker(choices, spans where max_reductions = 1)
    let iter = _SortSpansIter(shrinker)

    h.assert_true(iter.has_next())
    let candidate = iter.next()?
    h.assert_eq[USize](candidate.size(), 5)

    match candidate(0)?
    | let ic: _IntChoice => h.assert_eq[I128](ic.value, 4)
    else h.fail("expected IntChoice at 0")
    end
    match candidate(1)?
    | let ic: _IntChoice => h.assert_eq[I128](ic.value, 5)
    else h.fail("expected IntChoice at 1")
    end
    match candidate(2)?
    | let ic: _IntChoice => h.assert_eq[I128](ic.value, 1)
    else h.fail("expected IntChoice at 2")
    end

// --- Serializer tests ---
class \nodoc\ iso _SerializerRoundTripAllTypesTest is UnitTest
  fun name(): String => "serializer/round_trip_all_types"

  fun apply(h: TestHelper) ? =>
    let choices: Array[_Choice val] val =
      [ _IntChoice(42, 0, 100, 0)
        _BoolChoice(true)
        _U128Choice(U128(999), 0, U128.max_value(), 0)
        _FloatChoice(3.14, -1e10, 1e10) ]
    let text: String val = _ChoiceSerializer.serialize(choices)
    let restored =
      _ChoiceSerializer.deserialize(text) as Array[_Choice val] val
    h.assert_eq[USize](restored.size(), 4)
    match restored(0)?
    | let ic: _IntChoice =>
      h.assert_eq[I128](ic.value, 42)
      h.assert_eq[I128](ic.min, 0)
      h.assert_eq[I128](ic.max, 100)
    else h.fail("expected IntChoice at 0")
    end
    match restored(1)?
    | let bc: _BoolChoice => h.assert_true(bc.value)
    else h.fail("expected BoolChoice at 1")
    end
    match restored(2)?
    | let uc: _U128Choice =>
      h.assert_eq[U128](uc.value, 999)
      h.assert_eq[U128](uc.max, U128.max_value())
    else h.fail("expected U128Choice at 2")
    end
    match restored(3)?
    | let fc: _FloatChoice =>
      h.assert_eq[F64](fc.value, 3.14)
      h.assert_eq[F64](fc.min, -1e10)
      h.assert_eq[F64](fc.max, 1e10)
    else h.fail("expected FloatChoice at 3")
    end

class \nodoc\ iso _SerializerEmptyArrayTest is UnitTest
  fun name(): String => "serializer/empty_array"

  fun apply(h: TestHelper) ? =>
    let choices: Array[_Choice val] val =
      recover val Array[_Choice val] end
    let text: String val = _ChoiceSerializer.serialize(choices)
    let restored =
      _ChoiceSerializer.deserialize(text) as Array[_Choice val] val
    h.assert_eq[USize](restored.size(), 0)

class \nodoc\ iso _SerializerBadHeaderTest is UnitTest
  fun name(): String => "serializer/bad_header"

  fun apply(h: TestHelper) =>
    match _ChoiceSerializer.deserialize("NOT_A_HEADER\n")
    | let _: Array[_Choice val] val =>
      h.fail("bad header should return None")
    end

class \nodoc\ iso _SerializerBadLineTest is UnitTest
  fun name(): String => "serializer/bad_line"

  fun apply(h: TestHelper) =>
    match _ChoiceSerializer.deserialize(
      "ponytest v1\nBAD_DATA\n")
    | let _: Array[_Choice val] val =>
      h.fail("bad line should return None")
    end

class \nodoc\ iso _SerializerFloatExactBitsTest is UnitTest
  fun name(): String => "serializer/float_exact_bits"

  fun apply(h: TestHelper) ? =>
    let choices: Array[_Choice val] val =
      [ _IntChoice(
        F64(3.14).bits().i128(),
        I128.min_value(),
        I128.max_value(),
        0) ]
    let text: String val = _ChoiceSerializer.serialize(choices)
    let restored =
      _ChoiceSerializer.deserialize(text) as Array[_Choice val] val
    match restored(0)?
    | let ic: _IntChoice =>
      let restored_f = F64.from_bits(ic.value.u64())
      h.assert_eq[F64](restored_f, 3.14)
    else h.fail("expected IntChoice")
    end

class \nodoc\ iso _SerializerNaNRoundTripTest is UnitTest
  fun name(): String => "serializer/nan_round_trip"

  fun apply(h: TestHelper) ? =>
    let nan_bits = (F64(0) / F64(0)).bits()
    let choices: Array[_Choice val] val =
      [ _IntChoice(
        nan_bits.i128(),
        I128.min_value(),
        I128.max_value(),
        0) ]
    let text: String val = _ChoiceSerializer.serialize(choices)
    let restored =
      _ChoiceSerializer.deserialize(text) as Array[_Choice val] val
    match restored(0)?
    | let ic: _IntChoice =>
      let restored_f = F64.from_bits(ic.value.u64())
      h.assert_true(restored_f.nan(), "NaN not preserved")
    else h.fail("expected IntChoice")
    end

class \nodoc\ iso _EncodeNameSafeCharsTest is UnitTest
  fun name(): String => "regression_db/encode_name/safe_chars"

  fun apply(h: TestHelper) =>
    let encoded = _RegressionDb._encode_name("simple_test-123")
    h.assert_eq[String](encoded, "simple_test-123")

class \nodoc\ iso _EncodeNameSpecialCharsTest is UnitTest
  fun name(): String => "regression_db/encode_name/special_chars"

  fun apply(h: TestHelper) =>
    let encoded = _RegressionDb._encode_name("test/with spaces!")
    h.assert_true(
      not encoded.contains("/"),
      "encoded name should not contain /")
    h.assert_true(
      not encoded.contains(" "),
      "encoded name should not contain spaces")

class \nodoc\ iso _RegressionDbSaveLoadClearTest is UnitTest
  fun name(): String => "regression_db/save_load_clear"

  fun apply(h: TestHelper) ? =>
    let dir = _TempDir(h.env)?
    let choices: Array[_Choice val] val =
      [_IntChoice(42, 0, 100, 0)]
    _RegressionDb.save(dir, "test_prop", choices, h)
    match _RegressionDb.load(dir, "test_prop", h)
    | let loaded: Array[_Choice val] val =>
      h.assert_eq[USize](loaded.size(), 1)
      match loaded(0)?
      | let ic: _IntChoice =>
        h.assert_eq[I128](ic.value, 42)
      else h.fail("expected IntChoice")
      end
    else
      h.fail("expected loaded choices")
    end
    _RegressionDb.clear(dir, "test_prop", h)
    match _RegressionDb.load(dir, "test_prop", h)
    | let _: Array[_Choice val] val =>
      h.fail("expected None after clear")
    end

class \nodoc\ iso _RegressionDbLoadMissingTest is UnitTest
  fun name(): String => "regression_db/load_missing"

  fun apply(h: TestHelper) ? =>
    let dir = _TempDir(h.env)?
    match _RegressionDb.load(dir, "nonexistent", h)
    | let _: Array[_Choice val] val =>
      h.fail("expected None for missing prop")
    end

class \nodoc\ iso _RegressionDbCorruptDeletesTest is UnitTest
  fun name(): String => "regression_db/corrupt_deletes"

  fun apply(h: TestHelper) ? =>
    let dir = _TempDir(h.env)?
    let file_path = dir.join("nonexistent.choices")?
    let file = CreateFile(file_path) as File
    file.print("CORRUPT_DATA")
    file.dispose()
    match _RegressionDb.load(dir, "nonexistent", h)
    | let _: Array[_Choice val] val =>
      h.fail("expected None for corrupt data")
    end
    h.assert_false(file_path.exists(), "corrupt file should be deleted")

class \nodoc\ iso _RegressionDbCreatesDirTest is UnitTest
  fun name(): String => "regression_db/creates_dir"

  fun apply(h: TestHelper) ? =>
    let parent = _TempDir(h.env)?
    let sub = parent.join("sub")?
    let choices: Array[_Choice val] val =
      [_IntChoice(1, 0, 10, 0)]
    _RegressionDb.save(sub, "test", choices, h)
    h.assert_true(sub.exists(), "db should create its directory")

primitive \nodoc\ _TempDir
  fun apply(env: Env): FilePath ? =>
    let auth = FileAuth(env.root)
    FilePath.mkdtemp(auth, "/tmp/ponytest-property-")?

// --- Stateful property tests ---
primitive \nodoc\ _NoOp is Stringable
  fun string(): String iso^ => "no-op".string()

primitive \nodoc\ _Inc is Stringable
  fun string(): String iso^ => "inc".string()

primitive \nodoc\ _Dec is Stringable
  fun string(): String iso^ => "dec".string()

type _CounterCmd is (_Inc | _Dec)

class \nodoc\ iso _CounterProperty
  is StatefulProperty[USize, USize, _CounterCmd]
  fun name(): String => "stateful/counter/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 50, seed' = 42)

  fun max_steps(): USize => 10

  fun initial_sut(): USize => 0

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[USize, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : StepResult[_CounterCmd]
  =>
    if rnd.bool() then
      ctx.sut = ctx.sut + 1
      ctx.model = ctx.model + 1
      _Inc
    else
      if ctx.sut > 0 then
        ctx.sut = ctx.sut - 1
        ctx.model = ctx.model - 1
      end
      _Dec
    end

  fun invariant(
    ctx: StatefulContext[USize, USize] box, h: PropertyHelper): Bool
  =>
    ctx.sut == ctx.model

  fun final_check(
    ctx: StatefulContext[USize, USize] box, h: PropertyHelper): Bool
  =>
    ctx.sut == ctx.model

class \nodoc\ iso _FailingCounterProperty
  is StatefulProperty[USize, USize, _CounterCmd]
  fun name(): String => "stateful/failing_counter/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 50, seed' = 42,
      regression_db' = false)

  fun max_steps(): USize => 10

  fun initial_sut(): USize => 0

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[USize, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : StepResult[_CounterCmd]
  =>
    if rnd.bool() then
      ctx.sut = ctx.sut + 1
      ctx.model = ctx.model + 1
      _Inc
    else
      ctx.sut = ctx.sut + 1
      _Dec
    end

  fun invariant(
    ctx: StatefulContext[USize, USize] box, h: PropertyHelper): Bool
  =>
    ctx.sut == ctx.model

class \nodoc\ iso _SuccessfulStatefulPropertyTest is UnitTest
  fun name(): String => "stateful/counter/success"

  fun apply(h: TestHelper) ? =>
    _StatefulPropertyTest[USize, USize, _CounterCmd](
      _CounterProperty).apply(h)?

class \nodoc\ iso _StatefulMaxStepsZeroProperty
  is StatefulProperty[USize, USize, _NoOp]
  fun name(): String => "stateful/max_steps_zero/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 5, seed' = 42)

  fun max_steps(): USize => 0

  fun initial_sut(): USize => 0

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[USize, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : StepResult[_NoOp]
  =>
    h.fail("step should never be called with max_steps = 0")
    _NoOp

class \nodoc\ iso _StatefulMaxStepsZeroTest is UnitTest
  fun name(): String => "stateful/max_steps_zero"

  fun apply(h: TestHelper) ? =>
    _StatefulPropertyTest[USize, USize, _NoOp](
      _StatefulMaxStepsZeroProperty).apply(h)?

class \nodoc\ iso _StatefulStepAlwaysRejectsProperty
  is StatefulProperty[USize, USize, _NoOp]
  fun name(): String => "stateful/step_always_rejects/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 3, seed' = 42,
      max_generator_retries' = 2)

  fun max_steps(): USize => 5

  fun initial_sut(): USize => 0

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[USize, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : StepResult[_NoOp]
  =>
    StepReject

class \nodoc\ iso _StatefulStepRetriesProperty
  is StatefulProperty[USize, USize, _NoOp]
  """
  Step rejects on the first call, succeeds on all subsequent calls.
  Exercises the retry path: the first sample is discarded and a fresh
  one starts.
  """
  var _first_step: Bool = true

  fun name(): String => "stateful/step_retries/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 5, seed' = 42,
      max_generator_retries' = 3, regression_db' = false)

  fun max_steps(): USize => 3

  fun initial_sut(): USize => 0

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[USize, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : StepResult[_NoOp]
  =>
    if _first_step then
      _first_step = false
      return StepReject
    end
    _NoOp

class \nodoc\ iso _StatefulFinalCheckFailureProperty
  is StatefulProperty[USize, USize, _NoOp]
  fun name(): String => "stateful/final_check_failure/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 5, seed' = 42,
      regression_db' = false)

  fun max_steps(): USize => 3

  fun initial_sut(): USize => 0

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[USize, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : StepResult[_NoOp]
  =>
    let v = rnd.usize(0, 10)
    ctx.sut = ctx.sut + v
    ctx.model = ctx.model + v
    _NoOp

  fun invariant(
    ctx: StatefulContext[USize, USize] box, h: PropertyHelper): Bool
  =>
    true

  fun final_check(
    ctx: StatefulContext[USize, USize] box, h: PropertyHelper): Bool
  =>
    false

class \nodoc\ iso _StatefulStepSutErrorProperty
  is StatefulProperty[USize, USize, _Inc]
  """
  Returns StepFail when the SUT reaches 3, exercising the step-failure
  path through shrinking.
  """
  fun name(): String => "stateful/step_sut_error/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 50, seed' = 42,
      max_shrink_reductions' = 100,
      regression_db' = false)

  fun max_steps(): USize => 20

  fun initial_sut(): USize => 0

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[USize, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : StepResult[_Inc]
  =>
    ctx.sut = ctx.sut + 1
    ctx.model = ctx.model + 1
    if ctx.sut >= 3 then
      StepFail
    else
      _Inc
    end

class \nodoc\ iso _StatefulShrinkQualityProperty
  is StatefulProperty[USize, USize, _CounterCmd]
  fun name(): String => "stateful/shrink_quality/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 50, seed' = 42,
      max_shrink_reductions' = 100,
      regression_db' = false)

  fun max_steps(): USize => 20

  fun initial_sut(): USize => 0

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[USize, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : StepResult[_CounterCmd]
  =>
    if rnd.bool() then
      ctx.sut = ctx.sut + 1
      ctx.model = ctx.model + 1
      _Inc
    else
      if ctx.sut > 0 then
        ctx.sut = ctx.sut - 1
        ctx.model = ctx.model - 1
      end
      _Dec
    end

  fun invariant(
    ctx: StatefulContext[USize, USize] box, h: PropertyHelper): Bool
  =>
    ctx.sut < 5

class \nodoc\ iso _MultiCommandProperty
  is StatefulProperty[USize, USize, _CounterCmd]
  fun name(): String => "stateful/multi_command/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 20, seed' = 42)

  fun max_steps(): USize => 10

  fun initial_sut(): USize => 0

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[USize, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : StepResult[_CounterCmd]
  =>
    if rnd.bool() then
      ctx.sut = ctx.sut + 1
      ctx.model = ctx.model + 1
      _Inc
    else
      if ctx.sut > 0 then
        ctx.sut = ctx.sut - 1
        ctx.model = ctx.model - 1
      end
      _Dec
    end

  fun invariant(
    ctx: StatefulContext[USize, USize] box, h: PropertyHelper): Bool
  =>
    ctx.sut == ctx.model

class \nodoc\ iso _MultiCommandStatefulPropertyTest is UnitTest
  fun name(): String => "stateful/multi_command"

  fun apply(h: TestHelper) ? =>
    _StatefulPropertyTest[USize, USize, _CounterCmd](
      _MultiCommandProperty).apply(h)?

class \nodoc\ iso _AsyncStatefulProperty
  is StatefulProperty[USize, USize, _Inc]
  fun name(): String => "meta/async_stateful/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 5, seed' = 42,
      regression_db' = false)

  fun max_steps(): USize => 3

  fun initial_sut(): USize => 0

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[USize, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : StepResult[_Inc]
  =>
    h.long_test(10_000_000_000)
    h.expect_action("step")
    ctx.sut = ctx.sut + 1
    ctx.model = ctx.model + 1
    h.complete_action("step")
    _Inc

  fun invariant(
    ctx: StatefulContext[USize, USize] box, h: PropertyHelper): Bool
  =>
    ctx.sut == ctx.model

class \nodoc\ iso _AsyncStatefulPropertyTest is UnitTest
  """
  Runs a sub-PonyTest with a stateful property that uses
  expect_action/complete_action, verifying the async completion
  path works for stateful properties.
  """
  fun name(): String => "stateful/async"

  fun apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test.stateful_property(_AsyncStatefulProperty)
      end
    let collector = _PassCollector(h)
    let env =
      Env.create(
        h.env.root,
        h.env.input,
        collector,
        collector,
        recover val ["test"] end,
        h.env.vars,
        {(code: I32) => None })
    PonyTest(env, list)

// --- classify/tabulate/cover/collect property definitions ---
class \nodoc\ iso _ClassifyAllSameProperty is Property[U8]
  fun name(): String => "classify/single_dimension/property"

  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 1)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.classify("all")

class \nodoc\ iso _ClassifyMultiLabelProperty is Property[U8]
  fun name(): String => "classify/multi_label/property"

  fun gen(): Generator[U8] => Generators.u8(0, 1)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 1)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.classify("all")
    h.classify("duplicate")

class \nodoc\ iso _ClassifyAlphabeticalProperty is Property[U8]
  fun name(): String => "classify/alphabetical/property"

  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 1)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.classify("zebra")
    h.classify("apple")
    h.classify("mango")

class \nodoc\ iso _CoverSatisfiedProperty is Property[U8]
  fun name(): String => "cover/satisfied/property"

  fun gen(): Generator[U8] => Generators.u8(0, 1)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 100, seed' = 1)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.cover(true, "always", 50.0)

class \nodoc\ iso _CoverUnsatisfiedProperty is Property[U8]
  fun name(): String => "cover/unsatisfied/property"

  fun gen(): Generator[U8] => Generators.u8(0, 100)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 100, seed' = 1,
      regression_db' = false)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.cover(sample == 0, "zero", 90.0)

class \nodoc\ iso _CoverNoShrinkProperty is Property[U8]
  fun name(): String => "cover/no_shrink/property"

  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 1,
      regression_db' = false)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.cover(false, "impossible", 5.0)

class \nodoc\ iso _CoverAndClassifyProperty is Property[U8]
  fun name(): String => "cover/and_classify/property"

  fun gen(): Generator[U8] => Generators.u8(0, 1)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 1)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.classify("all")
    h.cover(true, "covered", 50.0)

class \nodoc\ iso _CoverLastWinsProperty is Property[U8]
  fun name(): String => "cover/last_wins/property"

  fun gen(): Generator[U8] => Generators.u8(0, 100)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 100, seed' = 1)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.cover(false, "mid", 99.0)
    h.cover(sample > 50, "mid", 1.0)

class \nodoc\ iso _CollectStringableProperty is Property[U8]
  fun name(): String => "collect/stringable/property"

  fun gen(): Generator[U8] => Generators.unit[U8](42)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 5, seed' = 1)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.collect(sample)

class \nodoc\ iso _TabulateSingleHeadingProperty is Property[U8]
  fun name(): String => "tabulate/single_heading/property"

  fun gen(): Generator[U8] => Generators.u8(0, 1)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 1)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.tabulate(
      "parity",
      if (sample %% 2) == 0 then "even" else "odd" end)

class \nodoc\ iso _TabulateMultipleHeadingsProperty is Property[U8]
  fun name(): String => "tabulate/multiple_headings/property"

  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 1)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.tabulate(
      "size",
      if sample < 5 then "small" else "large" end)
    h.tabulate(
      "parity",
      if (sample %% 2) == 0 then "even" else "odd" end)

class \nodoc\ iso _TabulateAndClassifyProperty is Property[U8]
  fun name(): String => "tabulate/and_classify/property"

  fun gen(): Generator[U8] => Generators.u8(0, 1)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 1)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.classify("flat")
    h.tabulate("grouped", "label")

class \nodoc\ iso _TabulateSameLabelDiffHeadingsProperty is Property[U8]
  fun name(): String => "tabulate/same_label_diff_headings/property"

  fun gen(): Generator[U8] => Generators.u8(0, 1)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 1)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.tabulate("alpha", "shared")
    h.tabulate("beta", "shared")

// --- Health check property definitions ---
class \nodoc\ iso _CleanProperty is Property[U8]
  fun name(): String => "health_check/clean/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 42)

  fun gen(): Generator[U8] => Generators.u8()

  fun ref property(arg1: U8, h: PropertyHelper) =>
    None

class \nodoc\ iso _DisabledCheckProperty is Property[U8]
  fun name(): String => "health_check/disabled/property"

  fun params(): PropertyParams =>
    PropertyParams(where
      num_samples' = 3,
      seed' = 42,
      max_sample_nanos' = 0)

  fun gen(): Generator[U8] => Generators.u8()

  fun ref property(arg1: U8, h: PropertyHelper) =>
    None

class \nodoc\ iso _AllDisabledCheckProperty is Property[U8]
  fun name(): String => "health_check/all_disabled/property"

  fun params(): PropertyParams =>
    PropertyParams(where
      num_samples' = 3,
      seed' = 42,
      max_filter_discard_ratio' = 0,
      max_choice_sequence_size' = 0,
      max_sample_nanos' = 0)

  fun gen(): Generator[U8] =>
    Generators.u8(0, 255)
      .filter({(u: U8): (U8^, Bool) => (u, (u % 20) == 0) })

  fun ref property(arg1: U8, h: PropertyHelper) =>
    None

// --- Multiple for_all test ---
class \nodoc\ iso _MultipleForAllTest is UnitTest
  fun name(): String => "pony_test/multiple_for_all"

  fun apply(h: TestHelper) ? =>
    h.for_all[U8](recover val Generators.u8(0, 10) end)(
      {(u, h) => h.assert_true(u <= 10) })?
    h.for_all[U8](recover val Generators.u8(0, 5) end)(
      {(u, h) => h.assert_true(u <= 5) })?

// --- Direct property execution tests ---
// These construct _PropertyExec directly and drive the sample loop,
// verifying failure detection, shrinking, coverage, and health checks
// without going through PonyTest.
class \nodoc\ iso _DirectErroringPropertyTest is UnitTest
  fun name(): String => "property/direct/erroring"

  fun apply(h: TestHelper) =>
    let prop = _ErroringProperty
    let params = prop.params()
    let exec = _PropertyExec[U8](consume prop, params, h, h.env)
    let ph = PropertyHelper._create(h, 0)
    exec.run_sample(ph)
    h.assert_false(
      exec.last_sample_passed(),
      "erroring property should fail")

class \nodoc\ iso _DirectErroringGeneratorTest is UnitTest
  fun name(): String => "property/direct/erroring_generator"

  fun apply(h: TestHelper) =>
    let prop = _ErroringGeneratorProperty
    let params = prop.params()
    let exec = _PropertyExec[String](consume prop, params, h, h.env)
    let ph = PropertyHelper._create(h, 0)
    exec.run_sample(ph)
    h.assert_true(
      exec.has_error(),
      "erroring generator should set has_error")
    h.assert_true(
      exec.error_message().contains("Unable to generate"),
      "error message should mention generation failure")

class \nodoc\ iso _DirectSometimesErroringGeneratorTest is UnitTest
  fun name(): String => "property/direct/sometimes_erroring_generator"

  fun apply(h: TestHelper) =>
    let prop = _SometimesErroringGeneratorProperty
    let params = prop.params()
    let exec = _PropertyExec[String](consume prop, params, h, h.env)
    let ph = PropertyHelper._create(h, 0)
    var all_passed = true
    while exec.has_more_samples() do
      exec.run_sample(ph)
      if exec.has_error() then
        all_passed = false
        break
      end
      if not exec.last_sample_passed() then
        all_passed = false
        break
      end
      exec.sample_passed()
    end
    h.assert_true(
      all_passed,
      "sometimes-erroring generator should complete all samples")

class \nodoc\ iso _DirectFailingStatefulTest is UnitTest
  fun name(): String => "stateful/direct/failing_counter"

  fun apply(h: TestHelper) =>
    let prop = _FailingCounterProperty
    let params = prop.params()
    let exec =
      _StatefulPropertyExec[USize, USize, _CounterCmd](
        consume prop, params, h, h.env)
    let ph = PropertyHelper._create(h, 0)
    var found_failure = false
    while exec.has_more_samples() do
      exec.run_sample(ph)
      if exec.has_error() then
        found_failure = true
        break
      end
      if not exec.last_sample_passed() then
        found_failure = true
        break
      end
      exec.sample_passed()
    end
    h.assert_true(
      found_failure,
      "failing counter should produce a failing sample")

class \nodoc\ iso _DirectStepAlwaysRejectsTest is UnitTest
  fun name(): String => "stateful/direct/step_always_rejects"

  fun apply(h: TestHelper) =>
    let prop = _StatefulStepAlwaysRejectsProperty
    let params = prop.params()
    let exec =
      _StatefulPropertyExec[USize, USize, _NoOp](
        consume prop, params, h, h.env)
    let ph = PropertyHelper._create(h, 0)
    exec.run_sample(ph)
    h.assert_false(
      exec.last_sample_passed(),
      "step-always-errors should fail")

class \nodoc\ iso _DirectStepRetryTest is UnitTest
  """
  Verifies that a step error below the retry threshold discards the
  sample and retries with a fresh one rather than failing the property.
  """
  fun name(): String => "stateful/direct/step_retry"

  fun apply(h: TestHelper) =>
    let prop = _StatefulStepRetriesProperty
    let params = prop.params()
    let exec =
      _StatefulPropertyExec[USize, USize, _NoOp](
        consume prop, params, h, h.env)
    let ph = PropertyHelper._create(h, 0)
    var all_passed = true
    while exec.has_more_samples() do
      exec.run_sample(ph)
      if exec.has_error() then
        all_passed = false
        break
      end
      if not exec.last_sample_passed() then
        all_passed = false
        break
      end
      exec.sample_passed()
    end
    h.assert_true(
      all_passed,
      "step error below retry threshold should retry, not fail")

class \nodoc\ iso _DirectFinalCheckFailureTest is UnitTest
  fun name(): String => "stateful/direct/final_check_failure"

  fun apply(h: TestHelper) =>
    let prop = _StatefulFinalCheckFailureProperty
    let params = prop.params()
    let exec =
      _StatefulPropertyExec[USize, USize, _NoOp](
        consume prop, params, h, h.env)
    let ph = PropertyHelper._create(h, 0)
    var found_failure = false
    while exec.has_more_samples() do
      exec.run_sample(ph)
      if exec.has_error() then
        found_failure = true
        break
      end
      if not exec.last_sample_passed() then
        found_failure = true
        break
      end
      exec.sample_passed()
    end
    h.assert_true(
      found_failure,
      "final_check returning false should produce a failing sample")

class \nodoc\ iso _DirectStatefulShrinkQualityTest is UnitTest
  fun name(): String => "stateful/direct/shrink_quality"

  fun apply(h: TestHelper) =>
    let prop = _StatefulShrinkQualityProperty
    let params = prop.params()
    let exec =
      _StatefulPropertyExec[USize, USize, _CounterCmd](
        consume prop, params, h, h.env)
    let ph = PropertyHelper._create(h, 0)
    var found_failure = false
    while exec.has_more_samples() do
      exec.run_sample(ph)
      if not exec.last_sample_passed() then
        found_failure = true
        break
      end
      exec.sample_passed()
    end
    if not found_failure then
      h.fail("expected property to fail")
      return
    end
    exec.sample_failed()
    if not exec.needs_shrink() then
      h.fail("expected shrinker to have choices")
      return
    end
    exec.begin_shrink()
    while not exec.shrink_exhausted() do
      exec.run_shrink_candidate(ph)
    end
    let repr = exec.sample_repr()
    // The invariant (ctx.sut < 5) fires at 5 net increments, so
    // the minimal trace is 5 inc commands. The original trace has
    // up to 20 steps; a well-shrunk trace should be shorter.
    h.assert_false(
      repr.contains("20. "),
      "shrunk trace should be shorter than 20 steps, got: " + repr)
    h.assert_true(
      repr.contains("Invariant failure"),
      "shrunk trace should show invariant failure, got: " + repr)

class \nodoc\ iso _DirectStepSutErrorTest is UnitTest
  fun name(): String => "stateful/direct/step_sut_error"

  fun apply(h: TestHelper) =>
    let prop = _StatefulStepSutErrorProperty
    let params = prop.params()
    let exec =
      _StatefulPropertyExec[USize, USize, _Inc](
        consume prop, params, h, h.env)
    let ph = PropertyHelper._create(h, 0)
    var found_failure = false
    while exec.has_more_samples() do
      exec.run_sample(ph)
      if not exec.last_sample_passed() then
        found_failure = true
        break
      end
      exec.sample_passed()
    end
    if not found_failure then
      h.fail("expected property to fail")
      return
    end
    exec.sample_failed()
    if not exec.needs_shrink() then
      h.fail("expected shrinker to have choices")
      return
    end
    exec.begin_shrink()
    while not exec.shrink_exhausted() do
      exec.run_shrink_candidate(ph)
    end
    let repr = exec.sample_repr()
    h.assert_true(
      repr.contains("Step failure"),
      "trace should show step failure, got: " + repr)
    h.assert_false(
      repr.contains("20. "),
      "shrunk trace should be shorter than 20 steps, got: " + repr)

class \nodoc\ iso _DirectCoverUnsatisfiedTest is UnitTest
  fun name(): String => "cover/direct/unsatisfied"

  fun apply(h: TestHelper) =>
    // h.cover() routes through the runner asynchronously, so in
    // direct tests we call exec.cover() after each sample instead.
    let prop = _CoverUnsatisfiedProperty
    let params = prop.params()
    let exec = _PropertyExec[U8](consume prop, params, h, h.env)
    let ph = PropertyHelper._create(h, 0)
    var sample_num: USize = 0
    while exec.has_more_samples() do
      exec.run_sample(ph)
      if not exec.last_sample_passed() then
        h.fail("samples should all pass")
        return
      end
      // Simulate what h.cover() would do through the runner:
      // "zero" must appear in >= 90% of samples — impossible with
      // U8 range 0..100
      exec.cover(sample_num == 0, "zero", 90.0)
      exec.sample_passed()
      sample_num = sample_num + 1
    end
    h.assert_false(
      exec.coverage_passed(),
      "coverage should fail with unsatisfied cover condition")

class \nodoc\ iso _DirectCoverNoShrinkTest is UnitTest
  fun name(): String => "cover/direct/no_shrink"

  fun apply(h: TestHelper) =>
    let prop = _CoverNoShrinkProperty
    let params = prop.params()
    let exec = _PropertyExec[U8](consume prop, params, h, h.env)
    let ph = PropertyHelper._create(h, 0)
    while exec.has_more_samples() do
      exec.run_sample(ph)
      if not exec.last_sample_passed() then
        h.fail("samples should all pass")
        return
      end
      // "impossible" condition is always false, must be >= 5%
      exec.cover(false, "impossible", 5.0)
      exec.sample_passed()
    end
    h.assert_false(
      exec.coverage_passed(),
      "coverage should fail with impossible cover condition")

// Error-based shrink quality properties (test the shrinker via error)
class \nodoc\ iso _ShrinkIntToMinErrorProperty is Property[U32]
  fun name(): String => "shrink/int_to_min_error/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 100,
      max_shrink_reductions' = 100, seed' = 42,
      regression_db' = false)

  fun gen(): Generator[U32] => Generators.u32(0, 1000)

  fun ref property(sample: U32, h: PropertyHelper) ? =>
    if sample != 0 then error end

class \nodoc\ iso _DirectShrinkIntToMinTest is UnitTest
  fun name(): String => "shrink/direct/int_to_min"

  fun apply(h: TestHelper) =>
    let prop = _ShrinkIntToMinErrorProperty
    let params = prop.params()
    let exec = _PropertyExec[U32](consume prop, params, h, h.env)
    let ph = PropertyHelper._create(h, 0)
    var found_failure = false
    while exec.has_more_samples() do
      exec.run_sample(ph)
      if not exec.last_sample_passed() then
        found_failure = true
        break
      end
      exec.sample_passed()
    end
    if not found_failure then
      h.fail("expected property to fail")
      return
    end
    exec.sample_failed()
    exec.begin_shrink()
    while not exec.shrink_exhausted() do
      exec.run_shrink_candidate(ph)
    end
    h.assert_true(
      exec.sample_repr() == "1",
      "shrinker should converge to sample 1, got: " +
      exec.sample_repr())

class \nodoc\ iso _ShrinkIntAboveThresholdErrorProperty is Property[U32]
  fun name(): String => "shrink/int_above_threshold_error/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 100,
      max_shrink_reductions' = 100, seed' = 42,
      regression_db' = false)

  fun gen(): Generator[U32] => Generators.u32(0, 1000)

  fun ref property(sample: U32, h: PropertyHelper) ? =>
    if sample > 5 then error end

class \nodoc\ iso _DirectShrinkIntAboveThresholdTest is UnitTest
  fun name(): String => "shrink/direct/int_above_threshold"

  fun apply(h: TestHelper) =>
    let prop = _ShrinkIntAboveThresholdErrorProperty
    let params = prop.params()
    let exec = _PropertyExec[U32](consume prop, params, h, h.env)
    let ph = PropertyHelper._create(h, 0)
    var found_failure = false
    while exec.has_more_samples() do
      exec.run_sample(ph)
      if not exec.last_sample_passed() then
        found_failure = true
        break
      end
      exec.sample_passed()
    end
    if not found_failure then
      h.fail("expected property to fail")
      return
    end
    exec.sample_failed()
    exec.begin_shrink()
    while not exec.shrink_exhausted() do
      exec.run_shrink_candidate(ph)
    end
    h.assert_true(
      exec.sample_repr() == "6",
      "shrinker should converge to sample 6, got: " +
      exec.sample_repr())

class \nodoc\ iso _ShrinkArrayToMinErrorProperty is Property[Array[U8]]
  fun name(): String => "shrink/array_to_min_error/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 100,
      max_shrink_reductions' = 100, seed' = 42,
      regression_db' = false)

  fun gen(): Generator[Array[U8]] =>
    Generators.array_of[U8](Generators.u8() where from = 1, to = 20)

  fun ref property(sample: Array[U8], h: PropertyHelper) ? =>
    if sample.size() > 0 then error end

class \nodoc\ iso _DirectShrinkArrayToMinTest is UnitTest
  fun name(): String => "shrink/direct/array_to_min"

  fun apply(h: TestHelper) =>
    let prop = _ShrinkArrayToMinErrorProperty
    let params = prop.params()
    let exec = _PropertyExec[Array[U8]](consume prop, params, h, h.env)
    let ph = PropertyHelper._create(h, 0)
    var found_failure = false
    while exec.has_more_samples() do
      exec.run_sample(ph)
      if not exec.last_sample_passed() then
        found_failure = true
        break
      end
      exec.sample_passed()
    end
    if not found_failure then
      h.fail("expected property to fail")
      return
    end
    exec.sample_failed()
    exec.begin_shrink()
    while not exec.shrink_exhausted() do
      exec.run_shrink_candidate(ph)
    end
    h.assert_true(
      exec.sample_repr() == "[0]",
      "shrinker should converge to [0], got: " +
      exec.sample_repr())

class \nodoc\ iso _ShrinkFilterPreservationErrorProperty is Property[U32]
  fun name(): String => "shrink/filter_preservation_error/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 100,
      max_shrink_reductions' = 100, seed' = 42,
      regression_db' = false)

  fun gen(): Generator[U32] =>
    Generators.u32(0, 1000)
      .filter({(u: U32): (U32^, Bool) => (u, (u % 2) == 0) })

  fun ref property(sample: U32, h: PropertyHelper) ? =>
    if sample > 10 then error end

class \nodoc\ iso _DirectShrinkFilterPreservationTest is UnitTest
  fun name(): String => "shrink/direct/filter_preservation"

  fun apply(h: TestHelper) =>
    let prop = _ShrinkFilterPreservationErrorProperty
    let params = prop.params()
    let exec = _PropertyExec[U32](consume prop, params, h, h.env)
    let ph = PropertyHelper._create(h, 0)
    var found_failure = false
    while exec.has_more_samples() do
      exec.run_sample(ph)
      if not exec.last_sample_passed() then
        found_failure = true
        break
      end
      exec.sample_passed()
    end
    if not found_failure then
      h.fail("expected property to fail")
      return
    end
    exec.sample_failed()
    exec.begin_shrink()
    while not exec.shrink_exhausted() do
      exec.run_shrink_candidate(ph)
    end
    h.assert_true(
      exec.sample_repr().contains("12"),
      "shrinker should converge to 12 (smallest even > 10), got: " +
      exec.sample_repr())

class \nodoc\ iso _ShrinkFlatMapErrorProperty is Property[(U32, U32)]
  fun name(): String => "shrink/flat_map_error/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 100,
      max_shrink_reductions' = 200, seed' = 42,
      regression_db' = false)

  fun gen(): Generator[(U32, U32)] =>
    Generators.u32(1, 100)
      .flat_map[(U32, U32)](
        {(outer: U32): Generator[(U32, U32)] =>
          Generators.map2[U32, U32, (U32, U32)](
            Generators.unit[U32](outer),
            Generators.u32(1, 100),
            {(a: U32, b: U32): (U32, U32) => (a, b) })
        })

  fun ref property(sample: (U32, U32), h: PropertyHelper) ? =>
    if (sample._1 * sample._2) > 50 then error end

class \nodoc\ iso _DirectShrinkFlatMapTest is UnitTest
  fun name(): String => "shrink/direct/flat_map"

  fun apply(h: TestHelper) =>
    let prop = _ShrinkFlatMapErrorProperty
    let params = prop.params()
    let exec = _PropertyExec[(U32, U32)](consume prop, params, h, h.env)
    let ph = PropertyHelper._create(h, 0)
    var found_failure = false
    while exec.has_more_samples() do
      exec.run_sample(ph)
      if not exec.last_sample_passed() then
        found_failure = true
        break
      end
      exec.sample_passed()
    end
    if not found_failure then
      h.fail("expected property to fail")
      return
    end
    exec.sample_failed()
    exec.begin_shrink()
    while not exec.shrink_exhausted() do
      exec.run_shrink_candidate(ph)
    end
    h.assert_true(
      exec.sample_repr().contains("(1, 51)"),
      "shrinker should converge to (1, 51), got: " +
      exec.sample_repr())

// --- Meta-tests: run a sub-PonyTest and verify its output ---
class \nodoc\ iso _AssertionOnlyFailProperty is Property[U8]
  """
  A property that fails only via h.assert_true(false) — it never
  throws an error. Used to verify that sync-mode assertion failures
  trigger shrinking.
  """
  fun name(): String => "meta/assertion_only_fail/property"

  fun gen(): Generator[U8] => Generators.u8(0, 100)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 100, seed' = 42,
      max_shrink_reductions' = 50, regression_db' = false)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.assert_true(sample == 0)

class \nodoc\ iso _AssertionOnlyFailTest is UnitTest
  """
  Runs a sub-PonyTest with a property that fails via assertion only
  (no error throw), and verifies that the failure is detected and
  shrinking occurs.
  """
  fun name(): String => "property/assertion_only_fail"

  fun apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test.property(_AssertionOnlyFailProperty)
      end
    let detector = _FailDetector(h)
    let env =
      Env.create(
        h.env.root,
        h.env.input,
        detector,
        detector,
        recover val ["test"] end,
        h.env.vars,
        {(code: I32) => detector.done() })
    PonyTest(env, list)

class \nodoc\ iso _AsyncSimpleProperty is Property[U8]
  """
  A property that signals async mode by calling long_test from
  within property(). All samples should pass.
  """
  fun name(): String => "meta/async_property/property"

  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 1,
      regression_db' = false)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.long_test(10_000_000_000)
    h.assert_true(sample <= 10)

class \nodoc\ iso _AsyncPropertyTest is UnitTest
  """
  Runs a sub-PonyTest with a property that calls long_test() from
  within property(), verifying that async mode detection works and
  the test passes.
  """
  fun name(): String => "property/async_long_test"

  fun apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test.property(_AsyncSimpleProperty)
      end
    let collector = _PassCollector(h)
    let env =
      Env.create(
        h.env.root,
        h.env.input,
        collector,
        collector,
        recover val ["test"] end,
        h.env.vars,
        {(code: I32) => None })
    PonyTest(env, list)

class \nodoc\ iso _AsyncFailingProperty is Property[U8]
  """
  A property that signals async mode via long_test and then fails
  via assertion for some samples. Used to verify that async-mode
  failure detection and shrinking work.
  """
  fun name(): String => "meta/async_failing/property"

  fun gen(): Generator[U8] => Generators.u8(0, 100)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 100, seed' = 42,
      max_shrink_reductions' = 50, regression_db' = false)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.long_test(10_000_000_000)
    h.assert_true(sample == 0)

class \nodoc\ iso _AsyncFailingPropertyTest is UnitTest
  """
  Runs a sub-PonyTest with an async property that fails, verifying
  that the failure is detected and reported.
  """
  fun name(): String => "property/async_failing"

  fun apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test.property(_AsyncFailingProperty)
      end
    let detector = _FailDetector(h)
    let env =
      Env.create(
        h.env.root,
        h.env.input,
        detector,
        detector,
        recover val ["test"] end,
        h.env.vars,
        {(code: I32) => detector.done() })
    PonyTest(env, list)

actor \nodoc\ _PassCollector is OutStream
  """
  Collects output from a sub-PonyTest run and verifies that all
  tests passed. Detects completion by watching for the summary
  output rather than the exit callback, because PonyTest only
  calls exitcode on failure.
  """
  let _h: TestHelper
  var _saw_passed: Bool = false
  var _saw_failed: Bool = false

  new create(h: TestHelper) =>
    _h = h

  be print(data: ByteSeq) =>
    let s: String =
      match \exhaustive\ data
      | let str: String => str
      | let a: Array[U8] val => String.from_array(a)
      end
    if s.contains("---- Passed:") then
      _saw_passed = true
      _check_done()
    end
    if s.contains("FAILED:") then
      _saw_failed = true
    end

  be write(data: ByteSeq) => None
  be printv(data: ByteSeqIter) => None
  be writev(data: ByteSeqIter) => None
  be flush() => None

  fun ref _check_done() =>
    if _saw_passed and (not _saw_failed) then
      _h.complete(true)
    end

actor \nodoc\ _FailDetector is OutStream
  """
  Detects failure in a sub-PonyTest run via the exit callback.
  PonyTest calls `exitcode(-1)` on failure, so the exit callback
  firing is sufficient to detect failure — no output parsing needed.
  """
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  be print(data: ByteSeq) => None
  be write(data: ByteSeq) => None
  be printv(data: ByteSeqIter) => None
  be writev(data: ByteSeqIter) => None
  be flush() => None

  be done() =>
    _h.complete(true)

// --- Async property test (rewritten for new architecture) ---
class \nodoc\ iso _ClassifyForAllTest is UnitTest
  fun name(): String => "classify/for_all"

  fun apply(h: TestHelper) ? =>
    h.for_all[U8](recover Generators.u8(0, 10) end)(
      {(sample, h) =>
        h.classify("inline")
      })?

// --- Health check reporter unit tests ---
class \nodoc\ iso _HealthCheckFilterWarningTest is UnitTest
  fun name(): String => "health_check/filter_warning"

  fun apply(h: TestHelper) =>
    let params =
      PropertyParams(where max_filter_discard_ratio' = 1.0)
    let count =
      _HealthCheckReporter.report(params, 10, 200, 10, 0, 0, h)
    h.assert_true(
      count > 0,
      "filter warning should fire when ratio exceeds threshold")

class \nodoc\ iso _HealthCheckFilterBelowThresholdTest is UnitTest
  fun name(): String => "health_check/filter_below_threshold"

  fun apply(h: TestHelper) =>
    let params =
      PropertyParams(where max_filter_discard_ratio' = 1.0)
    let count =
      _HealthCheckReporter.report(params, 10, 5, 10, 0, 0, h)
    h.assert_eq[USize](0, count)

class \nodoc\ iso _HealthCheckChoiceWarningTest is UnitTest
  fun name(): String => "health_check/choice_warning"

  fun apply(h: TestHelper) =>
    let params =
      PropertyParams(where max_choice_sequence_size' = 10)
    let count =
      _HealthCheckReporter.report(params, 5, 0, 0, 50, 0, h)
    h.assert_true(
      count > 0,
      "choice warning should fire when max exceeds threshold")

class \nodoc\ iso _HealthCheckChoiceBelowThresholdTest is UnitTest
  fun name(): String => "health_check/choice_below_threshold"

  fun apply(h: TestHelper) =>
    let params =
      PropertyParams(where max_choice_sequence_size' = 100)
    let count =
      _HealthCheckReporter.report(params, 5, 0, 0, 50, 0, h)
    h.assert_eq[USize](0, count)

class \nodoc\ iso _HealthCheckSlowWarningTest is UnitTest
  fun name(): String => "health_check/slow_warning"

  fun apply(h: TestHelper) =>
    let params =
      PropertyParams(where max_sample_nanos' = 1_000_000)
    let count =
      _HealthCheckReporter.report(params, 3, 0, 0, 0, 5_000_000_000, h)
    h.assert_true(
      count > 0,
      "slow warning should fire when peak exceeds threshold")

class \nodoc\ iso _HealthCheckSlowBelowThresholdTest is UnitTest
  fun name(): String => "health_check/slow_below_threshold"

  fun apply(h: TestHelper) =>
    let params =
      PropertyParams(where max_sample_nanos' = 10_000_000_000)
    let count =
      _HealthCheckReporter.report(params, 3, 0, 0, 0, 5_000_000, h)
    h.assert_eq[USize](0, count)

class \nodoc\ iso _HealthCheckNoWarningTest is UnitTest
  fun name(): String => "health_check/no_warning"

  fun apply(h: TestHelper) =>
    let params = PropertyParams
    let count =
      _HealthCheckReporter.report(params, 10, 0, 0, 0, 0, h)
    h.assert_eq[USize](0, count)

class \nodoc\ iso _HealthCheckDisabledWarningTest is UnitTest
  fun name(): String => "health_check/disabled_warning"

  fun apply(h: TestHelper) =>
    let params = PropertyParams(where max_sample_nanos' = 0)
    let count =
      _HealthCheckReporter.report(
        params, 3, 0, 0, 0, 999_999_999_999, h)
    h.assert_eq[USize](0, count)

class \nodoc\ iso _HealthCheckAllDisabledWarningTest is UnitTest
  fun name(): String => "health_check/all_disabled_warning"

  fun apply(h: TestHelper) =>
    let params =
      PropertyParams(where
        max_filter_discard_ratio' = 0,
        max_choice_sequence_size' = 0,
        max_sample_nanos' = 0)
    let count =
      _HealthCheckReporter.report(
        params, 10, 9999, 1, 9999, 999_999_999_999, h)
    h.assert_eq[USize](0, count)

class \nodoc\ iso _HealthCheckMultipleWarningsTest is UnitTest
  fun name(): String => "health_check/multiple_warnings"

  fun apply(h: TestHelper) =>
    let params =
      PropertyParams(where
        max_filter_discard_ratio' = 1.0,
        max_choice_sequence_size' = 10,
        max_sample_nanos' = 1_000_000)
    let count =
      _HealthCheckReporter.report(
        params, 10, 200, 10, 50, 5_000_000_000, h)
    h.assert_eq[USize](3, count)

// --- Async action protocol meta-tests ---
class \nodoc\ iso _AsyncCompleteProperty is Property[U8]
  fun name(): String => "meta/async_complete/property"
  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 5, seed' = 1,
      regression_db' = false)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.long_test(10_000_000_000)
    h.complete(true)

class \nodoc\ iso _AsyncCompleteTest is UnitTest
  fun name(): String => "async_action/complete"

  fun apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test.property(_AsyncCompleteProperty)
      end
    let collector = _PassCollector(h)
    let env =
      Env.create(
        h.env.root,
        h.env.input,
        collector,
        collector,
        recover val ["test"] end,
        h.env.vars,
        {(code: I32) => None })
    PonyTest(env, list)

class \nodoc\ iso _AsyncCompleteFalseProperty is Property[U8]
  fun name(): String => "meta/async_complete_false/property"
  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 5, seed' = 1,
      regression_db' = false)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.long_test(10_000_000_000)
    h.complete(false)

class \nodoc\ iso _AsyncCompleteFalseTest is UnitTest
  fun name(): String => "async_action/complete_false"

  fun apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test.property(_AsyncCompleteFalseProperty)
      end
    let detector = _FailDetector(h)
    let env =
      Env.create(
        h.env.root,
        h.env.input,
        detector,
        detector,
        recover val ["test"] end,
        h.env.vars,
        {(code: I32) => detector.done() })
    PonyTest(env, list)

class \nodoc\ iso _AsyncFailProperty is Property[U8]
  fun name(): String => "meta/async_fail/property"
  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 5, seed' = 1,
      regression_db' = false)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.long_test(10_000_000_000)
    h.fail("Oh noes!")

class \nodoc\ iso _AsyncFailTest is UnitTest
  fun name(): String => "async_action/fail"

  fun apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test.property(_AsyncFailProperty)
      end
    let detector = _FailDetector(h)
    let env =
      Env.create(
        h.env.root,
        h.env.input,
        detector,
        detector,
        recover val ["test"] end,
        h.env.vars,
        {(code: I32) => detector.done() })
    PonyTest(env, list)

class \nodoc\ iso _AsyncExpectCompleteProperty is Property[U8]
  fun name(): String => "meta/async_expect_complete/property"
  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 5, seed' = 1,
      regression_db' = false)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.long_test(10_000_000_000)
    h.expect_action("action")
    h.complete_action("action")

class \nodoc\ iso _AsyncExpectCompleteTest is UnitTest
  fun name(): String => "async_action/expect_complete"

  fun apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test.property(_AsyncExpectCompleteProperty)
      end
    let collector = _PassCollector(h)
    let env =
      Env.create(
        h.env.root,
        h.env.input,
        collector,
        collector,
        recover val ["test"] end,
        h.env.vars,
        {(code: I32) => None })
    PonyTest(env, list)

class \nodoc\ iso _AsyncExpectFailProperty is Property[U8]
  fun name(): String => "meta/async_expect_fail/property"
  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 5, seed' = 1,
      regression_db' = false)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.long_test(10_000_000_000)
    h.expect_action("action")
    h.fail_action("action")

class \nodoc\ iso _AsyncExpectFailTest is UnitTest
  fun name(): String => "async_action/expect_fail"

  fun apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test.property(_AsyncExpectFailProperty)
      end
    let detector = _FailDetector(h)
    let env =
      Env.create(
        h.env.root,
        h.env.input,
        detector,
        detector,
        recover val ["test"] end,
        h.env.vars,
        {(code: I32) => detector.done() })
    PonyTest(env, list)

class \nodoc\ iso _AsyncFailThenCompleteProperty is Property[U8]
  fun name(): String => "meta/async_fail_then_complete/property"
  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 5, seed' = 1,
      regression_db' = false)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.long_test(10_000_000_000)
    h.expect_action("only-once")
    h.fail_action("only-once")
    h.complete_action("only-once")

class \nodoc\ iso _AsyncFailThenCompleteTest is UnitTest
  fun name(): String => "async_action/fail_then_complete"

  fun apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test.property(_AsyncFailThenCompleteProperty)
      end
    let detector = _FailDetector(h)
    let env =
      Env.create(
        h.env.root,
        h.env.input,
        detector,
        detector,
        recover val ["test"] end,
        h.env.vars,
        {(code: I32) => detector.done() })
    PonyTest(env, list)

class \nodoc\ iso _AsyncCompleteThenFailProperty is Property[U8]
  fun name(): String => "meta/async_complete_then_fail/property"
  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 5, seed' = 1,
      regression_db' = false)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.long_test(10_000_000_000)
    h.expect_action("succeed-once")
    h.complete_action("succeed-once")
    h.fail_action("succeed-once")

class \nodoc\ iso _AsyncCompleteThenFailTest is UnitTest
  fun name(): String => "async_action/complete_then_fail"

  fun apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test.property(_AsyncCompleteThenFailProperty)
      end
    let detector = _FailDetector(h)
    let env =
      Env.create(
        h.env.root,
        h.env.input,
        detector,
        detector,
        recover val ["test"] end,
        h.env.vars,
        {(code: I32) => detector.done() })
    PonyTest(env, list)

// --- Classification shrink contamination unit test ---
class \nodoc\ iso _ClassifyShrinkContaminationTest is UnitTest
  fun name(): String => "classify/shrink_no_contamination"

  fun apply(h: TestHelper) =>
    let engine =
      _GenerationEngine(
        PropertyParams(where num_samples' = 10, seed' = 1,
          regression_db' = false),
        "test/shrink_contamination",
        h.env)

    engine.classify("run")
    engine.classify("run")
    engine.classify("run")
    h.assert_eq[USize](3, engine._label_count("run"))

    engine.classify("run")
    engine.classify("run")
    h.assert_eq[USize](5, engine._label_count("run"))

    h.assert_eq[USize](0, engine._label_count("other"))

// --- Regression save-on-fail meta-tests ---
class \nodoc\ iso _RegressionSaveProperty is Property[U8]
  fun name(): String => "meta/regression_save/property"

  fun gen(): Generator[U8] => Generators.u8(0, 10)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 42)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.assert_true(sample == 0)

class \nodoc\ iso _RegressionSaveOnFailTest is UnitTest
  fun name(): String => "regression/integration/save_on_fail"

  fun apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let nanos = Time.nanos().string()
    let dir_name: String val =
      ".ponytest-save-test-" + consume nanos
    let regression_dir =
      FilePath(FileAuth(h.env.root), dir_name)
    let expected_filename: String val =
      _RegressionDb._encode_name(
        "meta/regression_save/property") + ".choices"
    let expected_path =
      try
        regression_dir.join(expected_filename)?
      else
        h.fail("could not join regression path")
        return
      end

    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test.property(_RegressionSaveProperty)
      end
    let collector =
      _RegressionFileCollector(h, expected_path, regression_dir)
    let db_var: String val =
      "PONYTEST_DB_DIR=" + dir_name
    let vars: Array[String] val = [db_var]
    let env =
      Env.create(
        h.env.root,
        h.env.input,
        collector,
        collector,
        recover val ["test"] end,
        vars,
        {(code: I32) => collector.done() })
    PonyTest(env, list)

class \nodoc\ iso _StatefulRegressionSaveProperty
  is StatefulProperty[USize, USize, _Inc]
  fun name(): String => "meta/stateful_regression_save/property"

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 42)

  fun max_steps(): USize => 5

  fun initial_sut(): USize => 0

  fun initial_model(): USize => 0

  fun ref step(
    ctx: StatefulContext[USize, USize],
    rnd: Randomness,
    h: PropertyHelper)
    : StepResult[_Inc]
  =>
    ctx.sut = ctx.sut + 1
    ctx.model = ctx.model + 2
    _Inc

  fun invariant(
    ctx: StatefulContext[USize, USize] box, h: PropertyHelper): Bool
  =>
    h.assert_eq[USize](ctx.model, ctx.sut)

class \nodoc\ iso _StatefulRegressionSaveOnFailTest is UnitTest
  fun name(): String => "regression/integration/stateful_save_on_fail"

  fun apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let nanos = Time.nanos().string()
    let dir_name: String val =
      ".ponytest-stateful-save-test-" + consume nanos
    let regression_dir =
      FilePath(FileAuth(h.env.root), dir_name)
    let expected_filename: String val =
      _RegressionDb._encode_name(
        "meta/stateful_regression_save/property") + ".choices"
    let expected_path =
      try
        regression_dir.join(expected_filename)?
      else
        h.fail("could not join regression path")
        return
      end

    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test.stateful_property(
            _StatefulRegressionSaveProperty)
      end
    let collector =
      _RegressionFileCollector(h, expected_path, regression_dir)
    let db_var: String val =
      "PONYTEST_DB_DIR=" + dir_name
    let vars: Array[String] val = [db_var]
    let env =
      Env.create(
        h.env.root,
        h.env.input,
        collector,
        collector,
        recover val ["test"] end,
        vars,
        {(code: I32) => collector.done() })
    PonyTest(env, list)

class \nodoc\ iso _ClassifyReplayProperty is Property[U8]
  fun name(): String => "meta/classify_replay/property"

  fun gen(): Generator[U8] => Generators.u8(0, 255)

  fun params(): PropertyParams =>
    PropertyParams(where num_samples' = 10, seed' = 99)

  fun ref property(sample: U8, h: PropertyHelper) =>
    h.classify("all")

class \nodoc\ iso _ClassifyRegressionReplayTest is UnitTest
  """
  Verifies that classification calls during regression replay do not
  contaminate the distribution report.
  """
  fun name(): String => "classify/regression_replay_no_contamination"

  fun apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let nanos = Time.nanos().string()
    let dir_name: String val =
      ".ponytest-classify-replay-" + consume nanos
    let regression_dir =
      FilePath(FileAuth(h.env.root), dir_name)
    if not regression_dir.mkdir() then
      h.fail("could not create regression directory")
      return
    end

    let choices: Array[_Choice val] val =
      [_IntChoice(42, 0, 255, 0)]
    _RegressionDb.save(
      regression_dir,
      "meta/classify_replay/property",
      choices,
      h)

    let list =
      object tag is TestList
        fun tag tests(test: PonyTest) =>
          test.property(_ClassifyReplayProperty)
      end
    let collector =
      _ClassifyOutputCollector(h, regression_dir)
    let db_var: String val =
      "PONYTEST_DB_DIR=" + dir_name
    let vars: Array[String] val = [db_var]
    let env =
      Env.create(
        h.env.root,
        h.env.input,
        collector,
        collector,
        recover val ["test"; "--verbose"] end,
        vars,
        {(code: I32) => collector.done() })
    PonyTest(env, list)

actor \nodoc\ _ClassifyOutputCollector is OutStream
  let _h: TestHelper
  let _regression_dir: FilePath
  embed _lines: Array[String] = Array[String]
  var _done: Bool = false

  new create(h: TestHelper, regression_dir: FilePath) =>
    _h = h
    _regression_dir = regression_dir

  fun ref _push(data: ByteSeq) =>
    match \exhaustive\ data
    | let s: String =>
      _lines.push(s)
      if s.contains("test") and s.contains("ran.") then
        _check_results()
      end
    | let a: Array[U8] val =>
      let s = String.from_array(a)
      _lines.push(s)
      if s.contains("test") and s.contains("ran.") then
        _check_results()
      end
    end

  be print(data: ByteSeq) => _push(data)
  be write(data: ByteSeq) => _push(data)

  be printv(data: ByteSeqIter) =>
    for d in data.values() do _push(d) end

  be writev(data: ByteSeqIter) =>
    for d in data.values() do _push(d) end

  be flush() => None

  be done() => _check_results()

  fun ref _check_results() =>
    if _done then return end
    _done = true

    var found_classification = false
    for line in _lines.values() do
      if line.contains("% all (") then
        found_classification = true
        if not line.contains("100.0% all (10/10)") then
          _h.fail(
            "classification contaminated by regression replay: " +
              line)
        end
      end
    end
    if not found_classification then
      _h.fail("classification output not found in test log")
    end

    try
      let filename: String val =
        _RegressionDb._encode_name(
          "meta/classify_replay/property") + ".choices"
      _regression_dir.join(filename)?.remove()
    end
    _regression_dir.remove()

    _h.complete(found_classification)

actor \nodoc\ _RegressionFileCollector is OutStream
  let _h: TestHelper
  let _expected_path: FilePath
  let _regression_dir: FilePath

  new create(
    h: TestHelper,
    expected_path: FilePath,
    regression_dir: FilePath)
  =>
    _h = h
    _expected_path = expected_path
    _regression_dir = regression_dir

  be print(data: ByteSeq) => None
  be write(data: ByteSeq) => None
  be printv(data: ByteSeqIter) => None
  be writev(data: ByteSeqIter) => None
  be flush() => None

  be done() =>
    _h.assert_true(
      _expected_path.exists(),
      "regression file should exist after failure")
    _expected_path.remove()
    _regression_dir.remove()
    _h.complete(true)
