use "pony_check"

primitive Blue is Stringable
  """
  A pony color.
  """
  fun string(): String iso^ => "blue".clone()

primitive Green is Stringable
  """
  A pony color.
  """
  fun string(): String iso^ => "green".clone()

primitive Pink is Stringable
  """
  A pony color.
  """
  fun string(): String iso^ => "pink".clone()

primitive Rose is Stringable
  """
  A pony color.
  """
  fun string(): String iso^ => "rose".clone()

type Color is ( Blue | Green | Pink | Rose )

class MyLittlePony is Stringable
  """
  A pony with a name, cuteness score, and color.
  """

  let name: String
  let cuteness: U64
  let color: Color

  new create(
    name': String,
    cuteness': U64 = U64.max_value(),
    color': Color)
  =>
    name = name'
    cuteness = cuteness'
    color = color'

  fun is_cute(): Bool =>
    (cuteness > 10) or (color is Pink)

  fun string(): String iso^ =>
    recover
      String(17 + name.size())
        .> append(
          "Pony(\"" + name + "\", " +
            cuteness.string() + ", " +
            color.string() + ")")
    end

class _CustomClassMapProperty is Property1[MyLittlePony]
  """
  The go-to approach for creating custom classes are the
  `Generators.map2`, `Generators.map3` and `Generators.map4`
  combinators and of course the `map` method on `Generator`
  itself (for single argument constructors).
  """
  fun name(): String => "custom_class/map"

  fun gen(): Generator[MyLittlePony] =>
    let name_gen = Generators.ascii_letters(5, 10)
    let cuteness_gen = Generators.u64(11, 100)
    let color_gen =
      Generators.one_of[Color](
        [Blue; Green; Pink; Rose])
    Generators.map3[String, U64, Color, MyLittlePony](
      name_gen,
      cuteness_gen,
      color_gen,
      {(name, cuteness, color) =>
        MyLittlePony(name, cuteness, color)
      })

  fun ref property(pony: MyLittlePony, ph: PropertyHelper) =>
    ph.assert_true(pony.is_cute())

class _CustomClassFlatMapProperty is Property1[MyLittlePony]
  """
  It is possible to create a generator using `flat_map` on a
  source generator, creating a new Generator in the `flat_map`
  function. This way it is possible to combine multiple
  generators into a single one that is based on multiple
  generators, one for each constructor argument.

  The nested `flat_map` syntax is a little bit cumbersome
  (e.g. the captured variables from the surrounding scope need
  to be provided explicitly). Prefer `Generators.map2`/`map3`
  when possible.
  """
  fun name(): String => "custom_class/flat_map"

  fun gen(): Generator[MyLittlePony] =>
    let name_gen = Generators.ascii_letters(5, 10)
    let cuteness_gen = Generators.u64(11, 100)
    let color_gen =
      Generators.one_of[Color](
        [Blue; Green; Pink; Rose])
    color_gen
      .flat_map[MyLittlePony](
        {(color: Color)(cuteness_gen, name_gen) =>
          name_gen
            .flat_map[MyLittlePony](
              {(name: String)(color, cuteness_gen) =>
                cuteness_gen
                  .map[MyLittlePony](
                    {(cuteness: U64)(color, name) =>
                      MyLittlePony.create(
                        name, cuteness, color)
                    })
              })
        })

  fun ref property(pony: MyLittlePony, ph: PropertyHelper) =>
    ph.assert_true(pony.is_cute())

class _CustomClassCustomGeneratorProperty
  is Property1[MyLittlePony]
  """
  Generating your class given a custom generator using a
  GenObj. The generate method draws from Randomness to build
  each field. Shrinking is automatic.
  """

  fun name(): String => "custom_class/custom_generator"

  fun gen(): Generator[MyLittlePony] =>
    Generator[MyLittlePony](
      object is GenObj[MyLittlePony]
        let name_gen: Generator[String] =
          Generators.ascii_printable(5, 10)
        let cuteness_gen: Generator[U64] =
          Generators.u64(11, 100)
        let color_gen: Generator[Color] =
          Generators.one_of[Color](
            [Blue; Green; Pink; Rose])

        fun generate(rnd: Randomness): MyLittlePony^ ? =>
          let name' = name_gen.generate(rnd)?
          let cuteness' = cuteness_gen.generate(rnd)?
          let color' = color_gen.generate(rnd)?
          MyLittlePony(
            consume name',
            consume cuteness',
            consume color')
      end)

  fun ref property(pony: MyLittlePony, ph: PropertyHelper) =>
    ph.assert_true(pony.is_cute())
