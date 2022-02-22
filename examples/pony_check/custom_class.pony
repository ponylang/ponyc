use "itertools"
use "pony_check"

primitive Blue is Stringable
  fun string(): String iso^ => "blue".clone()
primitive Green is Stringable
  fun string(): String iso^ => "green".clone()
primitive Pink is Stringable
  fun string(): String iso^ => "pink".clone()
primitive Rose is Stringable
  fun string(): String iso^ => "rose".clone()

type Color is ( Blue | Green | Pink | Rose )

class MyLittlePony is Stringable

  let name: String
  let cuteness: U64
  let color: Color

  new create(name': String, cuteness': U64 = U64.max_value(), color': Color) =>
    name = name'
    cuteness = cuteness'
    color = color'

  fun is_cute(): Bool =>
    (cuteness > 10) or (color is Pink)

  fun string(): String iso^ =>
    recover
      String(17 + name.size()).>append("Pony(\"" + name + "\", " + cuteness.string() + ", " + color.string() + ")")
    end

class _CustomClassMapProperty is Property1[MyLittlePony]
  """
  The go-to approach for creating custom classes are the
  `Generators.map2`, `Generators.map3` and `Generators.map4` combinators and
  of course the `map` method on `Generator` itself (for single argument
  constructors).

  Generators created like this have better shrinking support
  and their creation is much more readable than the `flat_map` solution below.
  """
  fun name(): String => "custom_class/map"

  fun gen(): Generator[MyLittlePony] =>
    let name_gen = Generators.ascii_letters(5, 10)
    let cuteness_gen = Generators.u64(11, 100)
    let color_gen = Generators.one_of[Color]([Blue; Green; Pink; Rose] where do_shrink=true)
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
  It is possible to create a generator using `flat_map` on a source
  generator, creating a new Generator in the `flat_map` function. This way
  it is possible to combine multiple generators into a single one that is based on
  multiple generators, one for each constructor argument.

  ### Drawbacks

  * the nested `flat_map` syntax is a little bit cumbersome (e.g. the captured
    from the surrounding scope need to be provided explicitly)
  * the resuling generator has only limited shrinking support.
    Only on the innermost created generator in the last `flat_map` function
    will be properly shrunken.
  """
  fun name(): String => "custom_class/flat_map"

  fun gen(): Generator[MyLittlePony] =>
    let name_gen = Generators.ascii_letters(5, 10)
    let cuteness_gen = Generators.u64(11, 100)
    let color_gen =
      Generators.one_of[Color]([Blue; Green; Pink; Rose] where do_shrink=true)
    color_gen.flat_map[MyLittlePony]({(color: Color)(cuteness_gen, name_gen) =>
      name_gen.flat_map[MyLittlePony]({(name: String)(color, cuteness_gen) =>
        cuteness_gen
          .map[MyLittlePony]({(cuteness: U64)(color, name) =>
            MyLittlePony.create(name, cuteness, color)
          })
      })
    })

  fun ref property(pony: MyLittlePony, ph: PropertyHelper) =>
    ph.assert_true(pony.is_cute())

class _CustomClassCustomGeneratorProperty is Property1[MyLittlePony]
  """
  Generating your class given a custom generator is the most flexible
  but also the most complicated approach.

  You need to understand the types `GenerateResult[T]` and `ValueAndShrink[T]`
  and how a basic `Generator` works.

  You basically have two options on how to implement a Generator:
  * Return only the generated value from `generate` (and optionally implement
    the `shrink` method to return an `(T^, Iterator[T^])` whose values need to
    meet the Generator's requirements
  * Return both the generated value and the shrink-Iterator from `generate`.
    this way you have the values from any Generators available your Generator
    is based upon.

  This Property is presenting the second option, returning a `ValueAndShrink[MyLittlePony]`
  from `generate`.
  """

  fun name(): String => "custom_class/custom_generator"

  fun gen(): Generator[MyLittlePony] =>
    Generator[MyLittlePony](
      object is GenObj[MyLittlePony]
        let name_gen: Generator[String] = Generators.ascii_printable(5, 10)
        let cuteness_gen: Generator[U64] = Generators.u64(11, 100)
        let color_gen: Generator[Color] =
          Generators.one_of[Color]([Blue; Green; Pink; Rose] where do_shrink=true)

        fun generate(rnd: Randomness): GenerateResult[MyLittlePony] ? =>
          (let name, let name_shrinks) = name_gen.generate_and_shrink(rnd)?
          (let cuteness, let cuteness_shrinks) =
            cuteness_gen.generate_and_shrink(rnd)?
          (let color, let color_shrinks) = color_gen.generate_and_shrink(rnd)?
          let res = MyLittlePony(consume name, consume cuteness, consume color)
          let shrinks =
            Iter[String^](name_shrinks)
              .zip2[U64^, Color^](cuteness_shrinks, color_shrinks)
              .map[MyLittlePony^]({(zipped) =>
                (let n: String, let cute: U64, let col: Color) = consume zipped
                MyLittlePony(consume n, consume cute, consume col)
              })
          (consume res, shrinks)
      end
      )

  fun ref property(pony: MyLittlePony, ph: PropertyHelper) =>
    ph.assert_true(pony.is_cute())

