use "pony_test"


class _ListReverseProperty is Property[Array[USize]]
  fun name(): String => "list/reverse"

  fun gen(): Generator[Array[USize]] =>
    Generators.seq_of[USize, Array[USize]](Generators.usize())

  fun ref property(arg1: Array[USize], ph: TestHelper) =>
    ph.assert_array_eq[USize](arg1, arg1.reverse().reverse())

class _ListReverseOneProperty is Property[Array[USize]]
  fun name(): String => "list/reverse/one"

  fun gen(): Generator[Array[USize]] =>
    Generators.seq_of[USize, Array[USize]](
      Generators.usize()
      where from = 1, to = 1)

  fun ref property(arg1: Array[USize], ph: TestHelper) =>
    ph.assert_eq[USize](arg1.size(), 1)
    ph.assert_array_eq[USize](arg1, arg1.reverse())

class _ListReverseMultipleProperties is UnitTest
  fun name(): String => "list/properties"

  fun apply(h: TestHelper) ? =>
    let g = Generators

    let gen1 = recover val g.seq_of[USize, Array[USize]](g.usize()) end
    h.for_all[Array[USize]](gen1)(
      {(arg1, ph) =>
        ph.assert_array_eq[USize](arg1, arg1.reverse().reverse())
      })?

    let gen2 = recover val g.seq_of[USize, Array[USize]](g.usize(), 1, 1) end
    h.for_all[Array[USize]](gen2)(
      {(arg1, ph) =>
        ph.assert_array_eq[USize](arg1, arg1.reverse())
      })?

